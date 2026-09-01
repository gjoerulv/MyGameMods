# fheroes2 Economy Mechanics (upstream HEAD b086d1aa, 2026-09-01)

All paths relative to `C:/Users/gjoer/source/repos/fheroes2/`.
**`Cost` struct field order (used in every brace-init below): `{ gold, wood, mercury, ore, sulfur, crystal, gems }`** — `src/fheroes2/resource/resource.h:37-46` (`uint16_t gold; uint8_t wood; uint8_t mercury; uint8_t ore; uint8_t sulfur; uint8_t crystal; uint8_t gems;`).

---

## 1. Kingdom starting resources — `Kingdom::_getKingdomStartingResources`
`src/fheroes2/kingdom/kingdom.cpp:865-903`. Applied in `Kingdom::Init` (kingdom.cpp:115), then reduced by human handicap (kingdom.cpp:121; MILD=85%, SEVERE=70%, rounded up — kingdom.cpp:63-95).

| Difficulty | Human (gold,w,m,o,s,c,g) | AI (gold,w,m,o,s,c,g) |
|---|---|---|
| EASY | 10000, 30, 10, 30, 10, 10, 10 | 7500, 20, 5, 20, 5, 5, 5 |
| NORMAL | 7500, 20, 5, 20, 5, 5, 5 | 7500, 20, 5, 20, 5, 5, 5 |
| HARD | 5000, 10, 2, 10, 2, 2, 2 | 10000, 30, 10, 30, 10, 10, 10 |
| EXPERT | 2500, 5, 0, 5, 0, 0, 0 | 10000, 30, 10, 30, 10, 10, 10 |
| IMPOSSIBLE | 0, 0, 0, 0, 0, 0, 0 | 10000, 30, 10, 30, 10, 10, 10 |

Note the second value in each tuple pair above is (wood) and third is (mercury) etc. per Cost order: e.g. NORMAL human = 7500 gold, 20 wood, 5 mercury, 20 ore, 5 sulfur, 5 crystal, 5 gems.

`Difficulty` enum: `EASY, NORMAL, HARD, EXPERT, IMPOSSIBLE` (difficulty.h:41-45). `Game::getDifficulty()` (game.cpp:75-91): non-campaign = player's chosen difficulty setting only.

---

## 2. Income sources — `ProfitConditions` (`src/fheroes2/kingdom/profit.cpp`)

### Per-day mine output — `ProfitConditions::FromMine` (profit.cpp:98-122)
| Mine | Output/day |
|---|---|
| Sawmill (WOOD) | 2 wood |
| Ore mine | 2 ore |
| Alchemist Lab (MERCURY) | 1 mercury |
| Sulfur mine | 1 sulfur |
| Crystal mine | 1 crystal |
| Gems mine | 1 gems |
| Gold mine | 1000 gold |

### Buildings — `ProfitConditions::FromBuilding` (profit.cpp:33-50)
| Source | Gold/day |
|---|---|
| Castle (BUILD_CASTLE) | 1000 |
| Town (BUILD_TENT, i.e. not yet a castle) | 250 |
| Statue (BUILD_STATUE) | 250 |
| Dungeon (BUILD_SPEC, Warlock only) | 500 |

### Other income (Kingdom::GetIncome, kingdom.cpp:564-646)
- INCOME flags (kingdom.h:73-78): CAPTURED=0x01, CASTLES=0x02, ARTIFACTS=0x04, HERO_SKILLS=0x08, CAMPAIGN_BONUS=0x10, ALL=0xFF.
- Artifacts: per-artifact GOLD/…_INCOME bonuses summed (profit.cpp:52-96) for every artifact in every hero's bag.
- Estates secondary skill: +100/250/500 gold per day per hero for Basic/Advanced/Expert (`game_static.cpp:104` `{ "estates", { 100, 250, 500 } }`; applied kingdom.cpp:604-610).
- AI-only bonuses (kingdom.cpp:623-639), see §10.
- Human handicap multiplier applied to final income (kingdom.cpp:645).

