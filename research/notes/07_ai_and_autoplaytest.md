# fheroes2 AI behavior & Auto-playtest (upstream b086d1aa, 2026-09-01)

Source root: `C:/Users/gjoer/source/repos/fheroes2`. All paths below are relative to `src/fheroes2/`.

---

## 1. Auto-playtest feature

### 1.1 What it is
An editor-only feature: the game plays the current FH2M map with **all players (human and computer colors alike) driven by the AI**, for N playthroughs, up to M days each, and reports win percentages per color. HEAD commit b086d1aa itself is "Disable 'Sound Effects' in the Auto-playtest window if animation is off (#10971)" and touches `game/game_auto_playtest.cpp`.

### 1.2 How it is launched
- **Editor only.** No main-menu path, no command line.
  - Editor "File" dialog button index 6 (`editor/editor_interface.cpp:2086` `buttonAutoPlaytest = optionButtons.button( 6 )`; click handler at `:2144-2150` calls `Get()._runAutoPlaytest()`; right-click help text "Run an automatic playtest of the map." at `:2167-2169`).
  - Editor hotkey: `Game::HotKeyEvent::EDITOR_AUTO_PLAYTEST` = **KEY_A** (`game/game_hotkeys.cpp:171-172`, category WORLD_MAP, name `hotkey|auto playtest`), handled in the editor main loop at `editor/editor_interface.cpp:1641-1643`.
- `EditorInterface::_runAutoPlaytest()` (`editor/editor_interface.cpp:3631-3670`):
  1. Calls `_prepareMapForGameplay()` (`:3599-3629`): if the map has never been saved it forces a save dialog; **rejects maps with `colorsAvailableForHumans == 0`** ("This map is not playable. You need at least one human player…", `:3622-3626`).
  2. Snapshots the in-editor map (`Maps::Map_Format::saveMap` into a `RWStreamBuf`) and `Maps::getLastObjectUID()`.
  3. Calls `fheroes2::openMapAutoPlayTest()`; afterwards restores the map from the snapshot and redraws the editor.

### 1.3 The configuration dialog (`fheroes2::openMapAutoPlayTest`, `game/game_auto_playtest.cpp:358-525`)
Window "Auto Playtest" with sliders/checkboxes bound to the singleton `fheroes2::AutoPlaytest` (`game/game_auto_playtest.h:33-201`):

| Setting | Range | Default | Field |
|---|---|---|---|
| Number of playthroughs | 1..100 (`playthroughLimit{100}`, h:51) | **1** (`_maxPlaythroughs{1}`, h:196) | slider |
| Max days per playthrough | 1..1000 (`dayLimit{1000}`, h:52) | **365** (`_maxDaysInPlaythrough{365}`, h:197) | slider |
| Animation | on/off | **on** (`_isAnimationEnabled{true}`, h:199) | checkbox |
| Animation speed | 1..10 (`animationLimit{10}`, h:53) | **10** (h:198) | slider (disabled when animation off) |
| Sound Effects | on/off | **on** (h:200) | checkbox, grayed & ignored when animation off (HEAD change) |

Yellow note in dialog: "Left-clicking at any point will interrupt the playtest." (cpp:438).

