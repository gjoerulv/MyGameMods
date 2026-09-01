# fheroes2 FH2M research — Map-level fields, players, metadata structs, victory/loss conditions

Source: local clone at `C:/Users/gjoer/source/repos/fheroes2`, upstream HEAD `b086d1aa8b921163712aec2fb8188f4d0d375b09` (2026-09-01).
All paths below are relative to `src/fheroes2/` unless stated otherwise.

---

## 1. File container facts (map_format_info.cpp)

- Magic word: `{ 'h', '2', 'm', 'a', 'p', '\0' }` — 6 bytes at file start (`maps/map_format_info.cpp:101`, written at 837-839, checked at 788-792/815-819).
- `minFileSize = 512` bytes — any smaller file is rejected on load (`maps/map_format_info.cpp:103-105`, checks at 783-785 and 810-812).
- `minimumSupportedVersion = 2`, `currentSupportedVersion = 13` (`maps/map_format_info.cpp:107-110`). Saving always writes `currentSupportedVersion` (13) regardless of `map.version` (`saveToStream`, line 440).
- Streams are **big-endian**: `fileStream.setBigendian( true )` (776-777, 803-804, 830-831); the compressed inner stream too (`RWStreamBuf compressed; compressed.setBigendian( true )`, 492-493).
- File layout: magic word, then **plain (uncompressed) BaseMapFormat**, then a single **zlib blob** (`Compression::zipData(..., false)`) containing everything else (`saveToStream(MapFormat)`, 484-504). Load side: `Compression::unzipData` (506-536).
- Serialization primitives (`src/engine/serialize.h`): `std::string` ← `operator<<(std::string_view)`; `std::vector`/`std::list`/`std::set`/`std::map`/`std::array` are all written as `put32(size)` followed by elements (lines 369-417); `std::array` also writes its size (line 409-417) and read fails if size differs (284-299). Enums are written as their underlying type (344-348). `std::pair` = first then second (350-354).
- `Funds` serialization (`resource/resource.cpp:649-652`):
  `stream << res.wood << res.mercury << res.ore << res.sulfur << res.crystal << res.gems << res.gold` — 7 × int32 (fields declared `int32_t wood{0}; mercury; ore; sulfur; crystal; gems; gold` at `resource/resource.h:127-133`).

## 2. BaseMapFormat — declaration order + defaults (maps/map_format_info.h:345-387)

```cpp
struct BaseMapFormat
{
    uint16_t version{ 1 };
    bool isCampaign{ false };
    uint8_t difficulty{ 1 };                       // "Normal difficulty."
    PlayerColorsSet availablePlayerColors{ 0 };    // uint8_t bitmask
    PlayerColorsSet humanPlayerColors{ 0 };
    PlayerColorsSet computerPlayerColors{ 0 };
    std::vector<PlayerColorsSet> alliances;
    std::array<uint8_t, 6> playerRace{ 0 };        // "Only 6 players are allowed per map."
    uint8_t victoryConditionType{ 0 };
    bool isVictoryConditionApplicableForAI{ false };
    bool allowNormalVictory{ false };
    std::vector<uint32_t> victoryConditionMetadata;
    uint8_t lossConditionType{ 0 };
    std::vector<uint32_t> lossConditionMetadata;
    int32_t width{ 0 };                            // square maps: height == width
    fheroes2::SupportedLanguage mainLanguage{ fheroes2::SupportedLanguage::English };  // uint8 enum
    std::string name;
    std::string description;
    std::string creatorNotes;                      // editor-only info (v>=9)
    std::map<fheroes2::SupportedLanguage, TranslationBaseMapMetadata> translations;    // v>=11
};
```

**Serialization order** (`saveToStream`, map_format_info.cpp:438-446):
`currentSupportedVersion, isCampaign, difficulty, availablePlayerColors, humanPlayerColors, computerPlayerColors, alliances, playerRace, victoryConditionType, isVictoryConditionApplicableForAI, allowNormalVictory, victoryConditionMetadata, lossConditionType, lossConditionMetadata, width, mainLanguage, name, description, creatorNotes, translations`.
Load (448-482) is identical; `width <= 0` ⇒ fail; `creatorNotes` only if version ≥ 9; `translations` only if version ≥ 11.

**MapFormat** (map_format_info.h:389-422), inner compressed section order (saveToStream 495-497):
`additionalInfo (vector<uint32_t>), tiles (vector<TileInfo>), dailyEvents (vector<DailyEvent>), rumors (vector<string>), castleMetadata (map<uint32,CastleMetadata>), heroMetadata, sphinxMetadata, signMetadata, adventureMapEventMetadata, selectionObjectMetadata, capturableObjectsMetadata, monsterMetadata, artifactMetadata, resourceMetadata, translationInfo`.
Load side (538-583): after tiles, `map.tiles.size()` must equal `width*width` else fail; `capturableObjectsMetadata` only if version > 8; `monsterMetadata/artifactMetadata/resourceMetadata` only if version > 9 (else converted from deprecated `StandardObjectMetadata`); `translationInfo` if version ≥ 11.

All metadata maps are keyed by **object UID** (`TileObjectInfo::id`).

Char limits (map_format_info.h:59-60): `messageCharLimit = 999`, `nameCharLimit = 30` (the latter is for castle/hero names, not the map name).