---

## 3. Day/week processing order & income timing

- `World::NewDay` (`src/fheroes2/world/world.cpp:425-460`): `++_day`; then NewMonth routines, then NewWeek routines (`vec_kingdoms.NewWeek(); vec_castles.NewWeek(); vec_heroes.NewWeek();`), then NewDay routines (`vec_kingdoms.NewDay(); vec_castles.NewDay(); vec_heroes.NewDay();`).
- Called once per game-day at top of turn loop (`src/fheroes2/game/game_startgame.cpp:790-793`) — **including at game start**, so the first `NewDay()` sets day=1.
- **Income is NOT applied in NewDay.** It is applied per-kingdom at the start of that kingdom's own turn via `Kingdom::ActionNewDayResourceUpdate` — human: game_startgame.cpp:1034-1046 (inside HumanTurn), AI: game_startgame.cpp:925-927 before `AI::Planner::KingdomTurn`. Skipped when resuming from a save.
- **Day 1 income is skipped**: `if ( world.CountDay() > 1 ) { AddFundsResource( GetIncome() ); }` (kingdom.cpp:227-230). So the first income lands on day 2. Date events (map events) fire even on day 1 (kingdom.cpp:234-245).
- `Kingdom::ActionNewDay` (kingdom.cpp:197-223) only handles lost-town countdown (`world.CountDay() > 1 && castles.empty()` decrements; loss when 0) — no resources.
- `Kingdom::ActionNewWeek` (kingdom.cpp:248-269): skips week 1; only debug gift + refresh recruit pair.
- `Castle::ActionNewDay` (castle.cpp:833-836) re-arms `ALLOW_TO_BUILD_TODAY` → **one construction per castle per day** (checked castle.cpp:1176-1178).
- `Castle::ActionNewWeek` (castle.cpp:838-947): **skips first week** (`world.CountWeek() < 2`, line 841). Growth per dwelling = `Monster(race, actualDwelling).GetGrown()` + 2 if Well + 8 if Horde Bldg (WEL2, level-1 dwelling only) (castle.cpp:866-875; constants game_static.cpp:145-153). Neutral towns get growth/2 (castle.cpp:877-880) and extra random garrison troops (40% chance for towns, always for castles, castle.cpp:906-914). "Week of <monster>" adds +5 flat to the matching dwelling (castle.cpp:885-904, GetGrownWeekOf game_static.cpp:155-158). "Month of <monster>" adds +100% of current stock (castle.cpp:917-945, GetGrownMonthOf game_static.cpp:160-163); Plague month halves all dwellings (castle.cpp:921-925).
- Adventure-map weekly objects (windmill etc.) reset in `World::NewWeek` only when `_week > 1` (world.cpp:462-477).
- Lost-town grace period: 7 days (`GameStatic::GetGameOverLostDays` game_static.cpp:130-133); counter initialized to 7+1 (kingdom.cpp:130, 319).

---

## 4. Heroes: max count and hiring
- **Max heroes per kingdom: 8** (`GameStatic::GetKingdomMaxHeroes`, game_static.cpp:140-143; used kingdom.cpp:559-562).
- **Hero recruit cost: flat 2500 gold, never scales** (`PaymentConditions::RecruitHero`, `src/fheroes2/kingdom/payment.cpp:42-45`; deducted in `Castle::RecruitHero`, castle.cpp:1004-1023).
- `Kingdom::AllowRecruitHero` (kingdom.cpp:499-502) = below max heroes AND can pay 2500.
- Tavern pool: 2 recruits per kingdom, refreshed weekly (`Kingdom::GetRecruits` kingdom.cpp:455-497; `World::NewWeek` resets RECRUIT modes world.cpp:479-490). At start of week a "native" (own race) hero is offered when both slots empty (kingdom.cpp:457-459, 485). Heroes that retreated/surrendered can reappear for hire.
- Other payments (payment.cpp): Boat = 1000 gold + 10 wood (32-35); Spell book = 500 gold (37-40); Alchemist tower artifact-curse removal = 750 gold (47-50); Magellan's Maps = 1000 gold (52-55).