### 1.4 Run loop (`runPlayTest`, `game/game_auto_playtest.cpp:247-325`)
- `autoPlaytest.reset( conf.GetPlayers().GetColors() )` — one result row per player color.
- Saves current `conf.AIMoveSpeed()`; if animation enabled sets AI move speed = animation speed, else **`SetAIMoveSpeed(0)`** (= hide AI movements entirely; in `AI::HeroesMove`, `hideAIMovements = ( conf.AIMoveSpeed() == 0 )`, `ai/ai_hero_action.cpp:2278`).
- Per playthrough: `prepareMap()` (cpp:88-103) asserts `mapInfo.version == GameVersion::RESURRECTION` (**FH2M only**), re-runs `players.Init( mapInfo )` + `players.SetStartGame()`, and reloads the world via `world.loadResurrectionMap( mapInfo.filename )` — i.e. **every playthrough is a fresh load of the .fh2m file** (fresh RNG → different outcomes).
- Sets game type: `conf.SetGameType( Game::TYPE_AUTO_PLAYTEST )` (`game/game.h:48`: `TYPE_AUTO_PLAYTEST = 0x20`).
- `Game::StartGame()` runs a full standard game.
- Under `WITH_DEBUG` builds only, after each playthrough results are dumped with `VERBOSE_LOG` ("----- Playthrough N -----", "Player X won/lost on D day", cpp:276-297). **Release builds produce no textual output** — only the results dialog.
- If a playthrough was interrupted (user clicked), the loop breaks; `popLastResults()` drops the in-progress row.
- Results dialog (`displayResults`, cpp:105-245): per-color win % = `wins*100/playthroughCount`, a "N playthroughs" line and "X playthrough(s) reached the specified time limit" line. Playthroughs that hit the time limit are **excluded from win counting** (cpp:112-129).

### 1.5 How the engine behaves in TYPE_AUTO_PLAYTEST
- `Interface::AdventureMap::StartGame()` (`game/game_startgame.cpp:732-1005`):
  - `:771-775`: on day 1, **every player is set `SetControl(CONTROL_HUMAN)` + `setAIAutoControlMode(true)`** — i.e. all colors are "human players under AI auto-control". `Player::GetControl()` then returns `CONTROL_AI` (`system/players.cpp:127-135`), so every kingdom takes the `CONTROL_AI` branch and is played by `AI::Planner::Get().KingdomTurn( kingdom )` (`:938`).
  - `:784-788`: with animation off, game area is centered once and only fog is drawn (no per-turn rendering).
  - `:802-811`: at the start of each day, if `world.CountDay() > getMaxDaysInPlaythrough()` → `markTimeLimit()` and exit to MAIN_MENU (ends the playthrough).
  - `:948-951`: after a kingdom's AI turn, if the player revoked auto-control (interrupt confirmed) → exit to MAIN_MENU.
- Win/loss bookkeeping in `GameOver::Result::checkGameOver()` (`game/game_over.cpp:481-682`):
  - `:484-485`: in auto-playtest, `humanColors = conf.GetPlayers().GetColors()` (all colors count as "human"), so the multiplayer branch is used and per-color win/loss conditions are evaluated for everyone.
  - Vanquished color → `AutoPlaytest::instance().setDefeatedPlayer( color, world.CountDay() )` (`:500-502`, also `:614-616`); loss/win dialogs and videos are suppressed (`:654-677`).
  - `AutoPlaytest::PlayerState` (h:36-42): `WINNER, LOSER, TIME_LIMIT, INTERRUPTED`; `PlayerInfo { PlayerColor color; PlayerState state{WINNER}; uint32_t dayOfState{1}; }` (h:44-49). Everyone starts WINNER; losers get flipped; whoever remains WINNER at game end won.
- Interrupting: any left-click during AI hero movement (`ai/ai_hero_action.cpp:2287-2289`) or during AI turn events calls `fheroes2::interruptAutoPlaytest()` (cpp:533-577) which shows a YES/NO dialog "Do you want to interrupt the automatic playtest? The effect will take place only on the next turn.", then `setAIAutoControlMode(false)` + `autoPlaytest.interrupt( world.CountDay() )`.
- Misc suppressions: no battle result dialogs for playtest (`battle/battle_main.cpp:374`), status panel shows AI resources (`gui/interface_status.cpp:123`), fog reveal rendering only when animation on (`maps/maps.cpp:475`, `game/game_interface.cpp:136`), no autosave (`game_startgame.cpp:932,943`).

