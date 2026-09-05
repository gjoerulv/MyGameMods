# FH2M Generator & HoMM2 Design — Research Ledger

## Scenario: "The King's Ransom"

Blue human Knight vs Green AI Knight. Victory = possess ≥100,000 gold (Blue
only — see §6.3). Loss = out of time on day 56. Normal difficulty. 36×36 map
(standard **Small**, `Maps::SMALL = 36`).

Reference document for engineers implementing the `.fh2m` generator. Consolidates
11 files from `research/notes/01_*.md`–`11_*.md` (fheroes2 @ HEAD
`b086d1aa8b921163712aec2fb8188f4d0d375b09`, 2026-09-01) with `file:line`
citations preserved from those notes. Full exhaustive tables (e.g. the 147-entry
LANDSCAPE_MISCELLANEOUS list) live in the note files; this ledger gives only
what the generator consumes plus pointers.

---

## 1. Header: commit, environment, compatibility

| Field | Value |
|---|---|
| Research commit (note stamps, `file:line` citations) | `b086d1aa8b921163712aec2fb8188f4d0d375b09` (2026-09-01) — each note header names the commit at which that note was last verified |
| Current pin (clone checkout, mapgen validated) | `d778cb44b30e4fcf81ee70ccf96354b355c81c4f` (2026-09-04), pinned 2026-09-05 with identical map hashes; every later check is one row in `research/upstream_log.md` |
| Ledger compiled | 2026-09-01, Windows 11 Pro 10.0.26200 |
| Installed game | release **1.1.17** (`D:\Spill\Homm2\fheroes2_windows_x64_SDL2\fheroes2.exe`), tag `2685c2188b541660f1ce261b554c3e92f79b1775`, **79 commits behind the pin** |

**Compatibility.** `git diff 1.1.17 d778cb44 -- src/fheroes2/maps/map_format_info.{h,cpp}`
= 0 lines — serializer byte-identical (notes/01 §8; re-checked 2026-09-05). Both revisions:
`minimumSupportedVersion=2`, `currentSupportedVersion=13`; save always writes
current version regardless of `map.version`. v13 shipped since 1.1.14. **A map
this generator writes (v13) loads correctly in both the installed 1.1.17 binary
and any HEAD build.**

**Build verification (this session).** A full MSVC build of fheroes2 @ HEAD
succeeded on this machine: VS2026, MSBuild, v143 toolset, Release-SDL2\|x64,
using the project's SHA256-pinned prebuilt-deps archive (`windows.zip` via
`script/windows/install_packages.bat`, notes/08 §1).

**Generator architecture** — standalone MSVC console tool `mapgen` (repo root
`mapgen/`, `gen_vcxproj.py` generates its project):
1. Compiles all fheroes2 engine sources except the game's `main()`, plus
   generator sources, into one exe — the same cherry-picked-`.cpp` pattern
   upstream uses for its own tools (`src/tools/icn2img-vs2019.vcxproj`, notes/08 §1).
2. Calls `world.generateUninitializedMap(width)` to drive the **real** editor
   helpers rather than reimplement them: `Maps::setTerrainWithTransition`,
   `Maps::setObjectOnTile` + `Maps::addObjectToMap`, `Maps::setRoadOnTile`,
   `Maps::addStream`, `Maps::updateMapPlayers` — Strategy 1 of notes/03 §7.
3. Calls the genuine `Maps::Map_Format::saveMap`.
4. Determinism via reseeding `Rand::CurrentThreadRandomDevice() =
   Rand::PCG32(seed)` (all terrain/road/stream/monster randomness flows through
   `Rand::Get`).

**Verified fallback** (notes/08, tested end-to-end by a research agent this
session): MinGW g++ 8.1.0, 13 unmodified fheroes2 `.cpp` files + ~40-line
`stubs.cpp`, no SDL/AGG/audio, `-lz` only. Confirmed `saveMap`/`loadMap`
round-trip, and confirmed loading + resaving the real bundled
`maps/4_dimensions.fh2m` (v10, 144×144, 40 castles, 8 heroes → resaved as v13).
Fallback path if the full-engine MSVC link hits a blocker.

**Provenance of prior "known facts."** All facts supplied by the commissioning
prompt were re-confirmed at HEAD, **no contradictions found**: magic `h2map\0`
(notes/01 §1, `map_format_info.cpp:101`); version 13 (notes/01 §2, cpp:107-110);
big-endian (notes/01 §3); min size 512B (notes/01 §1/§7, cpp:105); container
count-prefix encoding (notes/01 §3); castle = basement+town+2 flags sharing one
UID (notes/06 §5, notes/02 §5.18); `VICTORY_COLLECT_ENOUGH_GOLD=5` (notes/04 §4);
`LOSS_OUT_OF_TIME=3` with `CountDay()>days` boundary (notes/04 §4,
`world.cpp:1066-1067`); standard sizes 36/72/108/144 (notes/01 §10); grass gold
mine = `ADVENTURE_MINES` index 9 (notes/02 §5.12, notes/06 §12).

One nuance: gold-victory metadata is effectively `floor(gold/1000)*1000`, since
`FileInfo::victoryConditionParams[0] = metadata[0]/1000` is truncated into a
`uint16_t` (`maps_fileinfo.cpp:505`). For this scenario's 100,000 (a multiple of
1000) this causes zero loss. Also confirmed: both `isVictoryConditionApplicableForAI`
and `allowNormalVictory` are honored for the gold condition specifically
("both checkboxes honored", `editor_map_specs_window.cpp:1001-1003`).