## 3. Player colors, races, alliances

### PlayerColor bitmask (`kingdom/color.h:44-58`)
```cpp
enum class PlayerColor : uint8_t {
    NONE = 0x00, BLUE = 0x01, GREEN = 0x02, RED = 0x04,
    YELLOW = 0x08, ORANGE = 0x10, PURPLE = 0x20, UNUSED = 0x80,
};
using PlayerColorsSet = std::underlying_type_t<PlayerColor>; // uint8_t
```
Header warns: "Do NOT change the order of the items as they are used for the map format."
`map_format_helper.cpp:1338-1343` static_asserts BLUE==1<<0 … PURPLE==1<<5.
Color index mapping (`kingdom/color.cpp:149-169`): index 0→BLUE, 1→GREEN, 2→RED, 3→YELLOW, 4→ORANGE, 5→PURPLE, anything else→NONE. `maxNumOfPlayers = 6` (`system/players.h:48`).

- `availablePlayerColors` — bitmask of kingdoms present on the map. **Derived, not authored**: `Maps::updateMapPlayers()` recomputes it from objects on tiles (`maps/map_format_helper.cpp:1421-1427`): a color is available iff it owns at least one hero or town.
- `humanPlayerColors` / `computerPlayerColors` — bitmasks; `updateMapPlayers` masks both by `availablePlayerColors` (1450-1451), then any available color not assigned to either becomes both human+computer (1455-1462), and if `humanPlayerColors == 0`, the first available color is made human (1464-1473).
- Invariants asserted at load into FileInfo (`maps/maps_fileinfo.cpp:432-434`):
  `(available & human) == human`, `(available & computer) == computer`, `(available & (human|computer)) == available`.
- `alliances` — `std::vector<PlayerColorsSet>`; used **only** with `VICTORY_DEFEAT_OTHER_SIDE`; exactly 2 alliances supported (`maps_fileinfo.cpp:507-514` asserts `map.alliances.size() == 2` and each set ⊆ availablePlayerColors). `updateMapPlayers` sanitizes alliances (1475-1527): dedup colors, drop empty alliances, clear if only 1 alliance remains, add missing colors to the last alliance; if alliances end up empty while victory type is 4, the victory type is reset to 0 (1537-1540).

### playerRace (`race.h:28-42`, semantics in map_format_helper.cpp)
```cpp
enum { NONE=0x00, KNGT=0x01, BARB=0x02, SORC=0x04, WRLK=0x08,
       WZRD=0x10, NECR=0x20, MULT=0x40, RAND=0x80, ALL=0x3F };
```
`Race::IndexToRace` (`kingdom/race.cpp:150-174`): 0→KNGT, 1→BARB, 2→SORC, 3→WRLK, 4→WZRD, 5→NECR, 6→MULT, 7→RAND, else NONE.
`playerRace` is `std::array<uint8_t, 6>` indexed by color index (0=Blue … 5=Purple), storing the **race bit value** (not index). It is also **derived** by `updateMapPlayers` (map_format_helper.cpp:1420-1448): OR of races of that color's heroes/towns; if RAND bit present ⇒ RAND; if MULT bit present or >1 race bit ⇒ MULT.
Random race handling at FileInfo load (`maps_fileinfo.cpp:522-526`): every color whose race == `Race::RAND` gets its bit set in `colorsOfRandomRaces`, which drives "Allow change race" in game setup (`AllowChangeRace`, maps_fileinfo.h:140-143).
At world load, a random-race town gets `Race::Rand()` when neutral or the kingdom's chosen race otherwise (`world/world_loadmap.cpp:804-817`); a random-race hero gets its kingdom's race (855-868); a jailed RAND hero gets `Race::Rand()` (995-997).

### Difficulty (`game/difficulty.h:39-46`)
```cpp
enum DifficultyLevel : int { EASY /*0*/, NORMAL /*1*/, HARD /*2*/, EXPERT /*3*/, IMPOSSIBLE /*4*/ };
```
("Do NOT change the order … used for the map format.") The editor only offers EASY..EXPERT (`editor/editor_map_specs_window.cpp:1986-2024`). BaseMapFormat default `difficulty{1}` = NORMAL.

## 4. Victory / loss condition enums and metadata

Enums (`maps/maps_fileinfo.h:160-176`):
```cpp
enum VictoryCondition : uint8_t {
    VICTORY_DEFEAT_EVERYONE = 0, VICTORY_CAPTURE_TOWN = 1, VICTORY_KILL_HERO = 2,
    VICTORY_OBTAIN_ARTIFACT = 3, VICTORY_DEFEAT_OTHER_SIDE = 4, VICTORY_COLLECT_ENOUGH_GOLD = 5 };
enum LossCondition : uint8_t {
    LOSS_EVERYTHING = 0, LOSS_TOWN = 1, LOSS_HERO = 2, LOSS_OUT_OF_TIME = 3 };
```

Metadata layouts (from `Maps::FileInfo::loadResurrectionMap`, maps_fileinfo.cpp:446-520):