### 1.6 Unattended use / validating generated maps
- **Cannot be launched headless or from CLI.** `int main` (`game/fheroes2.cpp:56-112`) ignores argv except for `Game::initConfigDir( argv[0] )`; there is no argument parsing anywhere in `src/fheroes2` (grep for `argv[1]` only hits `src/tools/*`). No "load this map" flag, no exit-after-test.
- Practical workflow to validate a generated `.fh2m`: put it in the maps dir → open Editor → load map → press **A** (or File→Auto Playtest) → set playthroughs/days, disable Animation for max speed → OK → read the win-rate dialog. A `WITH_DEBUG` build additionally logs per-playthrough winners/losers and the full `DEBUG_LOG(DBG_AI, ...)` stream (AI decisions, funds, targets), which is the richest machine-readable signal.
- Requirements for the map: valid FH2M, at least one color in `humanPlayerColors` (editor gate `editor_interface.cpp:3622`), and `readResurrectionMap` also rejects maps with no human colors when loading for game (`maps/maps_fileinfo.cpp:409-413`).
- Difficulty during auto-playtest = `Settings::GameDifficulty()` (`game/game.cpp:75-91`), default **Difficulty::NORMAL** (`system/settings.cpp:97`); it is not persisted in the config file (only in save streams, settings.cpp:1254), so a fresh session runs playtests at Normal unless the user changed difficulty in a scenario dialog this session.

---

## 2. FH2M load: castle with no hero — is a hero auto-created?

### 2.1 Load path
`World::loadResurrectionMap` (`world/world_loadmap.cpp:711` ff): loads tiles, `Maps::updateMapPlayers`, then iterates tile objects:
- `KINGDOM_TOWNS` (`:792-844`): builds `Castle`, `SetColor`, `loadFromResurrectionMap( castleInfo )`, registers capture object.
- `KINGDOM_HEROES` (`:845-893`): for each hero object, `color = Color::IndexToColor( metadata[0] )`; RAND race resolved to kingdom race; **only hired if `kingdom.AllowRecruitHero( false )`** (max 8 heroes/kingdom: `kingdom/kingdom.cpp:499-502`, `GameStatic::GetKingdomMaxHeroes()` = **8**, `game/game_static.cpp:140-143`); `GetHeroForHire(race)` + `applyHeroMetadata`; hero on water tile → `SetShipMaster(true)`.
- Then `_processNewResurrectionMap` (`:1463-1490`): `vec_kingdoms.AddHeroes/AddCastles`, `setHeroIdsForMapConditions()`, `setUltimateArtifact()`, `PostLoad`, **`vec_kingdoms.ApplyPlayWithStartingHero()`** (`:1485`), `tryAddDebugHero()`.

### 2.2 `Kingdom::ApplyPlayWithStartingHero` (`kingdom/kingdom.cpp:504-557`)
- For each castle: if a hero of the same color stands on the tile **directly below the castle entrance** (`castle center + (0,1)`), he is moved into the castle (and trained by the mage guild); this sets `foundHeroes`.
- Fallback: `if ( !foundHeroes && Settings::Get().getCurrentMapInfo().startWithHeroInFirstCastle )` → recruit a free hero of the castle's race into the first castle (`:543-556`).
- **`startWithHeroInFirstCastle` is only ever set from the MP2 header** (`maps/maps_fileinfo.cpp:281`: `startWithHeroInFirstCastle = ( 0 == fs.get() )`). `FileInfo::Reset()` sets it `false` (`:177`) and `FileInfo::loadResurrectionMap` (`:418-...`) never sets it. ⇒ **For FH2M maps the flag is always false: a color that owns a castle but has no hero object on the map starts with NO hero — human or AI alike.** No auto-creation.
- `tryAddDebugHero` (`world/world_loadmap.cpp:1733-1756`) only fires in `IS_DEVEL()` builds (adds `Heroes::DEBUG_HERO` to the first human castle).