---

## 2. Native `.fh2m` format (full detail: notes/01)

```
offset 0   6 bytes   magic "h2map\0"
offset 6   ...       BaseMapFormat, PLAINTEXT, big-endian ints
offset ?   to EOF    one zlib (RFC1950) stream — rest of MapFormat, no size prefix/framing
```

Not the framed `zipStreamBuf` format used by save-games. `minimumSupportedVersion=2`,
`currentSupportedVersion=13` (always written on save); v13 unchanged since 1.1.14.

**Wire encodings** (all BE): `bool`/`uint8_t`/enum = 1 byte; `uint16_t`=2B;
`uint32_t`=4B; `std::string` = u32 len + raw bytes, no NUL; `vector`/`list`/`set`
= u32 count + elements; `map` = u32 count + (k,v) ascending; `std::array<T,N>`
**also** writes a redundant u32 count(=N), load hard-fails on mismatch;
`Funds` = 7×`int32_t`, order **wood, mercury, ore, sulfur, crystal, gems, gold**.

**BaseMapFormat field order** (plaintext, `map_format_info.cpp:438-446`):
`version, isCampaign, difficulty, availablePlayerColors, humanPlayerColors,
computerPlayerColors, alliances, playerRace[6], victoryConditionType,
isVictoryConditionApplicableForAI, allowNormalVictory, victoryConditionMetadata,
lossConditionType, lossConditionMetadata, width, mainLanguage, name, description,
creatorNotes, translations`. Min size empty: 54B. `width<=0` fails.

**MapFormat compressed section**, 15 containers in order:
`additionalInfo, tiles, dailyEvents, rumors, castleMetadata, heroMetadata,
sphinxMetadata, signMetadata, adventureMapEventMetadata,
selectionObjectMetadata, capturableObjectsMetadata, monsterMetadata,
artifactMetadata, resourceMetadata, translationInfo`. `tiles.size()==width*width`.
All metadata maps keyed by object UID except `translationInfo` (by language).
Full struct layouts / op<< line refs: notes/01 §5, §9.

**Generator checklist:** (1) magic→plaintext header→one zlib blob, all BE incl.
counts; (2) always version=13; (3) `std::array` redundant count must match N;
(4) `tiles` = `width*width`, square only (here `width=36`); (5) file **≥512B**
(a uniform 36×36 map compressed to 130B and was correctly rejected — notes/08
"Test-run gotchas"; pad via `description`/`creatorNotes`, uncompressed, if ever
short); (6) emit `map` in ascending key order for reproducibility (not
required); (7) editor UI caps, not loader-enforced: `messageCharLimit`=999
(description/creatorNotes/event/sign/sphinx text), `nameCharLimit`=30
(castle/hero names), map name cap 50 (`editor_interface.cpp:115`), rumor cap
200; (8) `PlayerColorsSet` = 1-byte bitmask (`BLUE=0x01…PURPLE=0x20`).

---

## 3. Tile & object model, object tables (full detail: notes/02)

```cpp
struct TileObjectInfo { uint32_t id{0}; ObjectGroup group{NONE}; uint32_t index{0}; };
struct TileInfo        { uint16_t terrainIndex{0}; uint8_t terrainFlags{0}; std::vector<TileObjectInfo> objects; };
```
(`map_format_info.h:42-57`.) An object is stored **only on its main tile**;
covered tiles are reconstructed at load from `getObjectInfo(group,index)`. `id`
(UID) must be >0.

**`ObjectGroup`** (`map_object_info.h:127-168`, `uint8_t`, sequential):
`NONE=0, ROADS=1, STREAMS=2, LANDSCAPE_MOUNTAINS=3, LANDSCAPE_ROCKS=4,
LANDSCAPE_TREES=5, LANDSCAPE_WATER=6, LANDSCAPE_MISCELLANEOUS=7,
LANDSCAPE_TOWN_BASEMENTS=8, LANDSCAPE_FLAGS=9, ADVENTURE_ARTIFACTS=10,
ADVENTURE_DWELLINGS=11, ADVENTURE_MINES=12, ADVENTURE_POWER_UPS=13,
ADVENTURE_TREASURES=14, ADVENTURE_WATER=15, ADVENTURE_MISCELLANEOUS=16,
KINGDOM_HEROES=17, KINGDOM_TOWNS=18, MONSTERS=19, MAP_EXTRAS=20, GROUP_COUNT=21`.

Table sizes for bounds-checks: ROADS 513, STREAMS 13, MOUNTAINS 63, ROCKS 40,
TREES 69, LANDSCAPE_WATER 12, LANDSCAPE_MISC 147, BASEMENTS 8, FLAGS 14,
ARTIFACTS 96, DWELLINGS 25, MINES 56, POWER_UPS 23, TREASURES 13, ADV_WATER 13,
ADV_MISC 71, HEROES 42, TOWNS 14, MONSTERS 71, MAP_EXTRAS 7 (notes/02 §8).

### Key indices used by this generator