| Condition | victoryConditionMetadata / lossConditionMetadata layout |
|---|---|
| VICTORY_DEFEAT_EVERYONE (0) | empty (assert); loader forces `compAlsoWins = true; allowNormalVictory = true` (481-482) |
| VICTORY_CAPTURE_TOWN (1) | `[tileIndex, colorBit]` — 2 entries; colorBit = PlayerColor bit value; assert colorBit ⊆ availablePlayerColors (492) |
| VICTORY_KILL_HERO (2) | `[tileIndex, colorBit]` — 2 entries |
| VICTORY_OBTAIN_ARTIFACT (3) | `[artifactId]` — 1 entry; **0 means "any Ultimate Artifact"** (editor_map_specs_window.cpp:829-831, 985) |
| VICTORY_DEFEAT_OTHER_SIDE (4) | metadata unused; uses `alliances` (must be exactly 2) |
| VICTORY_COLLECT_ENOUGH_GOLD (5) | `[goldAmount]` — 1 entry; assert `>= 10000`; converted to legacy params via `/1000` (504-505) |
| LOSS_EVERYTHING (0) | empty |
| LOSS_TOWN (1) | `[tileIndex, colorBit]`; assert colorBit ⊆ **humanPlayerColors** (462) |
| LOSS_HERO (2) | `[tileIndex, colorBit]` (same human-colors assert) |
| LOSS_OUT_OF_TIME (3) | `[days]` — 1 entry; assert `> 0` (465-466) |

`tileIndex` is a linear index; converted to (x, y) as `% map.width` / `/ map.width` (460-461, 490-491).

Mapping to runtime bits (`ConditionWins`, maps_fileinfo.cpp:602-624): 0→`WINS_ALL`; 1→`WINS_TOWN(|WINS_ALL if allowNormalVictory)`; 2→`WINS_HERO(|ALL)`; 3→`WINS_ARTIFACT(|ALL)`; 4→`WINS_SIDE`; 5→`WINS_GOLD(|ALL)`. `ConditionLoss` (626-644): 0→`LOSS_ALL`, 1→`LOSS_TOWN`, 2→`LOSS_HERO`, 3→`LOSS_TIME`.
GameOver bits (`game/game_over.h:44-68`): WINS_ALL=0x1, WINS_TOWN=0x2, WINS_HERO=0x4, WINS_ARTIFACT=0x8, WINS_SIDE=0x10, WINS_GOLD=0x20; LOSS_ALL=0x100, LOSS_TOWN=0x200, LOSS_HERO=0x400, LOSS_TIME=0x800, LOSS_ENEMY_WINS_TOWN=0x10000, LOSS_ENEMY_WINS_ARTIFACT=0x20000, LOSS_ENEMY_WINS_GOLD=0x40000.

### Field names in MapFormat (question 3)
- `uint8_t victoryConditionType{0}` (map_format_info.h:363)
- `bool isVictoryConditionApplicableForAI{ false }` (h:364) → becomes `FileInfo::compAlsoWins` (maps_fileinfo.cpp:443)
- `bool allowNormalVictory{ false }` (h:365) → `FileInfo::allowNormalVictory` (444)
- Serialized in that order right after playerRace (map_format_info.cpp:440-443).
- `WinsCompAlsoWins()` (maps_fileinfo.cpp:646-649): `compAlsoWins && ( (WINS_TOWN | WINS_GOLD) & ConditionWins() )` — i.e. "AI can also win" only matters for capture-town and gold conditions.
- Editor semantics (`editor/editor_map_specs_window.cpp:939-1018` `updateCondition`):
  - DEFEAT_EVERYONE: metadata cleared; both flags forced false (in file; loader re-forces true at play).
  - CAPTURE_TOWN: allowNormalVictory = checkbox; applicableForAI = checkbox **only when target town is neutral** (`_townToCapture.second == PlayerColor::NONE`), else false (962). Town candidates offered: computer-only colors (`computerPlayerColors & ~humanPlayerColors`) plus neutral (776).
  - KILL_HERO: both flags forced false; hero candidates: computer-only colors (805).
  - OBTAIN_ARTIFACT: applicableForAI=false, allowNormalVictory = checkbox; artifactId 0 = any ultimate (985).
  - DEFEAT_OTHER_SIDE: both flags false; writes `alliances`.
  - COLLECT_ENOUGH_GOLD: **both** checkboxes honored (1001-1003). Gold selector: `ValueSelectionDialogElement{ min=10000, max=1000000, current=10000, step=1000 }` (line 1456; ctor signature `(minimum, maximum, current, step, offset)` in `gui/ui_dialog.h:368`).

### Gold victory runtime check — current treasury, not cumulative
`World::KingdomIsWins` case `GameOver::WINS_GOLD` (`world/world.cpp:1003-1005`):
```cpp
return ( ( kingdom.isControlHuman() || mapInfo.WinsCompAlsoWins() ) && 0 < kingdom.GetFunds().Get( Resource::GOLD )
         && static_cast<uint32_t>( kingdom.GetFunds().Get( Resource::GOLD ) ) >= mapInfo.getWinningGoldAccumulationValue() );
```
`getWinningGoldAccumulationValue() = victoryConditionParams[0] * 1000` (maps_fileinfo.h:115-118) and `victoryConditionParams[0] = metadata[0] / 1000` (maps_fileinfo.cpp:505) — so the effective threshold is `(goldMetadata / 1000) * 1000` (integer division; e.g. 25500 → 25000). It compares the **current kingdom treasury** (`GetFunds()`), not cumulative income.