### 2.3 Does the AI recover from a hero-less start? Yes — it buys one on turn 1
`AI::Planner::KingdomTurn` (see §3) calls `purchaseNewHeroes` every iteration of its main loop (`ai/ai_planner_kingdom.cpp:860`). `purchaseNewHeroes` (`:929-987`):
- `isEarlyGameWithSingleCastle = world.CountDay() < 5 && sortedCastleList.size() == 1` → `heroLimit = 2`; otherwise `heroLimit = world.w() / Maps::SMALL + 2` (SMALL=36; e.g. 144-wide map → 6), floored to number of castles, capped at 8.
- Candidate castle must satisfy `castle->isCastle()` (**a tent-only town can never recruit** — `:955`), have no guest hero, not be in danger while other heroes exist, and its region must have ≤1 friendly hero.
- Recruit via `AI::Planner::recruitHero` (`:273-321`): picks the better of the two tavern `Recruits` by `getRecruitValue()`, skips heroes bound to WINS_HERO/LOSS_HERO conditions, pays **2500 gold** (`kingdom/payment.cpp:42-45`: `RecruitHero = {2500,0,0,0,0,0,0}`), then `reinforceCastle` (buy army) unless early-game-single-castle.
- Starting resources cover this: at Normal both AI and human kingdoms start with **7500 gold, 20 wood, 5 mercury, 20 ore, 5 sulfur, 5 crystal, 5 gems** (`kingdom/kingdom.cpp:865-895`; AI Easy/Normal = {7500,20,5,20,5,5,5}, AI Hard+ = {10000,30,10,30,10,10,10}; humans: Easy {10000,30,10,30,10,10,10}, Normal {7500,...}, Hard {5000,10,2,10,2,2,2}, Expert {2500,5,0,5,0,0,0}, Impossible all-0).
- **Map-design caveat:** the AI only recruits from a built castle (`isCastle()`). A color owning only a town (tent) and no hero can still build the castle (BUILD_CASTLE is first in every AI build order, §5) if resources allow, then recruit next turn — but a poor town-only start can strand the AI. A human player with a town-only, hero-less start simply has nothing to move on day 1.
- Human "castle but no hero" start is legal for the engine; game-over only triggers when `!kingdom.isPlay()` (no heroes AND no castles) or per map loss conditions.

---

## 3. AI kingdom turn (`AI::Planner::KingdomTurn`, `ai/ai_planner_kingdom.cpp:623-927`)

Turn skeleton:
1. Clear per-turn caches (`_mapActionObjects`, `_priorityTargets`, `_enemyArmies`, `_tileArmyStrengthValues`, `_regions`).
2. Optional **View All** cast by the best-suited hero (`:695-712`).
3. **Map scan** (`:716-782`): iterates all tiles; skips tiles under fog for this color unless View All was cast (`:725-727` — the AI reads only *revealed* tiles; there is no difficulty-based map vision cheat). Valuable action objects go into `_mapActionObjects`; heroes/castles per region counted; every enemy hero/castle → `_enemyArmies` (`getEnemyArmyOnTile`, `:218-270`: enemy hero threat = own army strength + castle garrison if garrisoned; enemy castle threat = garrison strength with assumed 1500 move points; towns that cannot build a castle are ignored; stationary PATROL-0 heroes ignored).
4. `evaluateRegionSafety()` (`:323-392`): region safetyFactor seeded −100 (enemy castles), −50 (contested/invaded), +100 (own castles), diffused to neighbors; islands ×1.5.
5. `updateKingdomBudget` (see §5).
6. Ultimate-Artifact dig if a hero is standing on it (`:794-818`).
7. Main loop (`:823-891`): give army to heroes standing in castles; `setHeroRoles` (§4.1); `findCastlesInDanger` (§3.1); `HeroesTurn` (§4.3); `purchaseNewHeroes` (§2.3) — loop repeats after a successful recruit; on `world.LastDay()` idle heroes are sent to garrison castles (`PriorityTaskType::REINFORCE`).
8. Castle development for each castle sorted by danger/safety (`CastleTurn`, §5).
9. Heroes in castles move slowest troops to garrison for next-day movement bonus (`:913-922`).