| Group | Index rule | Values used |
|---|---|---|
| `LANDSCAPE_TOWN_BASEMENTS`(8) | fixed per ground | **0 = grass** (order grass,snow,swamp,lava,desert,dirt,wasteland,beach — `getTownBasementId`, `ui_map_object.cpp:295-326`) |
| `KINGDOM_TOWNS`(18) | `race=i/2, isCastle=(i%2==0)` | **0 = Knight castle**, 1 = Knight town |
| `LANDSCAPE_FLAGS`(9) | `colorIdx*2`(left)/`+1`(right) | **Blue 0/1, Green 2/3**, neutral **12/13** |
| `KINGDOM_HEROES`(17) | `color*7+raceSlot` | Blue Knight=`0*7+0=0`; Green Knight=`1*7+0=7`; slot 6=random hero |
| `ADVENTURE_MINES`(12) | `terrainSlot*5+resourceSlot` | grass gold=`1*5+4=9`; resourceSlot Ore0/Sulfur1/Crystal2/Gems3/Gold4; terrainSlot generic/beach0,grass1,snow2,swamp3,lava4,desert5,dirt6,wasteland7 |
| `ADVENTURE_MINES` sawmill | `48+sawmillSlot` | grass/swamp0→**48**; snow1,lava2,desert/beach3,dirt4,wasteland5 |
| `ADVENTURE_MINES` abandoned | `40+terrainSlot` | grass=**41** |
| `ADVENTURE_TREASURES`(14) | fixed | **0-6** resource piles (wood…gold), 7 Genie's Lamp, 8 random resource, **9=Treasure Chest**, 10-12 campfires |
| `MONSTERS`(19) | `monster id − 1` | uniform incl. the 5 random-monster entries |
| `ADVENTURE_MISCELLANEOUS`(16) | fixed | **18-22 signs** (snow/swamp/lava/desert/generic), **23/24 water wheel** (snow/generic), **4-6 windmill** (grass/snow/dirt), **41 magic garden**, **39 event**, **31 sphinx** |
| `ROADS`(1) | neighbor-direction bitmask | `0-255`=`Direction` mask; `256-511`=mask+256 (alt sprite, `Rand::Get(1)*256`); **512**=castle-entrance apron |
| `STREAMS`(2) | neighbor shape | `0-12`, see §4 |

`metadata[]` quick table: `ADVENTURE_MINES` [0]=resource bit, [1]=daily income
(Ore2, Sulfur/Crystal/Gems1, Gold1000, Sawmill2, Lab1); `ADVENTURE_TREASURES`
piles [0]=resource bit; `KINGDOM_HEROES` [0]=color idx, [1]=race slot;
`KINGDOM_TOWNS` [0]=race slot, [1]=1 castle/0 town; `LANDSCAPE_FLAGS` [0]=color
idx; `MONSTERS` [0]=monster id.

---

## 4. Terrain, transitions, roads, streams (full detail: notes/03)

**Ground image ranges** (`TIL::GROUND32`, 0-431): Water 0-29, Grass 30-91, Snow
92-145, Swamp 146-207, Lava 208-261, Desert 262-320, Dirt 321-360, Wasteland
361-414, Beach 415-431. `terrainIndex` alone determines ground type via range
comparison (`getGroundByImageIndex`, `ground.cpp:61-94`).

**Sub-layout** (rel. to start index), six full-land terrains: `+0..+3`
straight→dirt; `+4..+7` corner→dirt; `+8..+11` straight→dirt (other axis);
`+12..+15` ¾-tile dirt corner; `+16..+31` same four families toward
water/beach; `+32..+37` mixed beach+dirt diagonals; `+38..+45` plain (8
variants); `+46+` plain with embedded decor. **Dirt** only transitions toward
water/beach — land-land borders render using the *other* terrain's "to dirt"
images. **Beach has no transition images** and is a universal, transition-free
connector to water.

**`terrainFlags`**: bit0=vertical flip, bit1=horizontal flip
(`map_format_helper.cpp:196`). No road/stream bit — those are separate
`TERRAIN_LAYER` objects.

**Road index**: 513-entry table. `index<256` **is** the `Direction` bitmask
(`TOP_LEFT=0x01,TOP=0x02,TOP_RIGHT=0x04,RIGHT=0x08,BOTTOM_RIGHT=0x10,
BOTTOM=0x20,BOTTOM_LEFT=0x40,LEFT=0x80`) of neighboring road tiles; `256-511`
= mask+256 (random alt sprite); **512** = fixed castle-entrance apron, chosen
when the tile directly **above** holds any `KINGDOM_TOWNS` object
(`getRoadObjectIndex`, `map_format_helper.cpp:858-882`).

**Stream index** (0-12): left+bottom→0, right+bottom→1, top+right→4,
top+left→7 (corners); left+top+right→8, top+right+bottom→9, top+left+bottom→10,
left+right+bottom→11 (T-shapes); all-four→6 (cross); left/right only→2 or 5
(random, horizontal); else→3 or 12 (random, vertical/isolated)
(`getStreamIndex`, `map_format_helper.cpp:758-810`).

**Movement costs** (`Ground::GetPenalty`, `ground.cpp:169-242`):

| Ground | None | Basic | Adv | Expert |
|---|---|---|---|---|
| Desert | 200 | 175 | 150 | 100 |
| Swamp | 175 | 150 | 125 | 100 |
| Snow | 150 | 125 | 100 | 100 |
| Wasteland/Beach | 125 | 100 | 100 | 100 |
| Lava/Dirt/Grass/Water | 100 | 100 | 100 | 100 |
| **Road** | **75** | 75 | 75 | 75 |

Road rate applies only when both source+dest tiles are road, else the source
tile's ground penalty is charged. **Diagonal movement ×1.5** (`penalty*3/2`),
applied after road/ground selection (`world_pathfinding.cpp:332-367`).

---

## 5. Placement recipes and UID semantics (full detail: notes/06)