---

## 5. Castle building costs — `buildingStats` (`src/fheroes2/castle/buildinginfo.cpp:106-200`)

### Race-common buildings (Race::ALL), lines 106-121
| Building | Cost |
|---|---|
| Thieves' Guild | 750g, 5w |
| Tavern | 500g, 5w |
| Shipyard | 2000g, 20w |
| Well | 500g |
| Statue | 1250g, 5 ore |
| Left Turret | 1500g, 5 ore |
| Right Turret | 1500g, 5 ore |
| Marketplace | 500g, 5w |
| Moat | 750g |
| **Castle** (from town) | 5000g, 20w, 20 ore |
| Captain's Quarters | 500g |
| Mage Guild 1 | 2000g, 5w, 5 ore |
| Mage Guild 2 | 1000g, 5w, 5 ore, 4 each rare (mercury/sulfur/crystal/gems) |
| Mage Guild 3 | 1000g, 5w, 5 ore, 6 each rare |
| Mage Guild 4 | 1000g, 5w, 5 ore, 8 each rare |
| Mage Guild 5 | 1000g, 5w, 5 ore, 10 each rare |
| WEL2 (horde bldg; all races) | 1000g (lines 123-128) |
| SPEC Knight (Fortifications) | 1500g, 5w, 15 ore (line 130) |
| Necromancer Shrine | 4000g, 10w, 10 crystal (line 137) |

### Knight dwellings (lines 139-149)
| Dwelling | Unit | Cost |
|---|---|---|
| DWELLING_MONSTER1 (Thatched Hut) | Peasant | 200g |
| DWELLING_MONSTER2 (Archery Range) | Archer | 1000g |
| DWELLING_UPGRADE2 | Ranger | 1500g, 5w |
| DWELLING_MONSTER3 (Blacksmith) | Pikeman | 1000g, 5 ore |
| DWELLING_UPGRADE3 | Veteran Pikeman | 1500g, 5 ore |
| DWELLING_MONSTER4 (Armory) | Swordsman | 2000g, 10w, 10 ore |
| DWELLING_UPGRADE4 | Master Swordsman | 2000g, 5w, 5 ore |
| DWELLING_MONSTER5 (Jousting Arena) | Cavalry | 3000g, 20w |
| DWELLING_UPGRADE5 | Champion | 3000g, 10w |
| DWELLING_MONSTER6 (Cathedral) | Paladin | 5000g, 20w, 20 crystal |
| DWELLING_UPGRADE6 | Crusader | 5000g, 10w, 10 crystal |