### LOSS_OUT_OF_TIME boundary
`World::KingdomIsLoss` case `GameOver::LOSS_TIME` (`world/world.cpp:1066-1067`):
```cpp
return ( CountDay() > conf.getCurrentMapInfo().LossCountDays() );
```
`LossCountDays()` returns `lossConditionParams[0]` = metadata[0] (maps_fileinfo.h:130-133; maps_fileinfo.cpp:468).
`CountDay()` returns `_day` (`world/world.h:386-389`); `_day` starts at 0 (world.h:566, world.cpp:328) and `World::NewDay()` pre-increments it (`++_day`, world.cpp:427) before the first turn, so **day 1 is `_day == 1`**. Defeat check runs in `GameOver::Result::checkGameOver()` at the start of a human turn (`game/game_startgame.cpp:1065` etc.).
**Confirmed: with metadata `[56]`, day 56 is fully playable (56 > 56 is false); the loss triggers at the start of day 57.** Weeks are 7 days, months 4 weeks (`world/world.h:68-71`), so 56 = end of month 2.

### Special-condition object validation
- At world load (`world/world_loadmap.cpp:1364-1395`): LOSS_HERO requires a hired hero at tile `lossConditionMetadata[0]`; LOSS_TOWN requires a castle there; VICTORY_KILL_HERO / VICTORY_CAPTURE_TOWN likewise — otherwise map load **fails**.
- On editor save (`Maps::updateMapPlayers`, map_format_helper.cpp:1548-1641): CAPTURE_TOWN/KILL_HERO/LOSS_TOWN/LOSS_HERO metadata is checked to point at an object of the right group with the right color; on mismatch the condition is silently reset to DEFEAT_EVERYONE / LOSS_EVERYTHING and metadata cleared.
- Hero color for condition check derives from object index: `1 << heroObjects[object.index].metadata[0]` (1587); town color from flag objects via `getTownColorIndex` (1573).

## 5. CastleMetadata (map_format_info.h:62-107)

Declaration order (== serialization order, map_format_info.cpp:626-636):
```cpp
std::string customName;                                  // empty ⇒ engine picks a random name
std::array<int32_t, 5> defenderMonsterType{ 0 };         // < 0 ⇒ default units (neutral), 0 ⇒ empty slot
std::array<int32_t, 5> defenderMonsterCount{ 0 };
bool customBuildings{ false };
std::vector<uint32_t> builtBuildings;                    // BUILD_* bit values
std::vector<uint32_t> bannedBuildings;
std::map<uint8_t, int32_t> mustHaveSpells;               // key: tens=guild level 0-4, units=slot 0-4
std::vector<int32_t> bannedSpells;
std::array<int32_t, 6> availableToHireMonsterCount{ -1 };// NOTE: aggregate init ⇒ only [0] == -1, [1..5] == 0
```
**Caveat:** `availableToHireMonsterCount` is serialized but currently **unused anywhere at runtime or in the editor** (whole-tree grep hits only map_format_info.h/.cpp). Comment says "negative value ⇒ no change".
`operator<<`: `customName, defenderMonsterType, defenderMonsterCount, customBuildings, builtBuildings, bannedBuildings, mustHaveSpells, bannedSpells, availableToHireMonsterCount`.

### BUILD_* bit values (`castle/castle.h:70-108`, enum `BuildingType : uint32_t`)
```
BUILD_NOTHING      0x00000000   BUILD_THIEVESGUILD 0x00000001   BUILD_TAVERN     0x00000002
BUILD_SHIPYARD     0x00000004   BUILD_WELL         0x00000008   BUILD_STATUE     0x00000010
BUILD_LEFTTURRET   0x00000020   BUILD_RIGHTTURRET  0x00000040   BUILD_MARKETPLACE 0x00000080
BUILD_WEL2         0x00000100   BUILD_MOAT         0x00000200   BUILD_SPEC       0x00000400
BUILD_CASTLE       0x00000800   BUILD_CAPTAIN      0x00001000   BUILD_SHRINE     0x00002000
BUILD_MAGEGUILD1   0x00004000   BUILD_MAGEGUILD2   0x00008000   BUILD_MAGEGUILD3 0x00010000
BUILD_MAGEGUILD4   0x00020000   BUILD_MAGEGUILD5   0x00040000   BUILD_TENT       0x00080000
DWELLING_MONSTER1  0x00100000 … DWELLING_MONSTER6  0x02000000
DWELLING_UPGRADE2  0x04000000   UPGRADE3 0x08000000  UPGRADE4 0x10000000
DWELLING_UPGRADE5  0x20000000   UPGRADE6 0x40000000  UPGRADE7 0x80000000  (UPGRADE7 = Black Dragons, WRLK only)
```