**UID mechanics**: one global 1-based counter (`world_object_uid.cpp`):
`getNewObjectUID()` increments+returns; `getLastObjectUID()` peeks;
`setLastObjectUID(n)` rewinds. Compound objects share one UID by rewinding the
counter before each additional part.

**`addObjectToMap` metadata auto-creation** (`map_format_helper.cpp:1089-1195`):

| Group/object | Auto-created | Notes |
|---|---|---|
| `KINGDOM_HEROES` | `heroMetadata[uid]` | `.race=Race::IndexToRace(metadata[1])` |
| `KINGDOM_TOWNS` | `castleMetadata[uid]` | `builtBuildings`={BUILD_TENT} or {BUILD_CASTLE} per object's isCastle flag |
| Jail | `heroMetadata[uid]` | race=RAND |
| `MONSTERS` | `monsterMetadata[uid]` | count defaults 0 |
| `OBJ_EVENT`/`OBJ_SIGN`/`OBJ_SPHINX` | resp. metadata map | |
| `OBJ_BOTTLE` | `signMetadata[uid]` | |
| `ADVENTURE_ARTIFACTS` | `artifactMetadata[uid]` | |
| **NOT** auto-created | `resourceMetadata`, `capturableObjectsMetadata` | editor sets these by hand |

**Compound castle recipe** (matches real editor `_placeCastle`), Blue Knight
castle at tile `i` (grass): let `U=next()`. (1) tile `i`: `{U, LANDSCAPE_TOWN_BASEMENTS, 0}`
(grass basement). (2) tile `i`: `{U, KINGDOM_TOWNS, 0}` (Knight castle; 1=town).
(3) tile `i-1`: `{U, LANDSCAPE_FLAGS, 0}` (left blue flag). (4) tile `i+1`:
`{U, LANDSCAPE_FLAGS, 1}` (right blue flag). (5) `castleMetadata[U]` auto-seeds
`builtBuildings=[BUILD_CASTLE]`; set `customBuildings=true` + explicit list for
determinism (Normal-difficulty DW2 is otherwise a 50% coin flip, §11). (6)
optional road index 512 below the entrance. Constraints: entrance not water;
whole 5×2 footprint on land/on-map; `0<i<width*height-1`. Owner color derives
**purely** from flag indices used (0/1⇒Blue), not any per-instance field. Same
sequence for Green uses flag indices 2/3. Starting hero convention (matches
RMG): place a `KINGDOM_HEROES` object on the tile directly below the entrance —
`Kingdom::ApplyPlayWithStartingHero` moves it into the castle at load
(`kingdom.cpp:504-541`).

**`updateMapPlayers`** — the mandatory finalize pass, called once right before
`saveMap` in the real editor's own save path (`editor_interface.cpp:3204-3273`;
no separate "verifyMap" exists). Recomputes `availablePlayerColors`,
`playerRace[6]`, sanitizes human/computer masks and alliances, resets invalid
special win/loss metadata, prunes stale capture ownership. **No** passability
computation or render-order sort — both reconstructed at load. This scenario:
`availablePlayerColors=BLUE|GREEN(0x03)`, `humanPlayerColors=BLUE(0x01)`,
`computerPlayerColors=GREEN(0x02)`, `playerRace[0]=playerRace[1]=KNGT(0x01)`.

**Load validation vs. silent breakage.** Hard-fails: magic/version/size/tiles-
count (§2); `LOSS_HERO`/`LOSS_TOWN`/`VICTORY_KILL_HERO`/`VICTORY_CAPTURE_TOWN`
metadata must reference a real matching object (`world_loadmap.cpp:1364-1395`
— not applicable here, this scenario uses gold/time conditions);
`availablePlayerColors==0` aborts. Silent breakage: a town missing/mismatched
flags → `getTownColorIndex` returns 0 → silently becomes Blue in release
(debug asserts); UID with no `castleMetadata` gets an empty entry via
`operator[]`; duplicate UIDs across unrelated objects diagnosed only in
`WITH_DEBUG` builds; wrong terrain/road/stream indices render incorrectly but
load fine — **nothing fixes bad decorative data at load**.

---

## 6. Victory / loss semantics (full detail: notes/04)

Enums: `VictoryCondition{DEFEAT_EVERYONE=0,CAPTURE_TOWN=1,KILL_HERO=2,
OBTAIN_ARTIFACT=3,DEFEAT_OTHER_SIDE=4,COLLECT_ENOUGH_GOLD=5}`;
`LossCondition{EVERYTHING=0,TOWN=1,HERO=2,OUT_OF_TIME=3}` (`maps_fileinfo.h:160-176`).

**Runtime checks.** Gold — `World::KingdomIsWins`, case `WINS_GOLD`
(`world.cpp:1003-1005`):
```cpp
return ( ( kingdom.isControlHuman() || mapInfo.WinsCompAlsoWins() ) && 0 < kingdom.GetFunds().Get( Resource::GOLD )
         && static_cast<uint32_t>( kingdom.GetFunds().Get( Resource::GOLD ) ) >= mapInfo.getWinningGoldAccumulationValue() );
```
Checks **current treasury**, not cumulative income — a live snapshot evaluated
at `checkGameOver()`. `WinsCompAlsoWins()=compAlsoWins && ((WINS_TOWN|WINS_GOLD)&ConditionWins())`.

Out-of-time — `World::KingdomIsLoss`, case `LOSS_TIME` (`world.cpp:1066-1067`):
`CountDay() > LossCountDays()`. `_day` starts 0, `NewDay()` pre-increments, so
day1=`_day==1`. **With metadata=[56]: day 56 fully playable; loss triggers at
start of day 57.**