### 3.1 Castle danger (`findCastlesInDanger` / `updateIndividualPriorityForCastle`, `:431-564`)
- Enemy paths computed with **own heroes temporarily removed** and "optimistic" pathfinder (`ARMY_ADVANTAGE_DESPERATE`, 0 spell reserve).
- `threatDistanceLimit = 3000` movement points (~30 tiles) (`:505`); enemy ignored if `daysToReach > 3`; projected enemy strength **halves per extra day** of distance (`:536-540`).
- Castle "in danger" if `castle.GetGarrisonStrength(enemyHero) < enemyStrength` or guest hero weaker than `enemyStrength * ARMY_ADVANTAGE_SMALL` (`:552-561`). Creates paired priority tasks: ATTACK (enemy tile) + DEFEND (castle tile).

---

## 4. AI heroes

### 4.1 Roles (`setHeroRoles`, `ai/ai_planner_kingdom.cpp:130-216`)
- Easy difficulty: roles disabled, all HUNTER (`Difficulty::areAIHeroRolesAllowed`, `game/difficulty.cpp:195-212`).
- PATROL heroes → FIGHTER. >3 heroes: best stats → CHAMPION, worst → COURIER, next worst → SCOUT; remainder: strength > 3× median → FIGHTER else HUNTER. Hero targeted by a WINS_HERO condition is always CHAMPION.

### 4.2 Strength-ratio constants (`ai/ai_planner_internals.h:27-30`)
```cpp
const double ARMY_ADVANTAGE_DESPERATE = 0.8;
const double ARMY_ADVANTAGE_SMALL     = 1.3;
const double ARMY_ADVANTAGE_MEDIUM    = 1.5;
const double ARMY_ADVANTAGE_LARGE     = 1.8;
```
Attack-decision thresholds (`isHeroStrongerThan`: heroArmyStrength > tileArmyStrength × multiplier, `ai/ai_planner_hero.cpp:261-264`; tile strength via cached `Army::setFromTile` — `ai/ai_planner.cpp:74-86`):
| Target | Threshold (× target strength) | Cite (ai_planner_hero.cpp) |
|---|---|---|
| **Neutral wandering monster** | ×1.5 (MEDIUM); ×1.0 if AI is losing | :693 |
| Enemy hero in open field | ×1.3 (SMALL); ×0.8 if losing | :722 |
| Enemy castle / hero in castle | garrison × 1.5 (MEDIUM); ×0.8 if losing (`AIShouldVisitCastle`) | :255-258 |
| Guarded mine/sawmill/alchemist | ×1.3 (SMALL) | :360-362 |
| Abandoned mine | ×1.8 (LARGE) | :369 |
| Artifact with fight condition | ×1.8 (LARGE) | :402-403 |
| Derelict ship / graveyard / shipwreck | ×2.0 | :673 |
| Pyramid | ×1.8 + Expert Wisdom required | :680-681 |
| Daemon cave | ×1.5 | :688 |
| City of Dead / Dragon City / Troll Bridge (unowned) | ×1.5 | :634 |
- "Losing game" = `hero.isLosingGame()` (kingdom in LOSS state).
- Monster **join** rules: `canMonsterJoinHero` (`ai/ai_hero_action.cpp:231-248`) — join accepted if hero already has that monster, else troop strength must exceed `hero.getAIMinimumJoiningArmyStrength()`; that threshold (`heroes/heroes.cpp:2226-2255`) = fraction of hero army strength by role: SCOUT 0.01, COURIER 0.015, HUNTER 0.02, FIGHTER 0.025, CHAMPION 0.03 (default 0.05). Same threshold gates whether free/paid dwellings are "valuable" (`isArmyValuableToObtain`, halved if army already has the monster, `ai_planner_hero.cpp:266-275`).
- Monster fight flow `AIToMonster` (`ai/ai_hero_action.cpp:596-696`): uses `Army::GetJoinSolution` (Alliance/Bane/Free/ForMoney/RunAway); AI never "declines then leaves" — if not joining/fleeing it fights.