(Other races' dwelling costs at lines 151-200; e.g. WRLK DW6 Green Dragon Tower = 15000g,30 ore,20 sulfur; WZRD DW6 = 12500g,5w,5o,20 gems.)

### Knight build dependency tree — `getBuildingRequirement` (`src/fheroes2/castle/castle_building_info.cpp:1097-1350`)
- Global rules (Castle::CheckBuyBuilding, castle.cpp:1137-1210): a town can only build BUILD_CASTLE; everything else requires castle built (1180-1189). Mage Guild N requires Mage Guild N-1 (1168-1174). One build per castle per day (1176-1178).
- Knight (`Race::KNGT`):
  - DW2 ← DW1 (1108-1116)
  - DW3 ← DW1 + Well (1128-1133)
  - DW4 ← DW1 + Tavern (1148-1153)
  - DW5 ← DW2 + DW3 + DW4 (1179-1186)
  - DW6 ← DW2 + DW3 + DW4 (1211-1217)
  - UPG2 ← DW2+DW3+DW4 (1236-1243); UPG3 ← DW2+DW3+DW4 (1259-1265); UPG4 ← DW2+DW3+DW4 (1286-1293); UPG5 ← DW5 (1306-1311); UPG6 ← DW6 (1328-1334)
  - Statue/Marketplace/Well/Tavern/Thieves' Guild/Shipyard/Moat/Turrets/Captain: no building prereqs beyond having a castle (shipyard also needs sea access, castle.cpp:1149-1152).

---

## 6. Creature stats & costs — `src/fheroes2/monster/monster_info.cpp`
Battle stats table lines 233-307 (`attack, defence, damageMin, damageMax, hp, speed, shots`), general stats lines 309-382 (`growth/week, race, level, cost{gold,w,m,o,s,c,g}`). Speed enum values (`src/fheroes2/kingdom/speed.h:34-41`): VERYSLOW=2, SLOW=3, AVERAGE=4, FAST=5, VERYFAST=6, ULTRAFAST=7.

### Knight line
| Unit | Cost (gold) | Growth/wk | HP | Att | Def | Dmg | Speed | Shots |
|---|---|---|---|---|---|---|---|---|
| Peasant | 20 | 12 | 1 | 1 | 1 | 1-1 | VERYSLOW | 0 |
| Archer | 150 | 8 | 10 | 5 | 3 | 2-3 | VERYSLOW | 12 |
| Ranger | 200 | 8 | 10 | 5 | 3 | 2-3 | AVERAGE | 24 |
| Pikeman | 200 | 5 | 15 | 5 | 9 | 3-4 | AVERAGE | 0 |
| Veteran Pikeman | 250 | 5 | 20 | 5 | 9 | 3-4 | FAST | 0 |
| Swordsman | 250 | 4 | 25 | 7 | 9 | 4-6 | AVERAGE | 0 |
| Master Swordsman | 300 | 4 | 30 | 7 | 9 | 4-6 | FAST | 0 |
| Cavalry | 300 | 3 | 30 | 10 | 9 | 5-10 | VERYFAST | 0 |
| Champion | 375 | 3 | 40 | 10 | 9 | 5-10 | ULTRAFAST | 0 |
| Paladin | 600 | 2 | 50 | 11 | 12 | 10-20 | FAST | 0 |
| Crusader | 1000 | 2 | 65 | 11 | 12 | 10-20 | VERYFAST | 0 |

(monster_info.cpp:236-246 stats; 312-322 cost/growth. Paladin/Crusader double melee attack; Crusader 2x dmg vs undead, immune to Curse — lines 397-402. Cavalry/Champion 2-hex — 393-395.)

### Common neutral guards & requested extras
| Unit | Cost | Growth | HP | Att | Def | Dmg | Speed | Shots | Notes |
|---|---|---|---|---|---|---|---|---|---|
| Rogue | 50 | 8 | 4 | 6 | 1 | 1-2 | FAST | 0 | neutral (369) |
| Nomad | 200 | 4 | 20 | 7 | 6 | 2-5 | VERYFAST | 0 | neutral (370) |
| Ghost | 1000 | 3 | 20 | 8 | 7 | 4-6 | FAST | 0 | neutral (371) |
| Genie | 650 + 1 gem | 2 | 50 | 10 | 9 | 20-30 | VERYFAST | 0 | neutral (372) |
| Medusa | 500 | 5 | 35 | 8 | 9 | 6-10 | AVERAGE | 0 | neutral (373) |
| Skeleton | 75 | 8 | 4 | 4 | 3 | 2-3 | AVERAGE | 0 | NECR DW1 (359/283) |
| Zombie | 150 | 6 | 15 | 5 | 2 | 2-3 | VERYSLOW | 0 | NECR DW2 (360/284) |
| Orc | 140 | 8 | 10 | 3 | 4 | 2-3 | VERYSLOW | 8 | BARB DW2 (324/248) |
| Goblin | 40 | 10 | 3 | 3 | 1 | 1-2 | AVERAGE | 0 | BARB DW1 (323/247) |
| Wolf | 200 | 5 | 20 | 6 | 2 | 3-5 | VERYFAST | 0 | BARB DW3; 2-hex, double melee attack (326/250, 404-405) |
| Dwarf | 200 | 6 | 20 | 6 | 5 | 2-4 | VERYSLOW | 0 | SORC DW2; magic resist (333/257) |
| Sprite | 50 | 8 | 2 | 4 | 2 | 1-2 | AVERAGE | 0 | SORC DW1; flies (332/256) |

Rare-resource unit costs elsewhere: Cyclops 750g+1 crystal, Phoenix 1500g+1 mercury, Green/Red/Black Dragon 3000/3500/4000g+1/1/2 sulfur, Giant 2000g+1 gem, Titan 5000g+2 gems (lines 331, 340, 347-349, 357-358).

---

## 7. Marketplace exchange rates — `src/fheroes2/kingdom/resource_trading.cpp:48-56`
Rates indexed by marketplace count (1..9+, capped at 9; getTradeCost lines 61-141). "common" = wood/ore; "rare" = mercury/sulfur/crystal/gems. A **Trading Post uses fixed rate = 3 marketplaces** (`src/fheroes2/dialog/dialog_marketplace.cpp:57-60`).

| #Markets | res→res (units per 1) | common→rare (units per 1) | rare→common (units per 1) | sell common (gold/unit) | sell rare (gold/unit) | buy common (gold/unit) | buy rare (gold/unit) |
|---|---|---|---|---|---|---|---|
| 1 | 10 | 20 | 5 | 25 | 50 | 2500 | 5000 |
| 2 | 7 | 14 | 4 | 37 | 74 | 1667 | 3334 |
| 3 | 5 | 10 | 3 | 50 | 100 | 1250 | 2500 |
| 4 | 4 | 8 | 2 | 62 | 124 | 1000 | 2000 |
| 5 | 4 | 7 | 2 | 74 | 149 | 834 | 1667 |
| 6 | 3 | 6 | 2 | 87 | 175 | 715 | 1429 |
| 7 | 3 | 5 | 2 | 100 | 200 | 625 | 1250 |
| 8 | 3 | 5 | 2 | 112 | 224 | 556 | 1112 |
| 9+ | 2 | 4 | 1 | 124 | 249 | 500 | 1000 |

Semantics (ai_common.cpp:284-341): selling to gold → get `tradeCost` gold per unit; buying with gold → pay `tradeCost` gold per unit; res→res → pay `tradeCost` units of source per 1 unit of target. Marketplace count = castles with BUILD_MARKETPLACE (kingdom.cpp:354-358).

---

## 8. Pickup / visitable object rewards
Content is rolled once at map load in `setInitialObjectInfo` (`src/fheroes2/maps/maps_tiles_helper.cpp`), weekly objects re-rolled in `updateObjectInfoTile` (same file, 1641-1695) from `World::NewWeek` (world.cpp:465-477, only when `_week > 1`).

### Treasure Chest (land) — maps_tiles_helper.cpp:1302-1362; action heroes_action.cpp:1795-1904
- Roll: 31% → 2000 gold, 32% → 1500 gold, 32% → 1000 gold, **5% → random Treasure-level artifact**.
- Gold chest offers choice gold OR experience; `exp = gold > 500 ? gold - 500 : 500` (heroes_action.cpp:1858) → 1500/1000/500 exp.
- If artifact rolled but hero's bag full → 1000 gold instead (`GoldInsteadArtifact`, artifact.cpp:1066-1083).
- A chest placed on water is converted into a Sea Chest (1303-1324).

### Sea Chest — maps_tiles_helper.cpp:1269-1300
20% empty; 70% 1500 gold; 10% 1000 gold + Treasure artifact. Bag full → 1500 gold instead of artifact (artifact.cpp:1073-1074). No exp option.

### Campfire (and Resurrection "Barrel") — maps_tiles_helper.cpp:1207-1211 + getFundsFromTile 728-731
N = Rand(4,6) of one random non-gold resource **plus N×100 gold** (400-600 gold). One-shot pickup.

### Flotsam — maps_tiles_helper.cpp:1218-1239
25% each: {10 wood + 500 gold}, {5 wood + 200 gold}, {5 wood}, {empty}. One-shot (heroes_action.cpp:964-993).

### Water Wheel — updateObjectInfoTile 1652-1655
Gold on tile: **500 if `world.CountDay() == 0`** (initial map-load stock, i.e. week 1) else **1000** on each weekly reset. First visitor of the week collects; empty until next week (isWeekLife). Action: heroes_action.cpp:813-817.

### Windmill — updateObjectInfoTile 1657-1666
2 units of one random rare-ish resource (random from mercury/ore/sulfur/crystal/gems — random non-gold excluding wood: the `while(res==WOOD)` loop). Weekly reset.

### Magic Garden — updateObjectInfoTile 1644-1650
50/50: 5 gems or 500 gold. Weekly reset.

### Lean-To — maps_tiles_helper.cpp:1213-1216
Rand(1,4) units of one random non-gold resource. **Set once at map load; not in the weekly-reset list** — after looting it stays empty (metadata reset heroes_action.cpp:862).

### Skeleton (desert remains) — maps_tiles_helper.cpp:1058-1072
80% empty; 20% random artifact of any normal level (treasure/minor/major). Bag full → 1000 gold instead (artifact.cpp:1069-1072). One-shot, globally visited (heroes_action.cpp:866-909).

### Wagon — maps_tiles_helper.cpp:1074-1097
20% empty; 10% artifact (50/50 Treasure or Minor level); **50% resource: Rand(2,5) units of a random non-gold resource**. (Remaining 20% weight: percent queue is normalized — actual odds 25% empty / 12.5% artifact / 62.5% resource.) Artifact + full bag → nothing (heroes_action.cpp:922-927). One-shot.

### Resource piles on map — maps_tiles_helper.cpp:1153-1205
| Type | Amount |
|---|---|
| Gold | 100 × Rand(5,10) → 500-1000 in steps of 100 |
| Wood / Ore | Rand(5,10) |
| Mercury / Sulfur / Crystal / Gems | Rand(3,6) |

Random-resource object type roll (`Resource::Rand`, resource.cpp:94-116): uniform over 6 non-gold (or 7 with gold).

### Tree of Knowledge — maps_tiles_helper.cpp:1453-1465; action heroes_action.cpp:2946-3021
Price rolled at map load, uniform 1/3 each: **10 gems**, **2000 gold**, or **free**. Reward: exactly enough XP for one level-up (`GetExperienceFromLevel(level) - GetExperienceFromLevel(level-1)`, heroes_action.cpp:2964). Once per hero (Visit::GLOBAL for the refusal-tracking; hero-visited check line 2953). XP thresholds (heroes.cpp:1512-1556): L1=1000, L2=2000, L3=3200, L4=4500, L5=6000, L6=7700, L7=9000, L8=11000, L9=13200, L10=15500...

### Witch's Hut — maps_tiles_helper.cpp:1018-1026; heroes_action.cpp:1058-1107
Grants one random secondary skill at Basic level; pool = all skills except Leadership and Necromancy (game_static.cpp:265-271). Free; no effect if already known/skills full.

### Gazebo — heroes_action.cpp:1533-1536
+1000 XP, once per hero.

### Other one-shot values (for completeness)
- Derelict Ship: 5000 gold after fight (maps_tiles_helper.cpp:1364-1366).
- Shipwreck: 40% 10 Ghosts/1000g, 30% 15/2000g, 20% 25/5000g, 10% 50/2000g+artifact (1368-1406). Bag-full substitute 5000g (artifact.cpp:1077-1078).
- Graveyard: fight → random normal artifact + 1000 gold (1408-1411); substitute 2000g (artifact.cpp:1075-1076).
- Daemon Cave: Rand 1-4: 1000exp / 1000exp+2500g / 1000exp+artifact / pay 2500g (1424-1451).
- Shipwreck Survivor: artifact 55% Treasure / 30% Minor / 15% Major (1241-1267); bag full → 1000g.
- Abandoned Mine: guarded by Rand(30,60) Ghosts (1554-1557); becomes a 1000g/day gold mine when captured.
- Genie Lamp: Rand(2,4) Genies to hire (1587-1590).

---

## 9. Well / growth constants
- `GameStatic::GetCastleGrownWell() = 2` (game_static.cpp:145-148) — Well: +2/week to every dwelling.
- `GameStatic::GetCastleGrownWel2() = 8` (150-153) — horde building: +8/week to level-1 dwelling only.
- `GetCastleGrownWeekOf() = 5` (155-158); `GetCastleGrownMonthOf() = 100` (% bonus, 160-163).

---

## 10. Difficulty effects (AI runtime), `src/fheroes2/game/difficulty.cpp`

### Income (getResourceIncomeBonusForAI 59-147, getGoldIncomeBonusForAI 149-160; applied kingdom.cpp:623-639)
| Difficulty | AI extra income | AI gold multiplier |
|---|---|---|
| EASY | none | **-25% of total gold income** |
| NORMAL | **none** | 0 |
| HARD | +1 gold-mine set (=1000 gold/day) | 0 |
| EXPERT | +1000 gold/day + per-castle bonus* | 0 |
| IMPOSSIBLE | +2000 gold/day + per-castle bonus* | 0 |

\* per-castle bonus (lines 71-133): +1000 gold/day per castle; castles with DW6 built additionally get their race's rare resource (BARB +1 crystal, SORC +1 mercury, WRLK +1 sulfur, WZRD +1 gems — only if kingdom has a marketplace or a mine of that resource) and WRLK/WZRD +1000 more gold/day.

### Other per-difficulty AI knobs
| Function (line) | EASY | NORMAL | HARD | EXPERT | IMPOSSIBLE |
|---|---|---|---|---|---|
| getArmyStrengthRatioForAIRetreat (162-177) | 100/6.0 | 100/7.5 | 100/8.5 | 100/8.5 | 100/10 |
| GetDimensionDoorLimitForAI (179-193) | 1 | 2 | 3 | ∞ | ∞ |
| areAIHeroRolesAllowed (195-212) | no | yes | yes | yes | yes |
| getMinStatDiffForAIHeroesMeeting (214-232) | 10 | 2 | 2 | 2 | 2 |
| allowAIToSplitWeakStacks (234-245) | no | no | yes | yes | yes |
| getGuardianSpellMultiplier (247-267) | 16 | 14 | 12 | 10 | 8 |
| isObjectVisitInfoSharingAllowedForAI (269-286) | no | no | yes | yes | yes |
| allowAIToDevelopCastlesOnDay (288-298) | every 2nd day (non-campaign) | daily | daily | daily | daily |
| allowAIToBuildCastleBuilding (300-311) | no DW6 / MG5 (non-campaign) | all | all | all | all |
| isArtifactSortingAllowedForAI (325-336) | no | no | yes | yes | yes |
| isFutureObjectPredictionAllowedForAI (338-341) | no | yes | yes | yes | yes |

**At NORMAL the AI gets zero economic bonuses** — same starting resources as human (7500/20/5/20/5/5/5) and unmodified income.

---

## 11. Misc map-generation-relevant facts
- Kingdom serialization order (kingdom.cpp:905-910): `modes, _color, resource, lost_town_days, castles, heroes, recruits, visit_object, puzzle_maps, _visitedTentsColors, _topCastleInKingdomView, _topHeroInKingdomView, _monstersUnderVision`.
- `Kingdom::AllowPayment` (kingdom.cpp:384-390) per-resource >= check.
- Neutral monster tile growth handled by `updateMonstersOnTile` weekly (world.cpp:473-475).
- Handicap income percentages: NONE 100 / MILD 85 / SEVERE 70 (kingdom.cpp:63-78), applied to both starting resources and daily income with round-up (`(x*pct+99)/100`).