**This scenario's exact config:**

| Field | Value |
|---|---|
| `victoryConditionType` | 5 (`VICTORY_COLLECT_ENOUGH_GOLD`) |
| `victoryConditionMetadata` | `[100000]` |
| `isVictoryConditionApplicableForAI` | `false` |
| `allowNormalVictory` | `false` |
| `lossConditionType` | 3 (`LOSS_OUT_OF_TIME`) |
| `lossConditionMetadata` | `[56]` |

`isVictoryConditionApplicableForAI=false` ⇒ `WinsCompAlsoWins()` false ⇒ Green
reaching 100,000 gold does **not** end the game — only Blue
(`isControlHuman()`) can win by gold. `allowNormalVictory=false` ⇒ the classic
"defeat all enemies" path is disabled — conquering Green's castle alone does
not win. Both flags apply because this is specifically the gold condition
(§1's nuance — the engine forces flags for most other condition types but not
this one). Net effect: pure economic race from Blue's perspective; Green can
only affect the outcome by denying Blue's gold (raiding, capturing mines,
threatening the castle) — Green cannot independently win. Day-56 timeout is
global/color-agnostic.

**Editor bounds:** gold `ValueSelectionDialogElement{min:10000,max:1000000,step:1000}`
(`editor_map_specs_window.cpp:1456`) — 100,000 well within range; days
`{min:1,max:3360(=10×336),default:28}` (`editor_map_specs_window.cpp:1875`) —
56 well within range. Debug asserts: `victoryConditionMetadata[0]>=10000`,
`lossConditionMetadata[0]>0` — both satisfied. `VICTORY_DEFEAT_EVERYONE`/
`LOSS_EVERYTHING` require empty metadata (not relevant here).

---

## 7. Economy mechanics (full detail: notes/05; original-source cross-check: notes/09)

**Starting resources** (`Kingdom::_getKingdomStartingResources`,
`kingdom.cpp:865-903`; order gold,wood,mercury,ore,sulfur,crystal,gems):

| Difficulty | Human | AI |
|---|---|---|
| Easy | 10000,30,10,30,10,10,10 | 7500,20,5,20,5,5,5 |
| **Normal** | **7500,20,5,20,5,5,5** | **7500,20,5,20,5,5,5** (identical) |
| Hard | 5000,10,2,10,2,2,2 | 10000,30,10,30,10,10,10 |
| Expert | 2500,5,0,5,0,0,0 | 10000,30,10,30,10,10,10 |
| Impossible | 0,0,0,0,0,0,0 | 10000,30,10,30,10,10,10 |

**Income:** Sawmill 2 wood/day, Ore mine 2/day, Alchemist Lab(mercury)/Sulfur/
Crystal/Gems mine 1/day each, Gold mine 1000/day; Castle 1000g/day, Town(tent)
250g/day, Statue +250g/day, Dungeon(Warlock) +500g/day; Estates skill
+100/250/500 g/day (Basic/Adv/Expert).

**Timing:** income NOT applied in `NewDay()`, applied per-kingdom's own turn.
**Day-1 income skipped** (`world.CountDay()>1` guard) — first income lands day
2. `Castle::ActionNewWeek` **skips week 1** entirely. Week: 25% chance "Week of
the (monster)" → +5 flat to that dwelling. Month (from month2/week5+): 40%
"Month of (monster)" → +100% stock; 10% "Month of Plague" → halve all, no
growth. Well +2/week/dwelling; Horde bldg (Knight Farm) +8/week to DW1 only.
56 days = 8 weeks = 2 months exactly → 7 growth cycles (weeks 2-8), one
month-2 roll (week 5).

**Knight building costs** (`buildinginfo.cpp:106-149`):

| Building | Cost |
|---|---|
| Town→Castle | 5000g,20w,20ore |
| Thieves' Guild | 750g,5w |
| Tavern | 500g,5w |
| Shipyard | 2000g,20w |
| Well | 500g |
| Statue | 1250g,5ore |
| L/R Turret | 1500g,5ore each |
| Marketplace | 500g,5w |
| Moat | 750g |
| Captain's Quarters | 500g |
| Mage Guild L1 | 2000g,5w,5ore |
| Mage Guild L2-L5 | 1000g,5w,5ore +4/6/8/10 each rare |
| Horde bldg (Farm) | 1000g |
| Fortifications | 1500g,5w,15ore |
| DW1 Thatched Hut (Peasant) | 200g |
| DW2 Archery Range (Archer) | 1000g |
| DW2 upg (Ranger) | 1500g,5w |
| DW3 Blacksmith (Pikeman) | 1000g,5ore |
| DW3 upg (Vet.Pikeman) | 1500g,5ore |
| DW4 Armory (Swordsman) | 2000g,10w,10ore |
| DW4 upg (Mstr.Swordsman) | 2000g,5w,5ore |
| DW5 Jousting Arena (Cavalry) | 3000g,20w |
| DW5 upg (Champion) | 3000g,10w |
| DW6 Cathedral (Paladin) | 5000g,20w,20crystal |
| DW6 upg (Crusader) | 5000g,10w,10crystal |

Dependencies: DW2←DW1; DW3←DW1+Well; DW4←DW1+Tavern; DW5/DW6←DW2+DW3+DW4;
upgrades need their dwelling set; Castle required before anything else; one
build/castle/day.

**Knight creatures** (`monster_info.cpp:233-322`):