### 4.3 Hero movement planning (`HeroesTurn`, `ai/ai_planner_hero.cpp:3078-3293`)
- Iterative best-(hero,target) selection over all available heroes; pathfinder tried with escalating aggressiveness (`:3120-3123`):
  `{ADVANTAGE_LARGE, SP-reserve 0.5} → {MEDIUM, 0.25} → {SMALL, 0.0}`; when losing: single config `{DESPERATE, 0.0}`.
- If nobody has a target: assumes a hero **blocks the way** — shuffles heroes and calls `_pathfinder.getNearestTileToMove` on any hero that `isHeroPossiblyBlockingWay` (`:3159-3189`) — relevant for narrow-corridor maps: blockers get nudged.
- Dimension Door used when `dimensionDoorDist < regularDist / 2` (`shouldUseDimensionDoor`, `:202-209`); per-turn DD casts limited by difficulty: Easy 1, Normal 2, Hard 3, Expert+ unlimited (`Difficulty::GetDimensionDoorLimitForAI`, `game/difficulty.cpp:179-193`).

### 4.4 Object valuation (`getGeneralObjectValue`, `ai/ai_planner_hero.cpp:1133+`; role variants exist e.g. `getFighterObjectValue` :1769)
Key numbers (value units ≈ gold; "1 tile distance ≈ 100.0 value", comment :1136):
- **Castle**: `buildingValue*150 + 3000`; +15000 if losing; +20000 if it's a human loss-condition town; ×1.25 if defenseless or own; threatened-castle logic can double values (`calculateCastleValue`, :1140-1193).
- **Enemy hero**: base 5000; + castle value if garrisoned; ×0.8 vs other AI heroes ("AI heroes should not attack other AI heroes so aggressively", :1306-1310).
- **Monster tile**: `1000 + totalHP/100` (:1315-1325).
- **Mines** (incl. sawmill/alchemist): `dailyIncome × getResourcePriorityModifier(res, isMine=true)`; mine modifiers normalize by gold-mine income ×2 days (`ai/ai_planner.cpp:88-157`; pile priorities: gold 1, wood/ore 125, rare 250; budget-priority resources ×2, recurring-cost ×1.5).
- Artifact: `1000 × artifactValue` (3000× if victory-condition artifact) (:1351-1357). Treasure chest est. 1500 gold; campfire {1 each resource +400 gold}; Daemon Cave 2500 gold; Xanadu 3000; skill-giving one-timers 500.
- Distance scaling `scaleWithDistanceAndTime` (:993-1009): `value − corrected*log10(corrected)`, with per-object distance modifiers (castle 0.8, mine/artifact/hero 0.9, pickups 0.95, morale/luck 1.1 — :952-991).
- **Fog discovery**: base value −10000 (`fogDiscoveryBaseValue`, :950); SCOUT 0, COURIER −20000 (`getFogDiscoveryValue` :1011-1030) — i.e. exploration is a last resort except for scouts; intensifies after N days without discovery (SCOUT 30, FIGHTER/CHAMPION 60, HUNTER 90, COURIER 120; :1110-1129).
- `dangerousTaskPenalty = 50000.0` (:949); tiles reachable by a stronger enemy hero within a turn get cumulative penalties `50000 × (2 − dist/threshold)` (`enemyThreatPenalties`, :2413-2491; enemy counts as threat only if `heroStrength × 1.3 < enemyStrength`, :2431).