### Castle-vs-town and defaults
- Editor: placing a KINGDOM_TOWNS object creates metadata with `builtBuildings = [ objects[index].metadata[1] == 0 ? BUILD_TENT : BUILD_CASTLE ]` (`maps/map_format_helper.cpp:1109-1117`). So a "castle" variant object has BUILD_CASTLE; a "town" variant has BUILD_TENT. `isCastle()` = `_constructedBuildings & BUILD_CASTLE` (castle.h:180). World-load asserts BUILD_CASTLE/BUILD_TENT presence matches the object graphic (`world/world_loadmap.cpp:819-822`).
- Neutral towns placed by the editor also get `setDefaultCastleDefenderArmy` — all `defenderMonsterType = -1` (default army) (`editor/editor_interface.cpp:3323-3326`; helper at map_format_helper.cpp:1759-1767).
- `Castle::loadFromResurrectionMap` (`castle/castle.cpp:397-426`):
  - `_constructedBuildings = OR of builtBuildings` (`getBuildingsFromVector`, map_format_helper.cpp:1749-1757);
  - `if ( !metadata.customBuildings ) _setDefaultBuildings();`
  - `_disabledBuildings = OR of bannedBuildings`;
  - custom army iff any `defenderMonsterType >= 0` slot is set (`loadCastleArmy` → `isDefaultCastleDefenderArmy` = all types < 0; map_format_helper.cpp:1769-1782);
  - custom name if non-empty; mage guild initialized with mustHaveSpells/bannedSpells.
- `_setDefaultBuildings` (`castle/castle.cpp:537-562`): always adds `DWELLING_MONSTER1`; adds `DWELLING_MONSTER2` with probability by game difficulty: EASY 75%, NORMAL 50%, HARD 25%, EXPERT 10% (`dwelling2 >= Rand::Get(1,100)`).
- `_postLoad` (castle.cpp:428-535): strips race-impossible dwelling upgrades; fills dwellings with `GetGrown()` populations; sets captain if BUILD_CAPTAIN; **neutral (gray) towns without custom army get 4 rounds of `_joinRNDArmy()` reinforcements** (508-512); removes shipyard if no sea access (514-517); NECR: tavern → shrine (519-527).
- So a brand-new town placed in the editor and never customized starts in game with: TENT (or CASTLE), DWELLING_MONSTER1, maybe DWELLING_MONSTER2 (difficulty roll), plus race defaults above.

## 6. HeroMetadata (map_format_info.h:109-174)

Declaration order (fields + defaults):
```cpp
std::string customName;                       // empty ⇒ engine name
int32_t customPortrait{ 0 };                  // <=0 ⇒ none; else portrait/hero id
std::array<int32_t, 5> armyMonsterType{ 0 };  // 0 = not set
std::array<int32_t, 5> armyMonsterCount{ 0 };
std::array<int32_t, 14> artifact{ 0 };        // 0 = not set
std::array<int32_t, 14> artifactMetadata{ 0 };// spell id for SPELL_SCROLL entries
std::vector<int32_t> availableSpells;         // spell book content when MAGIC_BOOK is in artifacts
bool isOnPatrol{ false };                     // AI heroes only
uint8_t patrolRadius{ 0 };
std::array<int8_t, 8> secondarySkill{ 0 };    // 0 = not set
std::array<uint8_t, 8> secondarySkillLevel{ 0 };
int16_t customLevel{ -1 };                    // mutually exclusive with customExperience
int32_t customExperience{ -1 };
int16_t customAttack{ -1 };                   // -1 ⇒ default
int16_t customDefense{ -1 };
int16_t customKnowledge{ -1 };
int16_t customSpellPower{ -1 };
int16_t magicPoints{ -1 };                    // -1 ⇒ default (max)
uint8_t race{ 0 };                            // Race bit value; RAND allowed
```
**Serialization order differs from declaration order** (map_format_info.cpp:638-652):
`customName, customPortrait, armyMonsterType, armyMonsterCount, artifact, artifactMetadata, availableSpells, isOnPatrol, patrolRadius, secondarySkill, secondarySkillLevel, customLevel, customExperience, customAttack, customDefense, customKnowledge, customSpellPower, magicPoints, race`.

### Color/race binding for placed heroes
Hero **color and type come from the object definition, not the metadata** (comment at h:111-112). At world load (`world/world_loadmap.cpp:845-892`): `heroObjects[object.index].metadata[0]` = color index (0-5 → PlayerColor via `Color::IndexToColor`), `metadata[1]` = race index; assert `heroInfo.race == Race::IndexToRace(metadata[1])` (858); heroes cannot be neutral (861). The editor sets `heroMetadata.race` from the object when placing (`map_format_helper.cpp:1100-1108`). Jail heroes are KINGDOM-less: ADVENTURE_MISCELLANEOUS OBJ_JAIL also stores a HeroMetadata (editor default race = `Race::RAND`, map_format_helper.cpp:1118-1124); at load, color = NONE, RAND race resolved by `Race::Rand()` (world_loadmap.cpp:984-1010).
Application: `Heroes::applyHeroMetadata( metadata, isInJail, isEditor )` (`heroes/heroes.cpp:600-…`): custom army via `loadHeroArmy` (any type > 0); artifacts (SPELL_SCROLL uses artifactMetadata spell; MAGIC_BOOK triggers spell book + availableSpells); custom secondary skills replace defaults; patrol; `customExperience > -1` sets experience, else `customLevel > -1` triggers level-ups; primary skill overrides when `> -1`; `magicPoints < 0` ⇒ max spell points.
A hero standing on the tile below a castle entrance is moved into the castle at game start (`Kingdom::ApplyPlayWithStartingHero`, kingdom/kingdom.cpp:504-541). `startWithHeroInFirstCastle` is MP2-only (stays false for FH2M; maps_fileinfo.cpp Reset:177).