| Unit | Cost(g) | Growth/wk | HP | Att | Def | Dmg | Speed | Shots |
|---|---|---|---|---|---|---|---|---|
| Peasant | 20 | 12 | 1 | 1 | 1 | 1-1 | V.Slow | 0 |
| Archer | 150 | 8 | 10 | 5 | 3 | 2-3 | V.Slow | 12 |
| Ranger | 200 | upg | 10 | 5 | 3 | 2-3 | Average | 24 |
| Pikeman | 200 | 5 | 15 | 5 | 9 | 3-4 | Average | 0 |
| Vet.Pikeman | 250 | upg | 20 | 5 | 9 | 3-4 | Fast | 0 |
| Swordsman | 250 | 4 | 25 | 7 | 9 | 4-6 | Average | 0 |
| Mstr.Swordsman | 300 | upg | 30 | 7 | 9 | 4-6 | Fast | 0 |
| Cavalry | 300 | 3 | 30 | 10 | 9 | 5-10 | V.Fast | 0 |
| Champion | 375 | upg | 40 | 10 | 9 | 5-10 | Ultra | 0 |
| Paladin | 600 | 2 | 50 | 11 | 12 | 10-20 | Fast | 0 |
| Crusader | 1000 | upg | 65 | 11 | 12 | 10-20 | V.Fast | 0 |

Relevant neutrals: Rogue 50g, Nomad 200g, Ghost 1000g (undead, flies), Medusa
500g (petrify chance).

**Marketplace rates** (`resource_trading.cpp:48-56`; common=wood/ore,
rare=mercury/sulfur/crystal/gems; Trading Post always trades at the
3-marketplace rate):

| #Markets | same-class | common→rare | rare→common | sell common(g) | sell rare(g) | buy common(g) | buy rare(g) |
|---|---|---|---|---|---|---|---|
| 1 | 10 | 20 | 5 | 25 | 50 | 2500 | 5000 |
| 2 | 7 | 14 | 4 | 37 | 74 | 1667 | 3334 |
| 3 (Trading Post) | 5 | 10 | 3 | 50 | 100 | 1250 | 2500 |
| 4 | 4 | 8 | 2 | 62 | 124 | 1000 | 2000 |
| 5 | 4 | 7 | 2 | 74 | 149 | 834 | 1667 |
| 6 | 3 | 6 | 2 | 87 | 175 | 715 | 1429 |
| 7 | 3 | 5 | 2 | 100 | 200 | 625 | 1250 |
| 8 | 3 | 5 | 2 | 112 | 224 | 556 | 1112 |
| 9+ | 2 | 4 | 1 | 124 | 249 | 500 | 1000 |

**Treasure/pickup rolls:** Land Chest 31%2000g/32%1500g/32%1000g/5%Treasure-
artifact (bag full→1000g); XP option=gold−500 (500/1000/1500). Sea Chest
20%empty/70%1500g/10%1000g+artifact. Campfire Rand(4,6) one resource + N×100g
(400-600g total), one-shot. Water Wheel 500g wk1 else 1000g, weekly reset,
first visitor. Windmill 2 units random non-wood resource, weekly reset. Magic
Garden 50/50 5gems or 500g, weekly reset. Flotsam 25% each {10w+500g},
{5w+200g},{5w},{empty}. Resource piles: gold 500-1000 step100
(`100×Rand(5,10)`), wood/ore `Rand(5,10)`, rares `Rand(3,6)`.

**Neutral monster counts:** `monsterMetadata[uid].count==0` ⇒
`mons.GetRNDSize()` (per-level base, final=`Rand(result/2,result)`), 20%
`JOIN_CONDITION_FREE` else `MONEY` (Ghosts/elementals always SKIP). `count>0`
⇒ exact count, **always** `JOIN_CONDITION_MONEY` — "map-designer counts are
always hostile" (`maps_tiles_helper.cpp:1766-1770`), no free join possible.
Weekly growth (wk2+): `bonus=1 if Rand(1,7)<=count%7 else 0; growth=count/7+bonus`,
capped at 4,000,000 for FH2M maps (vs 4000 for legacy MP2) — effectively
uncapped here.

**Heroes:** recruit flat **2500g**, never scales; max **8**/kingdom; tavern
refreshes 2 candidates weekly.

---

## 8. AI findings (full detail: notes/07)

**No economic or vision cheats at Normal.** AI income bonus by difficulty:
Easy none/−25%gold; **Normal none/0%**; Hard +1000g/day; Expert
+1000g/day+per-castle; Impossible +2000g/day+per-castle. AI reads only
de-fogged tiles unless a hero casts View All — no map-vision cheat at any
difficulty (`ai_planner_kingdom.cpp:725-727`). Combined with identical
starting resources at Normal (§7), Green has zero structural
economic/vision advantage or disadvantage vs Blue.

**Hero recruitment / castle-less-hero start.** AI recruits only from a
**built** castle (`isCastle()==true`; a tent can never recruit —
`ai_planner_kingdom.cpp:955`), paying flat 2500g. Hero-count limit this turn:
early-game (day<5, 1 castle)→2; else `world.w()/36+2` clamped to
`[castleCount,8]` — for `w=36` that's `1+2=3`, clamped to Green's actual
castle count. A color owning a castle but no placed `KINGDOM_HEROES` object
gets **no** auto-created hero (`startWithHeroInFirstCastle` is MP2-only,
always false for FH2M) — legal for both sides; the AI self-recruits turn 1
(7500≥2500 affordable), a hero-less human has nothing to move day 1.
**This scenario places explicit starting heroes for both Blue and Green**
(tile directly below each entrance, §5) to guarantee day-1 presence and avoid
first-turn asymmetry.