### 4.5 AI pathfinder constraints (map-design relevant)
`world/world_pathfinding.{h,cpp}`:
- Defaults: `_minimalArmyStrengthAdvantage{1.0}`, `_spellPointsReserveRatio{0.5}` (h:272,276).
- `isTileAccessibleForAIWithArmy` (cpp:293-303): a tile whose **protecting monsters** (adjacent monster tiles, `Maps::getMonstersProtectingTile`) are stronger than `armyStrength / minimalAdvantage` is **treated as impassable** by the AI pathfinder (`Maps::isTileProtectionStrongerThan`, `maps/maps.cpp:644-662`). Monster tiles themselves stay "accessible" so high-level logic can evaluate attacking them.
- Walk-through rules (`isTileAvailableForWalkThroughForAIWithArmy`, cpp:74-218): enemy heroes passable only if `enemyStrength × advantage ≤ ownStrength` (:125); heroes on water can't be walked through from land; monsters passable under same strength rule (:183-185); artifacts with purchase/skill conditions block walk-through (:166-172); barriers passable only with tent visited (:188-190); castles only own-color (:206-208).
- Consequence for **corridor + guard** design: a neutral stack guarding a chokepoint fully walls off AI heroes whose army strength is below `guardStrength × advantage-config` (1.8/1.5/1.3 tried in that order per §4.3, or 0.8 when desperate). Because `findCastlesInDanger` also uses these paths with 0.8, guards also shield castles from being "in danger".
- Monster-protected tiles force movement toward one of the protecting monsters (PlayerWorldPathfinder, cpp:510-535 — human path preview; AI analog via accessibility rules).
- Region logic (`world_regions`): castles on islands/peninsulas (`neighbourRegions < 3`) push the AI to buy Shipyard/boats (`ai/ai_planner_castle.cpp:179-242`).

---

## 5. AI castle development vs army (`ai/ai_planner_castle.cpp`)