## 7. Other metadata structs

### SphinxMetadata (h:176-200; serialization cpp:654-662)
`std::string riddle; std::vector<std::string> answers; int32_t artifact{0}; int32_t artifactMetadata{0}; Funds resources;` — serialized in that order. Runtime (world_loadmap.cpp:1048-1086): empty answers asserted against; `artifact == Artifact::SPELL_SCROLL` ⇒ `SetSpell(artifactMetadata)`; `isTruncatedAnswer = false` (fheroes2 doesn't truncate answers to 4 chars like OG).

### SignMetadata (h:202-205; cpp:664-672)
`std::string message;` only. Used for OBJ_SIGN (ADVENTURE_MISCELLANEOUS) and OBJ_BOTTLE (ADVENTURE_WATER) (world_loadmap.cpp:1027-1047, 1122-1144). Empty message ⇒ default message; else `message.language = map.mainLanguage`.

### AdventureMapEventMetadata (h:207-254; cpp:674-686)
Declaration+serialization order:
```cpp
std::string message;
PlayerColorsSet humanPlayerColors{ 0 };
PlayerColorsSet computerPlayerColors{ 0 };
bool isRecurringEvent{ false };
int32_t artifact{ 0 };
int32_t artifactMetadata{ 0 };
Funds resources;
int16_t attack{0}; int16_t defense{0}; int16_t knowledge{0}; int16_t spellPower{0};
int32_t experience{ 0 };
uint8_t secondarySkill{ 0 }; uint8_t secondarySkillLevel{ 0 };
int32_t monsterType{ 0 }; int32_t monsterCount{ 0 };
```
Runtime (world_loadmap.cpp:946-983): colors masked by map human/computer colors; event dropped if it applies to nobody in the actual game; `isSingleTimeEvent = !isRecurringEvent`. NOTE: as of this commit the runtime `MapEvent` consumes resources/artifact/colors/secondarySkill/experience/message; attack/defense/knowledge/spellPower/monsterType/monsterCount are not copied into MapEvent in world_loadmap (fields exist in format for the editor).

### SelectionObjectMetadata (h:256-259; cpp:688-696)
`std::vector<int32_t> selectedItems;` — used for: Witch's Hut (allowed secondary skills), Shrine 1st/2nd/3rd circle (spells of level 1/2/3), Pyramid (level-5 spells) (world_loadmap.cpp:1012-1026, 1087-1113, 1232-1263). Runtime picks `Rand::Get(selectedItems)` after validating every entry; invalid set ⇒ default behavior.

### CapturableObjectMetadata (h:261-264; cpp:698-706)
`PlayerColor ownerColor{ 0 };` (single uint8). Applies to OBJ_ALCHEMIST_LAB, OBJ_LIGHTHOUSE, OBJ_MINE, OBJ_SAWMILL (`isCapturableObject`, map_format_helper.cpp:1724-1737; applied via `captureObject` at world load for ADVENTURE_MINES and ADVENTURE_MISCELLANEOUS default case, world_loadmap.cpp:1114-1117, 1269-1275). `updateMapPlayers` deletes ownership metadata whose color is not in `availablePlayerColors` (map_format_helper.cpp:1643-1665).

### MonsterMetadata (h:266-278; cpp:708-716)
```cpp
int32_t count{ 0 };                 // 0 ⇒ random size at game start
int32_t joinCondition{ 0 };         // reserved, unused
bool isWeeklyGrowthDisabled{ false };// reserved, unused
std::vector<int32_t> selected;      // only for OBJ_RANDOM_MONSTER* objects
```
Runtime: `tileData[0] = count` (world_loadmap.cpp:894-902); for random monster objects, `selected` is filtered to valid monsters of the correct level and `tileData[1] = Rand::Get(selected)` (904-939).
`setInitialObjectInfo` → `OBJ_MONSTER` (maps_tiles_helper.cpp:1598-1602) → `setMonsterOnTile(tile, mons, tile.metadata()[0])` (1727-1781):
- `count > 0` ⇒ exact count, and the troop is always JOIN_CONDITION_MONEY ("map-designer counts are always hostile", 1766-1770).
- `count == 0` ⇒ `mons.GetRNDSize()` (`monster/monster.cpp:165-220`): per-monster base (`defaultArmySizePerLevel[7] = {0,50,30,25,25,12,8}` by level; outliers: PEASANT 80, ROGUE 40, PIKEMAN/VET_PIKEMAN/WOLF/ELF/GRAND_ELF 30, GARGOYLE 25, GHOST/MEDUSA 20, MINOTAUR(+KING)/ROC/VAMPIRE(+LORD)/UNICORN 16, CAVALRY/CHAMPION 18, PALADIN/CRUSADER/CYCLOPS/PHOENIX 12), final count = `Rand::Get(result/2, result)`. Then 20% chance JOIN_CONDITION_FREE, else MONEY (1772-1780); Ghost/elementals always SKIP.
- Weekly growth (`World::NewWeek`, world/world.cpp:462-477, from week 2 onward): `updateMonstersOnTile` (maps_tiles_helper.cpp:1697-1725): `bonusUnit = (Rand::Get(1,7) <= count % 7) ? 1 : 0; growth = count/7 + bonusUnit;` capped at limit; limit = `GameStatic::getNeutralMonsterLimit(isResurrectionMap)` = **4,000,000 for FH2M** maps, 4000 for OG maps (`game/game_static.cpp:310-320`).

### ArtifactMetadata (h:280-290; cpp:718-726)
```cpp
int32_t radius{ 0 };            // only Random Ultimate Artifact
int32_t captureCondition{ 0 };  // reserved, unused
std::vector<int32_t> selected;  // random artifacts and Spell Scroll
```
Runtime (world_loadmap.cpp:1145-1210): for OBJ_RANDOM_ARTIFACT[_TREASURE/_MINOR/_MAJOR]/OBJ_RANDOM_ULTIMATE_ARTIFACT, `selected` filtered by level, `tileData[1] = Rand::Get(selected)`; random ultimate also `tileData[0] = radius`. Spell Scroll artifact: `selected` must have exactly 1 entry > 0; `tileData[0] = selected[0] - 1` (spells are 0-based in OG format; 1202-1209). Ultimate placement: suitable tile picked within `radius` of the object tile, at least 9 tiles from map edge (`ultimateArtifactOffset = 9`, world_loadmap.cpp:77, 1627-1731); radius 0 ⇒ exact tile.

### ResourceMetadata (h:292-295; cpp:728-736)
`int32_t count{ 0 };` only. Applied only for `OBJ_RESOURCE` in ADVENTURE_TREASURES: `tileData[0] = resource type (objectInfo.metadata[0]); tileData[1] = count` (world_loadmap.cpp:1211-1226; tolerates missing metadata on corrupted maps).
`setInitialObjectInfo` OBJ_RESOURCE (maps_tiles_helper.cpp:1153-1205): if `(metadata[0] & Resource::ALL) != 0 && metadata[1] > 0` ⇒ keep authored value; **else random**: GOLD `100 * Rand::Get(5,10)` = 500–1000; WOOD/ORE `Rand::Get(5,10)`; MERCURY/SULFUR/CRYSTAL/GEMS `Rand::Get(3,6)`. So `count = 0` ⇒ random. (Resource type bits: WOOD=0x01, MERCURY=0x02, ORE=0x04, SULFUR=0x08, CRYSTAL=0x10, GEMS=0x20, GOLD=0x40; resource.h:49-62.)

### DailyEvent (h:297-312; cpp:609-619)
Declaration+serialization order:
```cpp
std::string message;
PlayerColorsSet humanPlayerColors{ 0 };
PlayerColorsSet computerPlayerColors{ 0 };
uint32_t firstOccurrenceDay{ 1 };
uint32_t repeatPeriodInDays{ 0 };  // 0 = never repeats
Funds resources;
```
Runtime (world_loadmap.cpp:1327-1355): events with `firstOccurrenceDay == 0` skipped; colors masked by map + actual human/computer assignment; dropped if nobody remains; mapped into `EventDate { resource, firstOccurrenceDay, repeatPeriodInDays, colors = human|computer, isApplicableForAIPlayers = (computerColors != 0), message }` (`world/world.h:171-190`).
Trigger check `EventDate::isAllow(color, date)` (world/world.cpp:1647-1669): color in mask; `firstOccurrenceDay > date` ⇒ no; exact first day ⇒ yes; else repeats iff `((date - firstOccurrenceDay) % repeatPeriodInDays) == 0`. Applied during `Kingdom::ActionNewDayResourceUpdate` (`kingdom/kingdom.cpp:225-246`) — resources added via `Resource::CalculateEventResourceUpdate` (funds can be negative; treasury adjusted), AI players only get it if `isApplicableForAIPlayers`. Non-repeating past events removed on NewDay (world.cpp:456-459).
Editor limits (`editor/editor_daily_event_spec_window.cpp:60-76, 172, 200`): firstOccurrenceDay clamped to [1, 33600] (= 100 game years, 336 days/yr), repeatPeriodInDays [0, 336]. Message limit `messageCharLimit` (999).

### Rumors
`MapFormat::rumors` = `std::vector<std::string>` (h:398). Loaded into `World::_customRumors` skipping empty strings (world_loadmap.cpp:1357-1362). Editor: max rumor length 200 chars (`longestRumor{200}`, editor_rumor_window.cpp:56, 235); duplicates rejected; empty rumors removed.

### Translations (v11+)
`TranslationBaseMapMetadata { name, description, creatorNotes }` (h:321-326); `TranslationFormat { dailyEvents(vector<string>), rumors, castleMetadata(map<uint32,string>), heroMetadata, sphinxMetadata(map<uint32,TranslationSphinxMetadata{riddle,answers}>), signMetadata, adventureMapEventMetadata }` (h:328-343; serialization cpp:738-768). `BaseMapFormat.translations` holds name/description/notes per language; `MapFormat.translationInfo` holds in-game texts per language. At play, `Maps::setInGameLanguage/loadTranslation` picks the current game language, falling back to English, then the map's first language (maps_fileinfo.cpp:393-403; world_loadmap.cpp:722-730).

## 8. mainLanguage
`fheroes2::SupportedLanguage : uint8_t` (`game/game_language.h:27-54`): `English = 0`, then in order French=1, Polish=2, German=3, Russian=4, Italian=5, Czech=6, Spanish=7, Belarusian=8, Bulgarian=9, Danish=10, Dutch=11, Esperanto=12, Greek=13, Hungarian=14, Norwegian=15, Portuguese=16, Romanian=17, Slovak=18, Swedish=19, Turkish=20, Ukrainian=21, Vietnamese=22.
`BaseMapFormat.mainLanguage` — "the main language of the map. At the moment only one language is being supported" (h:374-375). Serialized as 1 byte. Drives the font/codepage used to render map texts (e.g. sign messages get `message.language = map.mainLanguage`, world_loadmap.cpp:1041). `FileInfo::getSupportedLanguage()` returns it only for RESURRECTION maps (maps_fileinfo.h:147-155).

## 9. FileInfo ingestion & scenario-list validity

`FileInfo::readResurrectionMap( path, isForEditor, currentLanguage )` (maps_fileinfo.cpp:384-416): `loadBaseMap` (only the uncompressed header!) → optional language selection → `loadResurrectionMap` (418-540) copies difficulty/width(=height)/name/description/colors/races/conditions/mainLanguage/translations/creatorNotes, sets `version = GameVersion::RESURRECTION` (=2; `maps_fileinfo.h:42-47`). **When not for editor, a map with `colorsAvailableForHumans == 0` is rejected** (409-413).
Scenario-list filter (`getValidMaps`, maps_fileinfo.cpp:64-122): reject if `Color::Count(HumanOnlyColors()) > humanPlayerCount` or `humanPlayerCount > humanOnly + compHumanColors`; if humanOnly == humanPlayerCount, the comp+human colors are stripped from human selection. FH2M files are listed only when PoL assets are present (`getAllMapFileInfos`, 730-756; extension `.fh2m`).

## 10. Editor save-path validation ("verifyMap")

There is **no function named `verifyMap`**. The save path is `EditorInterface::saveMapToFile` (`editor/editor_interface.cpp:3204-3273`):
1. `Maps::updateMapPlayers( _mapFormat )` must succeed (else "The map is corrupted."). This is the real sanitizer (recomputes availablePlayerColors/playerRace, fixes human/computer masks, sanitizes alliances, resets broken special win/loss conditions, prunes stale capturable ownership).
2. File dialog (`Editor::mapSaveSelectFile`), name limited to `maxMapNameLength = 50` (editor_interface.cpp:115); saved as `<name>.fh2m` under the fheroes2 data dir `maps/` folder.
3. `Maps::Map_Format::saveMap( fullPath, _mapFormat )`.
**No minimum castle/hero count is enforced to save.** Playability is only enforced when starting a game: `_prepareMapForGameplay` (editor_interface.cpp:3599-3629) requires the map to be saved and `colorsAvailableForHumans != 0` ("You need at least one human player…"). Similarly `world.loadResurrectionMap` fails when `availablePlayerColors == 0` (world_loadmap.cpp:746-749).
Map specs dialog (`Editor::mapSpecificationsDialog`, editor_map_specs_window.cpp:2029+): empty name replaced with "My Map" (2058-2060); name edit capped at maxMapNameLength=50 (2311); description and creatorNotes capped at `messageCharLimit` = 999 (2322, 2367); unknown condition types reset to defaults (2032-2039). Loss "days" selector: `ValueSelectionDialogElement{ 1, 10*336=3360, 28, 1 }` (line 1875) — i.e. 1..3360 days, default 28.

## 11. Misc cross-checks / gotchas for a generator

- `TileObjectInfo { uint32_t id; ObjectGroup group; uint32_t index; }` — `id` is the object UID keying all metadata maps; multi-tile objects (towns + their flags/basement) share one UID (editor uses `setLastObjectUID(uid-1)` trick, editor_interface.cpp:3304-3340).
- Town color is not stored in CastleMetadata: it comes from the **two LANDSCAPE_FLAGS objects with the same UID** on tiles entrance-1 and entrance+1; `flagObjects[index].metadata[0]` is the color index; both must match (`getTownColorIndex`, map_format_helper.cpp:1670-1717). Flag object index = `colorIndex*2` (left) and `colorIndex*2+1` (right) (editor_interface.cpp:3332-3338). Color index 6 = neutral.
- Metadata maps must exactly cover the corresponding objects — debug builds assert set equality between placed objects and metadata keys (world_loadmap.cpp:1279-1325). A generator must emplace (possibly default) CastleMetadata for every town, HeroMetadata for every hero/jail, MonsterMetadata for every monster, etc.
- `victoryConditionParams` in FileInfo are `uint16` — gold metadata beyond 65,535,000 would overflow the division; editor max is 1,000,000.
- `assert( map.victoryConditionMetadata[0] >= 10000 )` for gold (debug builds).
- `assert( map.lossConditionMetadata[0] > 0 )` for out-of-time (debug builds).
- VICTORY_DEFEAT_EVERYONE / LOSS_EVERYTHING must have **empty** metadata vectors (asserts, maps_fileinfo.cpp:452, 480).
- `LOSS_ENEMY_WINS_*`: a human player also loses when another player fulfills WINS_TOWN/ARTIFACT/GOLD (world.cpp:1121-1134).
- First day income is skipped (`world.CountDay() > 1` guard, kingdom.cpp:228).