**Attack thresholds** (× target strength): neutral wandering monster ×1.5
(×1.0 if losing); enemy hero open field ×1.3; enemy castle/garrison ×1.5;
guarded mine/sawmill/lab ×1.3; abandoned mine ×1.8; artifact w/ fight
condition ×1.8. **Guard-walls block AI pathfinding directly**:
`isTileAccessibleForAIWithArmy` treats a tile as impassable to the AI if
adjacent protecting-monster strength exceeds `armyStrength/advantageRatio` —
a correctly-scaled guard fully walls off a weaker AI hero and shields nearby
objects from "in danger" flagging via the same check.

**Auto-playtest — editor-UI-only, no headless mode.** Launch only via
Editor File-dialog (button idx 6) or hotkey **A**; `int main()` ignores
`argv` beyond config-dir init — zero CLI argument parsing anywhere in
`src/fheroes2`. Config: playthroughs 1-100 (default1), max days 1-1000
(default365), animation/sound toggles. All colors become `CONTROL_AI` for
the run; each playthrough is a fresh `world.loadResurrectionMap()` reload
(independent RNG per run). Release builds show only the final win%-per-color
dialog; `WITH_DEBUG` adds per-playthrough `VERBOSE_LOG`. Validation workflow:
copy `.fh2m` to `maps/` → Editor → load → press A → disable animation → read
results. Manual, cannot be scripted (see §11).

---

## 9. Design principles extracted (full detail: notes/10, notes/11)

**Checklist** (Kuzmanov/Kristo/Ururam synthesis): plan layout/story/win
condition before opening the editor; size fits content, no themeless empty
regions (here: S/36 deliberately, for a tight 2-castle duel); starting area =
town + reachable unguarded wood+ore within ~1 turn + one hook (Kristo: without
this "the map dies on hard difficulties"); guard **stepladder** — strength
scales with object value to the nearest player, never stack guards on
self-defending objects (Dragon City, Daemon Cave); density gradients not
uniform sprinkle — rotate obstacle sprites constantly, monotone ranges are the
#1 "generated" tell; roads connect objects (movement bonus + naturalism);
jagged organic terrain borders, never rectangular; multiple viable strategies
+ mild intentional asymmetry + test every color; in-game look pass required
(editor view ≠ rendered game — animation, fog, overlap differ).