- `CastleTurn( castle, defensiveStrategy )` (:516-536): if under threat → `reinforceCastle` (hire max troops so the enemy can't get them; garrison/guest-hero merging & weakest-troop dismissal logic :314-514) then only defensive structures; else `CastleDevelopment`.
- `CastleDevelopment` (:164-249): after day 6 a castle without a Well builds Well first; then income structures `{BUILD_CASTLE, STATUE, MARKETPLACE}` (+SPEC for Warlock) (:65-71); shipyard forced on islands/peninsulas; then race-specific `GetBuildOrder` (:73-137) — every order begins `{BUILD_CASTLE, prio 2}, {STATUE,1}, {MARKETPLACE,1}` then dwellings 6→1 with upgrades; mage guilds later (Knight/Barbarian de-prioritize magic; Wizard prioritizes it). `BuildOrder.priority` is a funds multiplier — build only if kingdom funds ≥ priority × cost (`BuildIfEnoughFunds`), so priority 10 items (thieves' guild, WEL2) need 10× spare resources.
- Budget: `updateKingdomBudget` (:270-312) marks resources missing for build orders as priority (→ ×2 in `getResourcePriorityModifier`) and level-6-dwelling unit costs as recurring (×1.5).
- Easy difficulty gimps: builds only every 2nd day and never DWELLING_MONSTER6/MAGEGUILD5 (`Difficulty::allowAIToDevelopCastlesOnDay` `game/difficulty.cpp:288-298`, `allowAIToBuildCastleBuilding` :300-311). Normal+ has no such limits.

---

## 6. Difficulty → AI modifiers (`game/difficulty.cpp`) — enum EASY=0, NORMAL=1, HARD=2, EXPERT=3, IMPOSSIBLE=4 (`difficulty.h:39-46`, order fixed for map format)

| Modifier | EASY | NORMAL | HARD | EXPERT | IMPOSSIBLE | Cite |
|---|---|---|---|---|---|---|
| Resource income bonus (`getResourceIncomeBonusForAI`) | none | **none** | +1 gold-mine income (1000 g/day) | +1000 g/day + per-castle bonuses | +2000 g/day + per-castle bonuses | :59-147 (applied `kingdom/kingdom.cpp:623-639` in `GetIncome`) |
| Gold income multiplier (`getGoldIncomeBonusForAI`) | **−25%** | 0 | 0 | 0 | 0 | :149-160 |
| Battle retreat when enemy/own strength > (`getArmyStrengthRatioForAIRetreat`) | 6.0 (default) | **7.5** | 8.5 | 8.5 | 10.0 | :162-177 (used `ai/ai_battle.cpp:773`) |
| Dimension Door casts/turn | 1 | **2** | 3 | ∞ | ∞ | :179-193 |
| Hero roles allowed | no | **yes** | yes | yes | yes | :195-212 |
| Min stat diff to merge AI armies | 10 | **2** | 2 | 2 | 2 | :214-232 |
| Split weak stacks | no | **no** | yes | yes | yes | :234-245 |
| Guardian-spell SP multiplier | 16 | **14** | 12 | 10 | 8 | :247-267 |
| Share visited-object info with AI allies | no | **no** | yes | yes | yes | :269-286 (used `ai/ai_common.cpp:389-416`) |
| Castle develop every day / all buildings | no (odd days, no T6/MG5) | **yes** | yes | yes | yes | :288-311 |
| "Basic" (weaker) battle AI | for AI players | for auto-controlled humans only | same | same | same | :313-323 (`isBasicAIBattleLogicApplicable`) |
| Artifact sorting for AI | no | **no** | yes | yes | yes | :325-336 |
| Predict future-valid objects | no | **yes** | yes | yes | yes | :338-341 (used `ai_planner_hero.cpp:2499-2534`) |
| AI starting resources | {7500,20,5,20,5,5,5} | same | {10000,30,10,30,10,10,10} | same | same | `kingdom/kingdom.cpp:865-882` (Cost order: gold, wood, mercury, ore, sulfur, crystal, gems) |

**Normal AI has no economic cheats and no map vision cheat** — it sees only de-fogged tiles (`ai_planner_kingdom.cpp:725-727`) unless a hero casts View All. Per-day per-color `ClearFog` uses normal scouting rules.

---

## 7. Miscellaneous facts useful for the map generator

- Players init from FileInfo (`system/players.cpp:309-347`): every kingdom color starts `CONTROL_AI`; colors in `colorsAvailableForHumans` get `CONTROL_AI|CONTROL_HUMAN`; the **first** such color is forced pure `CONTROL_HUMAN`. (Auto-playtest later overrides all to human+auto-AI.)
- FH2M `FileInfo::loadResurrectionMap` note: `height = map.width` (`maps/maps_fileinfo.cpp:426-427`) — FH2M maps are square by definition.
- Victory/loss condition metadata sanity: VICTORY_DEFEAT_EVERYONE must have empty metadata and forces `compAlsoWins=true, allowNormalVictory=true` (`maps_fileinfo.cpp:476-483`); LOSS/VICTORY town/hero metadata = {tileIndex, colorMask}; gold victory ≥10000 and divided by 1000 (`:499-506`).
- `checkGameOver` in auto-playtest exercises exactly the same win/loss logic the human game uses, so a playtest also validates hero/town-based special conditions (`setHeroIdsForMapConditions` fails the load — returns false — if a WINS_HERO/LOSS_HERO position doesn't contain a hero, `world/world_loadmap.cpp:1758-1789`; for auto-playtest a failed `prepareMap` shows "Failed to prepare the map for auto playtest.", `game_auto_playtest.cpp:267-270`).
- AI hero cap interacts with map heroes: heroes placed on the map count against the 8-hero limit at load; extra ones are silently dropped (`VERBOSE_LOG "cannot be hired"`, `world_loadmap.cpp:890-892`).
- Jail heroes: neutral, hired on release; AI values jails only if `kingdom.GetHeroes().size() < 8` (`ai_planner_hero.cpp:728-729`).
- Recruit cost 2500 gold; max heroes 8; AI hero limit formula `w/36 + 2` (map 36→3, 72→4, 108→5, 144→6) clamped to [castleCount, 8] (`ai_planner_kingdom.cpp:932-943`).