**Community anti-patterns** (notes/11): recurring negative-resource weekly
events are the **most-hated** custom-map trope (Reddit, verbatim: "penalizing
me −10 crystals when I only make 7 a week") — design for one stated
difficulty instead (this scenario: Normal only, no penalty-event mechanic).
**Map size/richness is the community's real balance dial**, not per-faction
rebalancing — small maps favor fast/cheap factions (Knight/Barbarian/
Sorceress), large/rich maps favor late-scaling ones (Warlock/Wizard); this
scenario's Knight-vs-Knight same-race pairing sidesteps any faction-scaling
mismatch entirely. Treasure-chest rule of thumb: gold on small/aggressive/
timer-pressured maps (this scenario qualifies), XP only on large/rich maps or
very high level. Every start needs unguarded wood+ore within ~1 turn
(non-negotiable per Kristo).

**fheroes2 AI vs original AI:** fheroes2's AI is written from scratch with
**no map-vision cheat and no creature-growth cheat**, unlike the original
486-era engine (which cheated on both); its only difficulty knobs are extra
gold income and fewer self-imposed restrictions (e.g. Dimension Door limits)
at higher difficulty — at Normal neither knob is turned (§8). Since this map
targets fheroes2's own AI directly (not ported from the original), it avoids
the "legacy maps play harder under fheroes2's stronger AI" mismatch the
community frequently reports.

---

## 10. Confidence classification

Legend: **(a)** verified engine behavior at HEAD, file:line cited in source
notes; **(b)** original-game documented behavior ([MANUAL]/[GUIDE], page-cited);
**(c)** community design advice, attributed opinion; **(d)** design inference
by this project, no direct source.

| Category | Class | Primary source |
|---|---|---|
| Byte-level format: magic, version 13, min-size, BE, container framing | (a) | notes/01; `map_format_info.cpp:101-853` |
| `ObjectGroup` enum + index rules used | (a) | notes/02; `map_object_info.cpp` |
| Terrain-transition algorithm, road/stream index computation | (a) | notes/03; `map_format_helper.cpp:128-1897` |
| Victory/loss runtime checks (treasury snapshot, day boundary) | (a) | notes/04; `world.cpp:1003-1067` |
| Compound castle recipe, UID sharing, `addObjectToMap`, `updateMapPlayers` | (a) | notes/06; `map_format_helper.cpp:1089-1668`, `editor_interface.cpp:3204-3348` |
| Starting resources, mine/building income, day-1 skip, week/month events | (a) | notes/05; `kingdom.cpp`, `profit.cpp`, `castle.cpp` |
| Knight building/creature cost tables (engine values) | (a) | notes/05; `buildinginfo.cpp`, `monster_info.cpp` |
| Original manual's income/building/creature statements | (b) | notes/09, [MANUAL] page-cited |
| Marketplace table, treasure/pickup odds, per-difficulty numbers unprinted in the manual | (a), engine is source of truth; manual silent | notes/05 vs notes/09 §"Explicitly NOT FOUND" |
| AI valuation constants, attack thresholds, difficulty modifier tables | (a) | notes/07; `ai_planner_*.cpp`, `difficulty.cpp` |
| Auto-playtest: editor-only launch, config ranges, per-run reload | (a) | notes/07 §1; `game_auto_playtest.cpp`, `editor_interface.cpp` |
| Kuzmanov/Kristo/Ururam/Watkins design checklists | (c) | notes/10; archived Celestial Heavens articles |
| Reddit/GameFAQs/HC opinions (penalty hate, chest rule, tier impressions) | (c) | notes/11; attributed per item |
| fheroes2-vs-original AI cheat differences | (a) dev claims; (c) community-observed quirks | notes/11 §4.1-4.2 |
| MSVC build success on this machine | (a) — directly observed this session | session build log |
| Feasibility of the `mapgen` MSVC console-tool architecture | (a) static per-function analysis + MinGW fallback proof; full-engine MSVC path not yet run | notes/06 §11; notes/08 |
| This scenario's specific pacing/balance numbers | (d) | this document — inference from §9 applied to §7; not playtested |

---

## 11. Unresolved uncertainties

- **Headless `world` init not yet proven for the chosen MSVC path.** notes/06
  §11 shows by static analysis that `world.generateUninitializedMap(width)`
  transitively calls `Settings::Get()`/`World::Defaults()` (→ kingdom/hero/
  castle vector `Init()`), and notes/08's MinGW fallback proves a much
  smaller no-`world` subset links and runs. The full "link the whole engine,
  drive real `world`" strategy chosen for `mapgen` has not itself been
  executed end to end — only its prerequisites (engine build; isolated
  format I/O) are separately verified. Being validated during implementation.
- **Auto-playtest cannot be scripted** — no CLI entry point exists anywhere
  in `src/fheroes2` (exhaustive `argv` grep, notes/07 §1.6); validating this
  scenario's Blue-vs-Green balance requires a human UI step and cannot be
  folded into an automated pipeline.
- **Monster join/flee for authored counts:** `count>0` is always
  `JOIN_CONDITION_MONEY` (`maps_tiles_helper.cpp:1766-1770`) — fixed-count
  guards never offer free join; leave `count=0` if that's desired.
- **Editor description cap 999 chars** (`messageCharLimit`,
  `map_format_info.h:59-60`) — not loader-enforced; a longer generated string
  loads fine but the editor cannot re-save it without truncating.
- **Normal-difficulty DW2 is a 50% coin flip** (`dwelling2>=Rand::Get(1,100)`,
  `castle.cpp:537-562`) unless `customBuildings=true` with an explicit
  `builtBuildings` list is set — the generator should do this for both
  castles for deterministic starts.
- **Manual vs. guide disagreement on AI Normal starting resources.**
  [MANUAL] gives no numbers; [GUIDE] (1996 Prima) states AI-Normal =
  10000g/30w/10o/10-each-rare, but the engine (`kingdom.cpp:865-903`) gives
  AI the **same** as human at Normal (7500g/20w/5m/20o/5s/5c/5g). This
  project targets the fheroes2 engine, not the original binary (unavailable
  to cross-check) — **the engine value is authoritative** and is what this
  document uses; the guide figure may be a print error, a different
  difficulty than labeled, or genuine original-engine behavior this research
  could not verify.
- **`allowNormalVictory=false` × `LOSS_OUT_OF_TIME` interaction** is derived
  by reading `WinsCompAlsoWins()` and the metadata handling together (§6),
  not observed in a running game this session. The generated map should be
  loaded in-engine (§8 workflow) specifically to confirm neither color can
  win via the disabled normal-victory path before day 56.
- **This document's pacing choices are design inference (d), not
  engine-verified as balanced.** 100,000-gold target against 7,500 starting
  gold and plausible combined income growth (§7.2-7.3), and the 56-day
  window, follow §9's principles but have not been playtested — deferred to
  implementation/QA via the manual auto-playtest workflow.

---

## 12. Post-implementation addendum (resolutions to §11)

Written after the generator was implemented and the map validated (see
`kings_ransom_validation.md` for full evidence):

- **Headless `world` init: PROVEN.** `mapgen` (full-engine link, MSVC) runs
  `world.generateUninitializedMap(36)` + the real editor helpers headlessly
  with no display, no AGG assets, no asserts. The entire generate → saveMap →
  loadMap → byte-identical-resave pipeline works.
- **Game-load semantics: OBSERVED, not just derived.** `mapgen gameload`
  executes the game's own `readResurrectionMap → setCurrentMapInfo →
  Players::Init/SetStartGame → World::loadResurrectionMap` sequence: win bits
  come back as exactly `0x20` (WINS_GOLD, no WINS_ALL — normal victory
  correctly disabled), loss `0x800` (LOSS_TIME), gold 100000, days 56,
  compAlsoWins 0.
- **Runtime: OBSERVED.** The HEAD build was run with real HoMM2 assets: the
  map lists, starts, renders, and plays (movement, pickups, sawmill capture,
  sign, gazebo) with no crash; the Editor loads it. Screenshots in
  `validation/`.
- **Auto-playtest: still requires a human UI step** (launch attempt was
  aborted because the desktop was in active use). Workflow: Editor → load
  map → press A → disable Animation → OK.
- **Balance: modeled, not playtested.** `mapgen/economy_model.py` documents
  the tuned curve; guard constants are centralized in `kings_ransom_map.cpp`
  for one-line retuning after human playtests.
