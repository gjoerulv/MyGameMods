# fheroes2 — Tile model, ObjectGroup enum, object info tables

Source of truth: local clone `C:/Users/gjoer/source/repos/fheroes2` @ `b086d1aa8b921163712aec2fb8188f4d0d375b09` (2026-09-01).
All line numbers refer to that revision.

---

## 1. TileInfo and TileObjectInfo

`src/fheroes2/maps/map_format_info.h:42-57`:

```cpp
struct TileObjectInfo
{
    uint32_t id{ 0 };

    ObjectGroup group{ ObjectGroup::NONE };

    uint32_t index{ 0 };
};

struct TileInfo
{
    uint16_t terrainIndex{ 0 };
    uint8_t terrainFlags{ 0 };

    std::vector<TileObjectInfo> objects;
};
```

Serialization (`src/fheroes2/maps/map_format_info.cpp:589-607`):

```cpp
OStreamBase & operator<<( OStreamBase & stream, const TileObjectInfo & object )
{
    return stream << object.id << object.group << object.index;
}
OStreamBase & operator<<( OStreamBase & stream, const TileInfo & tile )
{
    return stream << tile.terrainIndex << tile.terrainFlags << tile.objects;
}
```

(`ObjectGroup` is `uint8_t`-typed, so it serializes as 1 byte.)

Field meanings:

- **`terrainIndex`** (`uint16_t`): image index into `TIL::GROUND32`. The ground *type* is derived purely from the index range — `Maps::Ground::getGroundByImageIndex` (`src/fheroes2/maps/ground.cpp:61-94`) with the ranges from `src/fheroes2/maps/ground.h:50-62`:

  | Ground | start index | range |
  |---|---|---|
  | WATER | 0 | 0–29 |
  | GRASS | 30 | 30–91 |
  | SNOW | 92 | 92–145 |
  | SWAMP | 146 | 146–207 |
  | LAVA | 208 | 208–261 |
  | DESERT | 262 | 262–320 |
  | DIRT | 321 | 321–360 |
  | WASTELAND | 361 | 361–414 |
  | BEACH | 415 | 415–431 (MAX_IMAGE_INDEX = 432) |

  Ground id constants (bitmask, `ground.h:34-47`): `UNKNOWN=0, DESERT=0x01, SNOW=0x02, SWAMP=0x04, WASTELAND=0x08, BEACH=0x10, LAVA=0x20, DIRT=0x40, GRASS=0x80, WATER=0x100`.
  The first ~16 (water/dirt) or ~38 (others) indices of each range are transition images (`ground.cpp:96-117`); "plain" random fill images come after those.

- **`terrainFlags`** (`uint8_t`): flip bits. `map_format_helper.cpp:196`: `mapTile.terrainFlags = ( verticalFlip ? 1 : 0 ) + ( horizontalFlip ? 2 : 0 );` — bit0 = vertical flip, bit1 = horizontal flip.

- **`objects`**: list of objects whose **main tile** ((0,0) part) is this tile. Multi-tile objects are stored ONLY on their main tile; the other tiles they cover are reconstructed at load time from the object tables (see §7).

- **`TileObjectInfo.id`**: global object UID (must be > 0). On load, `readTileObject` (`map_format_helper.cpp:1028-1048`) does `setLastObjectUID( object.id - 1 )` and then `setObjectOnTile(...)`, which internally takes `getNewObjectUID()` — i.e. the placed world object gets exactly `id`. Composite placements (basement + town + left flag + right flag) deliberately reuse the SAME UID: the editor resets the UID counter before each component (`editor_interface.cpp:3304-3310, 3330, 3336`). Metadata maps in `MapFormat` (castleMetadata, heroMetadata, monsterMetadata, …) are keyed by this UID (`map_format_info.h:400-419`).

- **`TileObjectInfo.group` / `.index`**: coordinates into the static object tables: `getObjectInfo( group, index )`.

---

## 2. ObjectGroup enum — complete, with numeric values

`src/fheroes2/maps/map_object_info.h:127-168`. `enum class ObjectGroup : uint8_t`, no explicit initializers, so values are sequential from 0. Header comment: *"Do NOT change the order of the items as they are used for the map format."*

| value | enumerator |
|---|---|
| 0 | `NONE` (not populated; a tile object with this group is invalid) |
| 1 | `ROADS` |
| 2 | `STREAMS` |
| 3 | `LANDSCAPE_MOUNTAINS` |
| 4 | `LANDSCAPE_ROCKS` |
| 5 | `LANDSCAPE_TREES` |
| 6 | `LANDSCAPE_WATER` |
| 7 | `LANDSCAPE_MISCELLANEOUS` |
| 8 | `LANDSCAPE_TOWN_BASEMENTS` |
| 9 | `LANDSCAPE_FLAGS` |
| 10 | `ADVENTURE_ARTIFACTS` |
| 11 | `ADVENTURE_DWELLINGS` |
| 12 | `ADVENTURE_MINES` |
| 13 | `ADVENTURE_POWER_UPS` |
| 14 | `ADVENTURE_TREASURES` |
| 15 | `ADVENTURE_WATER` |
| 16 | `ADVENTURE_MISCELLANEOUS` |
| 17 | `KINGDOM_HEROES` |
| 18 | `KINGDOM_TOWNS` |
| 19 | `MONSTERS` |
| 20 | `MAP_EXTRAS` (boat directions; not placeable in editor UI) |
| 21 | `GROUP_COUNT` |

Debug invariants (`map_object_info.cpp:6115-6132`): every object in groups 1–9 (ROADS..LANDSCAPE_FLAGS) is a **non-action** object; every object in groups 10–20 (ADVENTURE_ARTIFACTS..MAP_EXTRAS) is an **action** object (`MP2::isOffGameActionObject`).

---

## 3. ObjectInfo model

`src/fheroes2/maps/map_object_info.h:42-115`:

```cpp
struct ObjectPartInfo
{
    // (ctor: icn, index, offset, type)
    fheroes2::Point tileOffset;      // tile offset from the MAIN object tile
    uint32_t icnIndex{ 0 };          // image index inside the ICN
    MP2::MapObjectType objectType{ MP2::OBJ_NONE }; // per-part type (shadows have OBJ_NONE)
    MP2::ObjectIcnType icnType{ MP2::OBJ_ICN_TYPE_UNKNOWN };
    uint8_t animationFrames{ 0 };    // number of FOLLOWING icn indices used as animation
};

struct LayeredObjectPartInfo final : public ObjectPartInfo
{
    ObjectLayerType layerType{ OBJECT_LAYER }; // passability + render order
};

struct ObjectInfo
{
    std::vector<LayeredObjectPartInfo> groundLevelParts; // NON-EMPTY; MAIN part FIRST
    std::vector<ObjectPartInfo> topLevelParts;           // only offsets with y < 0
    std::array<uint32_t, 2> metadata{ 0 };               // group-specific meaning
    MP2::MapObjectType objectType{ MP2::OBJ_NONE };
};
```

Layers (`src/fheroes2/maps/maps_tiles.h:50-55`):

```cpp
enum ObjectLayerType : uint8_t
{
    OBJECT_LAYER = 0,     // affects passability
    BACKGROUND_LAYER = 1, // affects passability, rendered as background
    SHADOW_LAYER = 2,     // no passability changes
    TERRAIN_LAYER = 3     // roads/streams/cracks; no passability changes
};
```

Occupied / used tiles (`map_object_info.cpp:6266-6294`):

```cpp
getGroundLevelOccupiedTileOffset -> offsets of parts on OBJECT_LAYER or BACKGROUND_LAYER
getGroundLevelUsedTileOffset     -> offsets of parts on any layer except SHADOW_LAYER
```

Max sizes (`map_object_info.h:33-37`): `maxActionGroundObjectDimensions{4,2}` and `maxObjectDimensions{8,5}` — asserted against actual tables in debug builds (`map_object_info.cpp:6158-6210`).

**Main part rule**: `groundLevelParts.front()` is the main/action part, always at offset {0,0}, and its `objectType == ObjectInfo::objectType` (debug assert `map_object_info.cpp:6084`). For action objects the main tile is the interaction tile. Secondary passability-blocking parts carry the corresponding `OBJ_NON_ACTION_*` type.

**MP2::MapObjectType mechanics** (`src/fheroes2/maps/mp2.h`): `OBJ_ACTION_OBJECT_TYPE = 128` (`mp2.h:267`); every action type = its non-action twin + 128, e.g. `OBJ_NON_ACTION_CASTLE = 35` (`mp2.h:173`) → `OBJ_CASTLE = 163` (`mp2.h:307`). Others used below: `OBJ_MINE`, `OBJ_ABANDONED_MINE`, `OBJ_SAWMILL`, `OBJ_ALCHEMIST_LAB`, `OBJ_HERO`, `OBJ_MONSTER`, `OBJ_ARTIFACT`, `OBJ_RESOURCE`, `OBJ_TREASURE_CHEST`, `OBJ_CAMPFIRE`, `OBJ_GENIE_LAMP`, `OBJ_RANDOM_*` — all `NON_ACTION + 128` (`mp2.h:273-336`).

---

## 4. Table construction machinery — determinism

`src/fheroes2/maps/map_object_info.cpp`:

- Global container: `std::array<std::vector<Maps::ObjectInfo>, GROUP_COUNT> objectData;` (line 42) plus `objectInfoByIcn` map (line 46) for reverse ICN lookup.
- `populateObjectData()` (6024-6214): lazily executed exactly once (guarded by `static bool isPopulated`). Comment at 6032-6033: **"The order of objects must be preserved. If you want to add a new object, add it to the end of the corresponding container."** All tables are built by straight-line code (fixed loops over fixed initializer lists), so ordering is fully deterministic and stable across runs/platforms; indices are a stable map-format contract.
- `getObjectsByGroup(group)` (6219-6226) → `objectData[group]` after populating.
- `getObjectInfo(group, index)` (6228-6238) → bounds-checked `objectInfo[objectIndex]`; returns a static empty `ObjectInfo{OBJ_NONE}` (with assert) if out of range.
- `getObjectPartByIcn(icnType, icnIndex)` (6240-6254) → reverse lookup, first-inserted wins.
- Group 0 (`NONE`) is never populated → empty vector.

---

## 5. Group-by-group tables

### 5.1 ROADS (group 1) — `populateRoads`, lines 48-1111

- `objects.resize(513)` (line 55). **Index semantics**: index 0–255 = bitmask of `Direction` bits of neighbouring tiles that contain roads; index 256–511 = the same but a second sprite variant; **index 512 = castle entrance road** (single part `OBJ_ICN_TYPE_ROAD` icn 31, line 57).
- Direction bits (`src/fheroes2/heroes/direction.h:35-47`): `TOP_LEFT=0x01, TOP=0x02, TOP_RIGHT=0x04, RIGHT=0x08, BOTTOM_RIGHT=0x10, BOTTOM=0x20, BOTTOM_LEFT=0x40, LEFT=0x80` (CENTER=0x100 never set in an index).
- Index computed by `getRoadObjectIndex` (`map_format_helper.cpp:858-882`): returns 512 if the tile directly above holds a castle entrance (any `KINGDOM_TOWNS` object), else `roadDirection + Rand::Get(1) * 256`.
- All parts are `MP2::OBJ_NONE` on `Maps::TERRAIN_LAYER`; parts spill onto neighbour tiles (offsets like {-1,0}, {1,1}) to draw connection stubs. Many indices alias each other via copy assignment (e.g. `objects[12] = objects[8]`). Indices 192-255 duplicate 128-191 (lines 1105-1110).
- Only one ROADS object may exist per tile; `updateRoadOnTile` (`map_format_helper.cpp:1848-1889`) rewrites `iter->index` in place when neighbours change. A road generator must recompute the direction mask per tile.

### 5.2 STREAMS (group 2) — `populateStreams`, lines 1113-1123

13 entries; entry *i* = single part `OBJ_ICN_TYPE_STREAM`, icnIndex=i, offset {0,0}, `OBJ_NONE`, `TERRAIN_LAYER`. Index semantics from `getStreamIndex` (`map_format_helper.cpp:758-810`) given the stream-neighbour direction mask:

| index | shape |
|---|---|
| 0 | left→bottom bend | 
| 1 | right→bottom bend |
| 2 / 5 | horizontal (random pick of two) |
| 3 / 12 | vertical (random pick; also the isolated-tile default) |
| 4 | top→right bend |
| 6 | 4-way cross |
| 7 | top→left bend |
| 8 | T: left+top+right |
| 9 | T: top+right+bottom |
| 10 | T: top+left+bottom |
| 11 | T: left+right+bottom |

### 5.3 LANDSCAPE_MOUNTAINS (group 3) — lines 1125-1528; **63 entries**

Outer loop over 8 ICN types **in this order**: `MTNMULT` (generic), `MTNGRAS`, `MTNSNOW`, `MTNSWMP`, `MTNLAVA`, `MTNDSRT`, `MTNDIRT`, `MTNCRCK` (line 1130-1131). Per type the sequence is: big TL→BR, big TR→BL, *(extra TL→BR, extra TR→BL — only for MTNDIRT and MTNCRCK, line 1197)*, medium #1, medium #2, small TL→BR, small TR→BL.

| terrain | indices |
|---|---|
| Generic (MTNMULT) | 0–5 |
| Grass | 6–11 |
| Snow | 12–17 |
| Swamp | 18–23 |
| Lava | 24–29 |
| Desert | 30–35 |
| Dirt | 36–43 (8: extra pair at 38,39) |
| Wasteland | 44–51 (8: extra pair at 46,47) |

Per-type footprints (OBJECT_LAYER occupied tiles, `OBJ_MOUNTAINS`):
- big TL→BR (e.g. idx 0/6/36…): main {0,0}; occupied {-2,-1},{-1,-1},{0,-1},{1,-1},{-2,0},{-1,0},{1,0},{2,0},{0,1},{1,1},{2,1} + main = 12 tiles (5w×3h); 4 shadow parts; 5 top parts.
- big TR→BL (idx+1): mirrored 12 tiles.
- extra dirt/wasteland pair: 6 occupied tiles each.
- medium (2 entries): 5–7 occupied tiles (grass version blocks its top row too, lines 1264-1272).
- small (2 entries): 4 occupied tiles ({0,0},{-1,-1} or {0,-1},{±1,-1},{±1,0}).

Then single objects: 52 grass medium mound (2 tiles, OBJNGRAS 77/78), 53 grass small mound (2 tiles, 149/150), 54 big volcano `OBJ_VOLCANO` (3 tiles wide, OBJNLAV3 244-246, huge animated top), 55 middle volcano (OBJNLAV2 30-32), 56 small volcano (OBJNLAV2 79-81), 57 smallest volcano (2 tiles, OBJNLAVA 76/77), 58 desert dune (2 tiles BACKGROUND, OBJNDSRT 14/15), 59 desert mound (2, 17/18), 60 desert dune (3, 20-22), 61 dirt mound (2, OBJNDIRT 12/13), 62 dirt mound (2, 15/16).

### 5.4 LANDSCAPE_ROCKS (group 4) — lines 1530-1889; **40 entries**

Sequential, grouped by terrain (main part ICN / icnIndex; all `OBJ_ROCK` unless noted):

| idx | description | ICN, main icn | occupied tiles |
|---|---|---|---|
| 0 | small+medium rocks, grass | OBJNGRAS 33 (+34 @{1,0}) | 2 |
| 1 | medium+very small, grass | OBJNGRAS 37 (+38) | 2 (+top 35) |
| 2 | single medium, grass | OBJNGRAS 40 | 1 |
| 3 | tiny rocks, grass | OBJNGRAS 41 | 1 |
| 4–7 | small 1-tile, grass ×4 | OBJNGRAS 43/45/47/49 | 1 each |
| 8 | small rocks, snow | OBJNSNOW 22 | 1 |
| 9 | medium rocks, snow | OBJNSNOW 26 (+27) | 2 (+2 top) |
| 10 | tiny rocks, snow (BACKGROUND) | OBJNSNOW 28 | 1 |
| 11–12 | small 1-tile, snow ×2 | OBJNSNOW 30/32 | 1 each |
| 13 | wide small rocks, snow | OBJNSNOW 34 (+35) | 2 |
| 14 | medium rock, snow | OBJNSNOW 37 | 1 |
| 15 | medium rock, snow | OBJNSNOW 39 (+38 @{-1,0}) | 2 |
| 16 | wide low mossy rock, swamp (BACKGROUND, `OBJ_MOSSY_ROCK`) | OBJNSWMP 139 (+138 @{-1,0}) | 2 |
| 17 | medium mossy rock, swamp (`OBJ_MOSSY_ROCK`) | OBJNSWMP 203 | 1 |
| 18 | medium rock, swamp | OBJNSWMP 205 | 1 |
| 19 | big mossy rock, swamp (`OBJ_MOSSY_ROCK`) | OBJNSWMP 209 (+208 @{-1,0}) | 2 (+top 206) |
| 20 | very small rocks, swamp | OBJNSWMP 210 | 1 |
| 21 | wide medium rocks, dirt | OBJNDIRT 92 (+93 @{1,0}) | 2 |
| 22 | big + 3 medium, dirt | OBJNDIRT 98 (+99) | 2 (+2 top) |
| 23 | medium+small, dirt | OBJNDIRT 101 (+102) | 2 |
| 24 | medium rock, dirt | OBJNDIRT 104 | 1 |
| 25 | small rocks, dirt | OBJNDIRT 105 | 1 |
| 26 | wide medium rock w/ tree, wasteland | OBJNCRCK 10 (+11) | 2 (+2 top) |
| 27 | small rock, wasteland | OBJNCRCK 18 | 1 |
| 28 | big + 2 medium, wasteland | OBJNCRCK 21 (+22) | 2 (+top) |
| 29 | wide medium, wasteland | OBJNCRCK 24 (+25) | 2 |
| 30 | wide big, wasteland | OBJNCRCK 30 (+29 @{-1,0}) | 2 (+2 top) |
| 31 | wide low, wasteland | OBJNCRCK 31 (+32) | 2 |
| 32 | wide low, wasteland | OBJNCRCK 34 (+35) | 2 |
| 33 | wide small + small, wasteland | OBJNCRCK 38 (+37 @{-1,0}) | 2 |
| 34 | wide small, wasteland | OBJNCRCK 40 (+41) | 2 |
| 35 | small+medium, wasteland | OBJNCRCK 43 (+42 @{-1,0}) | 2 |
| 36–39 | tall rocks, wasteland ×4 | OBJNCRCK 46/49/52/55 | 1 each (+1 top) |

### 5.5 LANDSCAPE_TREES (group 5) — lines 1891-2283; **69 entries**

Loop over 6 forest ICN types **in this order** (line 1896-1897): `TREDECI` (deciduous), `TREEVIL` (dead/evil), `TREFALL` (autumn), `TREFIR` (fir), `TREJNGL` (jungle), `TRESNOW` (snowy fir). Per type 6 entries: big TL→BR, big TR→BL, medium TL→BR, medium TR→BL, small ×2.

| appearance | indices |
|---|---|
| Deciduous | 0–5 |
| Evil/dead | 6–11 |
| Autumn | 12–17 |
| Fir | 18–23 |
| Jungle | 24–29 |
| Snowy fir | 30–35 |

Footprints (`OBJ_TREES`): big = 4 occupied tiles ({0,0},{-1,0},{0,1},{1,1} or mirror) + 3 top; medium = 4 occupied ({0,0},{-1,-1},{0,-1},{1,0} or mirror); small = 1 occupied.

Singles:

| idx | description | ICN main | occupied |
|---|---|---|---|
| 36 | three trees, grass v1 | OBJNGRAS 84 (83,85 sides) | 3 |
| 37 | three trees, grass v2 | OBJNGRAS 89 (+90) | 2 |
| 38 | single tree, grass | OBJNGRAS 93 | 1 |
| 39 | two stumps, snow (`OBJ_STUMP`) | OBJNSNOW 41 | 1 |
| 40 | single stump, snow (`OBJ_STUMP`) | OBJNSNOW 42 | 1 |
| 41 | dead tree medium wide, snow (`OBJ_DEAD_TREE`) | OBJNSNOW 49 (+50) | 2 |
| 42 | dead tree tall wide, snow | OBJNSNOW 56 (55,57) | 3 |
| 43 | dead tree medium 1-tile, snow | OBJNSNOW 60 | 1 |
| 44 | dead tree thinned wide, snow | OBJNSNOW 64 (+65) | 2 |
| 45–49 | dead trees 1-tile, snow ×5 | OBJNSNOW 68/71/74/77/80 | 1 each |
| 50 | dead tree in pool, swamp | OBJNSWMP 167 (+166 @{-1,0}) | 2 |
| 51–52 | dead trees 2-tile, swamp ×2 | OBJNSWMP 171/176 | 2 each |
| 53 | two tall palms, desert | OBJNDSRT 3 | 1 |
| 54–56 | tall palms, desert ×3 | OBJNDSRT 6/9/12 | 1 each |
| 57 | lonely palm | OBJNDSRT 76 | 1 |
| 58–60 | small palms, desert ×3 | OBJNDSRT 24/26/28 | 1 each |
| 61 | tall single tree, dirt | OBJNDIRT 118 | 1 |
| 62 | two tall trees, dirt | OBJNDIRT 123 | 1 |
| 63 | two medium trees, dirt | OBJNDIRT 127 | 1 |
| 64 | dead tree, generic | OBJNMULT 2 | 1 |
| 65 | log, generic (`OBJ_DEAD_TREE`) | OBJNMULT 4 | 1 |
| 66 | three stumps, generic (`OBJ_STUMP`) | OBJNMUL2 16 | 1 |
| 67 | two stumps, generic | OBJNMUL2 18 | 1 |
| 68 | single stump, generic | OBJNMUL2 19 | 1 |

### 5.6 LANDSCAPE_WATER (group 6) — lines 2285-2425; **12 entries**

0 rock w/ seagulls (`OBJ_ROCK`, OBJNWATR 182/183, BACKGROUND, animated shadows), 1 rock (OBJNWATR 185), 2 rock 2-tile (186/187), 3 rock (OBJNWAT2 2), 4 aquatic plants (`OBJ_NONE` TERRAIN, OBJNWATR 83, 3 tiles, anim 6), 5 aquatic plants (OBJNWATR 97, 2 tiles), 6–11 reefs (`OBJ_REEFS`, X_LOC2 icn 113…135, BACKGROUND; footprints 7,7,4,3,2,2 tiles respectively).

### 5.7 LANDSCAPE_MISCELLANEOUS (group 7) — lines 2427-3770; **147 entries**

Sequential terrain blocks (main-part ICN noted; C=`OBJ_NONE` TERRAIN_LAYER crack/hole/cliff — no passability effect; most others OBJECT or BACKGROUND layer):

**Grass (0–28):** 0-2 cracks C (OBJNGRAS 9/15/21), 3 small crack C (28), 4 big water lake `OBJ_WATER_LAKE` (61; 5 ground tiles), 5 medium lake (71, BACKGROUND), 6 small lake (74), 7 wide shrub `OBJ_SHRUB` (96), 8 wide shrub (100), 9-11 medium-wide shrub ×3 (104/106/108 main=offset+1 in loop, offset=103+2n… see 2550-2559), 12 small shrub (112), 13 pink flowers `OBJ_FLOWERS` (64), 14 wide flowers (115), 15 wide taller flowers (122), 16-17 wide flowers ×2 (2606 loop), 18 medium-wide taller (136), 19 very small flowers (137, BACKGROUND), 20 low medium-wide (140), 21-24 small 1-tile flowers ×4 (2652 loop), 25 dug hole C (OBJNGRA2 9), 26-28 small cliff C ×3 (OBJNGRA2, 2671 loop).

**Snow (29–39):** 29 crack C (OBJNSNOW 1), 30 dug hole C (11), 31-32 small cliff C ×2, 33 wide small cliff C (19), 34-36 very small shrub ×3, 37 big frozen lake `OBJ_FROZEN_LAKE` (87, BACKGROUND), 38 medium (91), 39 small (94).

**Swamp (40–67):** 40 crack C (OBJNSWMP 30), 41 shrub (33), 42 dug hole C (86), 43 big lake `OBJ_SWAMPY_LAKE` (96), 44 medium wide lake (113), 45 medium tall lake (119), 46 single mandrake `OBJ_MANDRAKE` (126), 47 medium-wide mandrake (129), 48 single mandrake (131), 49 wide mandrakes (134), 50 small mandrake (137), 51 wide swampy lake (147), 52-54 small wide lakes ×3, 55 small lake (160), 56 wide reed `OBJ_NOTHING_SPECIAL` (181, BACKGROUND), 57 medium reed (185), 58 small reed (187), 59-60 rotten roots ×2, 61 small rotten roots (192), 62 medium-wide shrub (194), 63-64 small shrub ×2, 65 medium-wide shrub (200), 66 small shrub (201), 67 mossy roots (213).

**Lava (68–82):** 68 crack C (OBJNLAVA 1), 69 medium crater wide `OBJ_CRATER` (8, BACKGROUND), 70 medium crater high (15), 71 small lava pool `OBJ_LAVAPOOL` (20), 72 passable lava C (24), 73 dug hole C (26), 74 big lava pool (36), 75 medium pool (43), 76 medium lifted pool (51), 77-79 small streams (56/59/63), 80-81 medium streams (67/72), 82 smoking volcano crater `OBJ_VOLCANO` (98).

**Desert (83–98):** 83 crack C (OBJNDSRT 1), 84 tiny cactus `OBJ_CACTUS` (30), 85-86 tiny ×2, 87-88 medium ×2, 89 medium (39), 90 small (40), 91 medium (42), 92 medium (45), 93 medium (48), 94 small (49), 95-96 medium ×2, 97 dug hole C (68), 98 big hole `OBJ_NOTHING_SPECIAL` (111, BACKGROUND).

**Dirt (99–128):** 99 crack C (OBJNDIRT 11), 100-102 crack craters ×3 `OBJ_CRATER` (BACKGROUND, loop 3360), 103 big water lake (35), 104 medium (55), 105 small (58), 106-108 wide shrub ×3, 109-110 shrub ×2, 111-114 wide flowers ×4, 115 small flowers (85), 116 tiny flowers (86, BACKGROUND), 117 small flowers (88), 118-119 tiny flowers ×2, 120 tiny meadow `OBJ_NOTHING_SPECIAL` (106), 121-122 wide meadow ×2, 123 grass high (112), 124 grass (113), 125 dug hole C (140), 126-128 small cliff C ×3.

**Wasteland (129–141):** 129 crack C (OBJNCRCK 6), 130 cactus (14), 131 cactus (16), 132 cow scull `OBJ_NOTHING_SPECIAL` (17), 133 shrub (57), 134 big crack `OBJ_CRATER` (64, BACKGROUND, wide), 135 dug hole C (70), 136-137 shrub ×2, 138 tar pit `OBJ_TAR_PIT` (113, BACKGROUND, large), 139 crack (225), 140 crack vertical (230), 141 crack horizontal (234).

**Generic (142–146):** 142 shrub (OBJNMULT 64), 143 river delta ocean-bottom C (OBJNMUL2 2), 144 river delta ocean-top C (11), 145 river delta ocean-right C (220), 146 river delta ocean-left C (229). (River deltas are the STREAM endpoints; `isRiverDeltaObject` in `map_format_helper.cpp:1282` checks `LANDSCAPE_MISCELLANEOUS` index 143-146.)

### 5.8 LANDSCAPE_TOWN_BASEMENTS (group 8) — lines 3772-3793; **8 entries**

Loop `basement = 0..7`, `icnOffset = basement * 10`, ICN `OBJ_ICN_TYPE_OBJNTWBA`, objectType `OBJ_NON_ACTION_CASTLE` (non-action).

**Order (comment line 3776): grass, snow, swamp, lava, desert, dirt, wasteland, beach:**

| index | terrain |
|---|---|
| 0 | Grass |
| 1 | Snow |
| 2 | Swamp |
| 3 | Lava |
| 4 | Desert |
| 5 | Dirt |
| 6 | Wasteland |
| 7 | Beach |

Selection for a terrain: `fheroes2::getTownBasementId( groundType )` (`src/fheroes2/gui/ui_map_object.cpp:295-326`) — exactly the table above (WATER asserts and returns 0).

Footprint (per entry): 10 parts, 5w×2h, main {0,0}=icn+2:
- y=0 row (OBJECT_LAYER): {-2,0}+0, {-1,0}+1, {0,0}+2, {1,0}+3, {2,0}+4
- y=1 row: {-2,1}+5, {-1,1}+6, **{0,1}+7 = SHADOW_LAYER** (the entrance apron below the castle gate stays passable), {1,1}+8, {2,1}+9.

So a basement occupies (blocks) 9 tiles; the tile directly below the main tile is free.

### 5.9 LANDSCAPE_FLAGS (group 9) — lines 3795-3823; **14 entries**

Loop `color = 0..6`, `icnOffset = color * 2`; per color first **Left flag** (table index `color*2`, FLAG32 icn `color*2`), then **Right flag** (`color*2+1`, icn `color*2+1`). Single ground part at {0,0}, `OBJ_NON_ACTION_CASTLE`, OBJECT_LAYER, `metadata[0] = color`.

Color order (comment line 3799 + `Color::GetIndex`, `color.cpp:102-123`): blue=0, green=1, red=2, yellow=3, orange=4, purple=5, gray/neutral=6.

| color | left idx | right idx |
|---|---|---|
| Blue | 0 | 1 |
| Green | 2 | 3 |
| Red | 4 | 5 |
| Yellow | 6 | 7 |
| Orange | 8 | 9 |
| Purple | 10 | 11 |
| Neutral (gray) | 12 | 13 |

Placement convention (`editor_interface.cpp:3328-3340`): left flag on tile `castleEntranceIndex - 1`, right flag on `castleEntranceIndex + 1`, both with the **same UID** as the town object (UID counter reset via `setLastObjectUID(objectId)` before each). `TODO: Add flags for other capture-able objects` (line 3822) — mines currently get ownership via `capturableObjectsMetadata[uid].ownerColor` instead, not via LANDSCAPE_FLAGS.

### 5.10 ADVENTURE_ARTIFACTS (group 10) — lines 3825-3865; **96 entries**

Helper `addArtifactObject(artifactId, objectType, mainIcnIndex)`: main part `OBJ_ICN_TYPE_OBJNARTI` icn `mainIcnIndex` at {0,0} (OBJECT_LAYER), `metadata[0]=artifactId`, shadow icn `mainIcnIndex-1` at {-1,0}.

Construction:
1. `for artifactId = Artifact::ARCANE_NECKLACE (9) .. MAGIC_BOOK-1 (81)`: icn = `artifactId*2-1`, type `OBJ_ARTIFACT` → **index = artifactId − 9** (0..72).
2. MAGIC_BOOK (id 82, temp icn 207) → **index 73**.
3. `for artifactId = SPELL_SCROLL (87) .. ARTIFACT_COUNT-1 (103)`: icn = `artifactId*2-1` → **index = artifactId − 13** (74..90).
4. Randoms (metadata[0] = `Artifact::UNKNOWN` = 0):
   - 91 `OBJ_RANDOM_ARTIFACT` (icn 163)
   - 92 `OBJ_RANDOM_ARTIFACT_TREASURE` (icn 167)
   - 93 `OBJ_RANDOM_ARTIFACT_MINOR` (icn 169)
   - 94 `OBJ_RANDOM_ARTIFACT_MAJOR` (icn 171)
   - 95 `OBJ_RANDOM_ULTIMATE_ARTIFACT` (icn 164, shadow icn **165** at {-1,0} — the one case where shadow = main+1)

Artifact ids (from `src/fheroes2/resource/artifact.h:67-189`, all sequential): UNKNOWN=0, ULTIMATE_BOOK=1 … GOLDEN_GOOSE=8, ARCANE_NECKLACE=9 … BLACK_PEARL=81, MAGIC_BOOK=82, EDITOR_ANY_ULTIMATE_ARTIFACT=83, UNUSED_84/85/86, SPELL_SCROLL=87 … SPADE_NECROMANCY=103, ARTIFACT_COUNT=104. Note: the 8 ultimate artifacts (ids 1–8) have **no** table entries (loop starts at 9). Header comment (artifact.h:65): "All artifact IDs are by value 1 bigger than in the original game."

Inverse formula: `id = index+9` for index ≤ 72; `id = 82` for 73; `id = index+13` for 74–90.

### 5.11 ADVENTURE_DWELLINGS (group 11) — lines 3867-4226; **25 entries**

Occupied = ground OBJECT/BACKGROUND-layer tiles (offsets relative to main/action tile at {0,0}):

| idx | object (`objectType`) | ICN main | occupied tiles (excl. shadow) |
|---|---|---|---|
| 0 | Peasant Hut `OBJ_PEASANT_HUT` | OBJNMULT 35 | {0,0} (+ top {0,-1}) |
| 1 | Ruins (Medusa) `OBJ_RUINS` | OBJNMULT 74 | {0,0},{-1,0} |
| 2 | Tree House (Sprites) `OBJ_TREE_HOUSE` | OBJNMULT 114 | {0,0} (+top) |
| 3 | Watch Tower (Orcs) `OBJ_WATCH_TOWER` generic | OBJNMULT 116 | {0,0} |
| 4 | Watch Tower snow | OBJNSNOW 138 | {0,0} (+top) |
| 5 | Halfling Hole `OBJ_HALFLING_HOLE` (dirt) | OBJNDIRT 138 | {0,0},{1,0},{-1,0} |
| 6 | Halfling Hole (grass) | OBJNGRA2 7 | {0,0},{1,0},{-1,0} |
| 7 | Tree City (Sprites) `OBJ_TREE_CITY` (dirt) | OBJNDIRT 151 | {0,0},{1,0} (+2 top) |
| 8 | Tree City (grass) | OBJNGRA2 21 | {0,0},{1,0} (+2 top) |
| 9 | Desert Tent (Nomads) `OBJ_DESERT_TENT` | OBJNDSRT 73 | {0,0},{-1,0} (+2 top) |
| 10 | City of the Dead (Liches) `OBJ_CITY_OF_DEAD` | OBJNDSRT 96 | {0,0},{-2,0},{-1,0},{1,0},{2,0},{-2,-1},{-1,-1},{0,-1},{1,-1},{2,-1} (10 tiles, 5×2) |
| 11 | Excavation (Skeletons) `OBJ_EXCAVATION` | OBJNDSRT 101 | {0,0},{-1,0},{-2,0} |
| 12 | Troll's Bridge `OBJ_TROLL_BRIDGE` | OBJNCRCK 189 (BACKGROUND) | {0,0},{-1,0},{-2,0},{-3,0},{0,-1},{-1,-1},{-2,-1},{-3,-1} (4×2, all BACKGROUND) |
| 13 | Archer's House `OBJ_ARCHER_HOUSE` | OBJNGRA2 84 (anim 6) | {0,0} (+top) |
| 14 | Goblin's Hut `OBJ_GOBLIN_HUT` | OBJNGRA2 92 | {0,0} |
| 15 | Dwarf's Cottage `OBJ_DWARF_COTTAGE` | OBJNGRA2 114 (anim 6) | {0,0} (+top) |
| 16 | Dragon City `OBJ_DRAGON_CITY` | OBJNMUL2 54 | {0,0},{-3,0},{-2,0},{-1,0},{1,0},{-1,-1} (+many top parts up to {−3,−3}) |
| 17 | Wagon Camp (Rogues) `OBJ_WAGON_CAMP` | OBJNMUL2 129 (anim 6) | {0,0},{1,0},{-1,0} (+3 top) |
| 18 | Cave (Centaurs) `OBJ_CAVE` | OBJNGRAS 152 | {0,0},{-1,0} |
| 19 | Snow Cave (Centaurs) | OBJNSNOW 3 | {0,0},{-1,0} |
| 20 | Barrow Mounds (Ghosts) `OBJ_BARROW_MOUNDS` | X_LOC1 77 | {0,0},{-1,0},{-2,0} (+top) |
| 21 | Earth Summoning Altar `OBJ_EARTH_ALTAR` | X_LOC1 94 (anim 8) | {0,0},{1,0},{-1,0} (+top) |
| 22 | Air Summoning Altar `OBJ_AIR_ALTAR` | X_LOC1 118 | {0,0},{1,0},{-1,0} (+top) |
| 23 | Fire Summoning Altar `OBJ_FIRE_ALTAR` | X_LOC1 127 | {0,0},{1,0},{-1,0} (+top) |
| 24 | Water Summoning Altar `OBJ_WATER_ALTAR` | X_LOC1 135 | {0,0},{1,0},{-1,0} (+top) |

### 5.12 ADVENTURE_MINES (group 12) — lines 4228-4448; **56 entries** (asserted at `ui_map_object.cpp:336`)

**Mines (indices 0–39):** outer loop over 8 `(icnType, icnOffset)` pairs in order (lines 4236-4239): `MTNMULT/74` (generic — used for beach), `MTNGRAS/74`, `MTNSNOW/74`, `MTNSWMP/74`, `MTNLAVA/74`, `MTNDSRT/74`, `MTNDIRT/104`, `MTNCRCK/104`. Inner loop resource 0..4 = **Ore, Sulfur, Crystal, Gems, Gold** (an `OBJ_ICN_TYPE_EXTRAOVR` cart part with icnIndex = resource is swapped on the main tile per entry, line 4254).

`index = terrainSlot * 5 + resourceSlot` where terrainSlot: generic/beach=0, grass=1, snow=2, swamp=3, lava=4, desert=5, dirt=6, wasteland=7 (`mineIndexFromGroundType`, `ui_map_object.cpp:91-120`); resourceSlot: Ore=0, Sulfur=1, Crystal=2, Gems=3, Gold=4 (`getMineObjectInfoId`, `ui_map_object.cpp:328-365`).

| terrain \ resource | Ore | Sulfur | Crystal | Gems | Gold |
|---|---|---|---|---|---|
| Generic (beach) | 0 | 1 | 2 | 3 | 4 |
| Grass | 5 | 6 | 7 | 8 | 9 |
| Snow | 10 | 11 | 12 | 13 | 14 |
| Swamp | 15 | 16 | 17 | 18 | 19 |
| Lava | 20 | 21 | 22 | 23 | 24 |
| Desert | 25 | 26 | 27 | 28 | 29 |
| Dirt | 30 | 31 | 32 | 33 | 34 |
| Wasteland | 35 | 36 | 37 | 38 | 39 |

metadata (lines 4256-4282): `[0]`=resource type, `[1]`=income/day — Ore {0x04, 2}, Sulfur {0x08, 1}, Crystal {0x10, 1}, Gems {0x20, 1}, Gold {0x40, 1000}. (Resource bitmask, `resource.h:50-59`: WOOD=0x01, MERCURY=0x02, ORE=0x04, SULFUR=0x08, CRYSTAL=0x10, GEMS=0x20, GOLD=0x40.)

Mine footprint (objectType `OBJ_MINE`, non-action parts `OBJ_NON_ACTION_MINE`): main **{0,0}** = icn offset+8 (action tile); occupied OBJECT_LAYER: **{1,-1}** (+4), **{-1,0}** (+7), **{1,0}** (+9), plus the EXTRAOVR cart at {0,0}; shadows {-3,-1},{-2,-1},{-3,0},{-2,0}; top parts {-1,-1},{0,-1}. → blocked tiles: {0,0}, {1,-1}, {-1,0}, {1,0}.

**Abandoned Mines (40–47):** `index = 40 + terrainSlot` (same slot function). Order: 40 generic (MTNMULT 92 main), 41 grass (OBJNGRAS 6 main; only 2 shadows), 42 snow (MTNSNOW 92), 43 swamp (MTNSWMP 92), 44 lava (MTNLAVA 92), 45 desert (MTNDSRT 92), 46 dirt (OBJNDIRT 8 main), 47 wasteland (MTNCRCK 122 main). objectType `OBJ_ABANDONED_MINE`; same footprint as mines; each also carries `EXTRAOVR` icn **5** at {0,0}; no metadata (resource UNKNOWN selects them: `getMineObjectInfoId` returns `40 + mineIndexFromGroundType` for `Resource::UNKNOWN`).

**Sawmills (48–53):** loop over `(icn, offset)` (lines 4389-4391): OBJNMUL2/210 (grass **and** swamp), OBJNSNOW/195, OBJNLAVA/118, OBJNDSRT/123, OBJNMUL2/74 (dirt), OBJNCRCK/239 (wasteland). `index = 48 + sawmillIndexFromGroundType` (`ui_map_object.cpp:122-151`): grass 0, swamp 0, snow 1, lava 2, desert 3, beach 3, dirt 4, wasteland 5.

| idx | terrain |
|---|---|
| 48 | Grass / Swamp |
| 49 | Snow |
| 50 | Lava |
| 51 | Desert (also used for Beach) |
| 52 | Dirt |
| 53 | Wasteland |

metadata {WOOD=0x01, 2}. Footprint (`OBJ_SAWMILL`): main {0,0}=offset+6; occupied {0,-1}(+2), {1,-1}(+3), {-2,0}(+4), {-1,0}(+5), {1,0}(+7); top {-2,-1}(+0), {-1,-1}(+1) → blocked: 6 tiles ({-2..1,0} minus {-2,-1}… precisely {0,0},{1,0},{-1,0},{-2,0},{0,-1},{1,-1}).

**Alchemist Labs (54–55):** 54 generic (OBJNMUL2, main {0,0}=26, occupied {-1,0}=25, {1,0}=27 anim 6; shadows {-2,0},{-2,-1}; top {-1,-1},{0,-1},{1,-1}); 55 snow (OBJNSNOW main 150, occupied {-1,0}=149, {1,0}=151 anim 6). metadata {MERCURY=0x02, 1}. Selection: `(groundType == SNOW) ? 55 : 54`.

`setObjectOnTile` copies mine/sawmill/lab metadata into the world tile (`maps_tiles_helper.cpp:2231-2244`). Ownership of a placed mine is stored per-UID in `MapFormat::capturableObjectsMetadata[uid].ownerColor` (map_format_info.h:261-264, 413) — not via LANDSCAPE_FLAGS (flags TODO at `map_object_info.cpp:3822`).

### 5.13 ADVENTURE_POWER_UPS (group 13) — lines 4450-4741; **23 entries**

| idx | object | ICN main | occupied |
|---|---|---|---|
| 0 | Artesian Spring `OBJ_ARTESIAN_SPRING` | OBJNCRCK 3 (+4 @{1,0}) | 2 |
| 1 | Watering Hole `OBJ_WATERING_HOLE` | OBJNCRCK 218 (217,219,220) BACKGROUND | 4: {-1..2,0} |
| 2 | Faerie Ring `OBJ_FAERIE_RING` (dirt) | OBJNDIRT 129 (+130 @{1,0}) | 2 |
| 3 | Faerie Ring (grass) | OBJNGRAS 30 (+31) | 2 |
| 4 | Faerie Ring (swamp) | OBJNSWMP 84 (+85) | 2 |
| 5 | Oasis `OBJ_OASIS` | OBJNDSRT 108 (+109) | 2 (+2 top) |
| 6 | Fountain `OBJ_FOUNTAIN` | OBJNMUL2 15 | 1 |
| 7 | Magic Well `OBJ_MAGIC_WELL` | OBJNMUL2 162 | 1 |
| 8 | Magic Well | OBJNMUL2 165 | 1 |
| 9 | Magic Well (snow) | OBJNSNOW 194 | 1 (+top) |
| 10 | Fort `OBJ_FORT` | OBJNMULT 59 (+58 @{-1,0}) | 2 (+top) |
| 11 | Gazebo `OBJ_GAZEBO` | OBJNMULT 62 | 1 (+top) |
| 12 | Witch Doctor's Hut `OBJ_WITCH_DOCTORS_HUT` | OBJNMULT 69 | 1 (+top) |
| 13 | Mercenary Camp `OBJ_MERCENARY_CAMP` | OBJNMULT 71 (+72 @{1,0}, +70 @{-1,0}) | 3 |
| 14 | Shrine of the First Circle `OBJ_SHRINE_FIRST_CIRCLE` | OBJNMULT 80 | 1 |
| 15 | Shrine of the Second Circle | OBJNMULT 76 | 1 |
| 16 | Shrine of the Third Circle | OBJNMULT 78 | 1 |
| 17 | Idol `OBJ_IDOL` | OBJNMULT 82 | 1 |
| 18 | Standing Stones `OBJ_STANDING_STONES` | OBJNMULT 84 (+85 @{1,0}) | 2 |
| 19 | Temple `OBJ_TEMPLE` | OBJNMULT 88 (+89 @{1,0}) | 2 (+2 top) |
| 20 | Tree of Knowledge `OBJ_TREE_OF_KNOWLEDGE` | OBJNMULT 123 | 1 (+4 top) |
| 21 | Xanadu `OBJ_XANADU` | OBJNSWMP 81 (+82 @{1,0}, 74 @{-1,0}, 67 @{-2,0}) | 4 (+many top, anim) |
| 22 | Arena `OBJ_ARENA` | X_LOC1 70 | 7: {0,0},{1,0},{-1,0},{1,-1},{0,-1},{-1,-1} + anim (+3 top) |

### 5.14 ADVENTURE_TREASURES (group 14) — lines 4743-4823; **13 entries**

All single-tile action objects at {0,0} with a shadow at {-1,0}.

| idx | object | ICN main icn | metadata[0] |
|---|---|---|---|
| 0 | Wood pile `OBJ_RESOURCE` | OBJNRSRC 1 | WOOD (0x01) |
| 1 | Mercury | OBJNRSRC 3 | MERCURY (0x02) |
| 2 | Ore | OBJNRSRC 5 | ORE (0x04) |
| 3 | Sulfur | OBJNRSRC 7 | SULFUR (0x08) |
| 4 | Crystal | OBJNRSRC 9 | CRYSTAL (0x10) |
| 5 | Gems | OBJNRSRC 11 | GEMS (0x20) |
| 6 | Gold | OBJNRSRC 13 | GOLD (0x40) |
| 7 | Genie's Lamp `OBJ_GENIE_LAMP` | OBJNRSRC 15 | — |
| 8 | Random Resource `OBJ_RANDOM_RESOURCE` | OBJNRSRC 17 | — |
| 9 | Treasure Chest `OBJ_TREASURE_CHEST` | OBJNRSRC 19 | — |
| 10 | Campfire `OBJ_CAMPFIRE` (generic) | OBJNMULT 131 (anim 6) | — |
| 11 | Campfire (snow) | OBJNSNOW 4 (anim 6; shadow borrowed from OBJNDSRT 54) | — |
| 12 | Campfire (desert) | OBJNDSRT 61 (anim 6) | — |

Resource count for piles is per-UID in `MapFormat::resourceMetadata[uid].count` (`map_format_info.h:292-295, 419`).

### 5.15 ADVENTURE_WATER (group 15) — lines 4825-5029; **13 entries**

0 Bottle `OBJ_BOTTLE` (OBJNWATR 0), 1 Sea Chest `OBJ_SEA_CHEST` (19), 2 Flotsam `OBJ_FLOTSAM` (45), 3 Buoy `OBJ_BUOY` (195), 4 Boat right `OBJ_BOAT` (BOAT32 18), 5 Shipwreck Survivor (111), 6 Magellan's Maps (62), 7 Whirlpool (218, BACKGROUND, multi-tile), 8 Shipwreck, 9 Derelict Ship, 10 Mermaid, 11 Sirens, 12 Barrel `OBJ_BARREL` (OBJNMUL2 248, Resurrection object).

### 5.16 ADVENTURE_MISCELLANEOUS (group 16) — lines 5031-5809; **71 entries**

| idx | object | ICN main | occupied (excl. shadow/top) |
|---|---|---|---|
| 0 | Graveyard `OBJ_GRAVEYARD` (generic) | OBJNMUL2 208 | {0,0},{1,0},{-1,0} |
| 1 | Graveyard (snow) | OBJNSNOW 209 | 3 |
| 2 | Hill Fort `OBJ_HILL_FORT` (grass) | OBJNGRA2 4 | {0,0},{-1,0},{-2,0} |
| 3 | Hill Fort (dirt) | OBJNDIRT 135 | 3 |
| 4 | Windmill `OBJ_WINDMILL` (grass) | OBJNGRA2 55 (anim 3) | {0,0},{1,0} (+4 top anim) |
| 5 | Windmill (snow) | OBJNSNOW 128 | 2 |
| 6 | Windmill (dirt) | OBJNDIRT 185 | 2 |
| 7 | Oracle `OBJ_ORACLE` (grass) | OBJNGRA2 126 (+125 @{-1,0}) | 2 |
| 8 | Oracle (dirt) | OBJNDIRT 198 | 2 |
| 9 | Obelisk `OBJ_OBELISK` (grass) | OBJNGRA2 129 | 1 (+top) |
| 10 | Obelisk (snow) | OBJNSNOW 141 | 1 |
| 11 | Obelisk (swamp) | OBJNSWMP 216 | 1 |
| 12 | Obelisk (lava) | OBJNLAVA 110 | 1 |
| 13 | Obelisk (desert) | OBJNDSRT 104 | 1 |
| 14 | Obelisk (wasteland) | OBJNCRCK 238 | 1 |
| 15 | Obelisk (dirt) | OBJNDIRT 201 | 1 |
| 16 | Lean-to `OBJ_LEAN_TO` (snow) | OBJNSNOW 13 | 1 |
| 17 | Lean-to (grass) | OBJNGRAS 154 | 1 |
| 18 | Sign `OBJ_SIGN` (snow) | OBJNSNOW 143 | 1 |
| 19 | Sign (swamp) | OBJNSWMP 140 | 1 |
| 20 | Sign (lava) | OBJNLAVA 117 | 1 |
| 21 | Sign (desert) | OBJNDSRT 119 | 1 |
| 22 | Sign (generic) | OBJNMUL2 114 | 1 |
| 23 | Water Wheel `OBJ_WATER_WHEEL` (snow) | OBJNSNOW 191 | {0,0},{-2,0}(anim 6),{-1,0}(anim 6) (+4 top) |
| 24 | Water Wheel (generic) | OBJNMUL2 112 (+98 @{-2,0} anim, +105 @{-1,0} anim) | 3 (+4 top) |
| 25 | Wagon `OBJ_WAGON` | OBJNCRCK 74 | 1 |
| 26 | Witch's Hut `OBJ_WITCHS_HUT` (swamp) | OBJNSWMP 22 (anim 6) | 1 (+2 top anim) |
| 27 | Daemon Cave `OBJ_DAEMON_CAVE` (lava) | OBJNLAVA 115 (+114 @{-1,0}) | 2 |
| 28 | Daemon Cave (desert) | OBJNDSRT 117 | 2 |
| 29 | Pyramid `OBJ_PYRAMID` (desert) | OBJNDSRT 82 (+81 @{-1,0}) | 2 (+2 top) |
| 30 | Skeleton `OBJ_SKELETON` (desert) | OBJNDSRT 84 (+83 @{-1,0}) | 2 |
| 31 | Sphinx `OBJ_SPHINX` (desert) | OBJNDSRT 87 (+88 @{1,0}) | 2 (+2 top) |
| 32 | Trading Post `OBJ_TRADING_POST` (wasteland) | OBJNCRCK 213 (+202 @{-1,0} anim 10) | 2 (+top) |
| 33 | Trading Post (generic) | OBJNMULT 111 (+104 @{-1,0} anim 6) | 2 (+top) |
| 34 | Lighthouse `OBJ_LIGHTHOUSE` | OBJNMUL2 73 | 1 (+2 top, anim) |
| 35 | Stone Liths `OBJ_STONE_LITHS` | OBJNMUL2 116 | 1 |
| 36–37 | Stone Liths ×2 | OBJNMUL2 119 / 122 | 1 (+top) |
| 38 | Stone Liths (Resurrection) | OBJNMUL2 232 | 1 (+top) |
| 39 | Event `OBJ_EVENT` | OBJNMUL2 163 | 1 (invisible marker) |
| 40 | Freeman's Foundry `OBJ_FREEMANS_FOUNDRY` | OBJNMUL2 188 (+187 @{-1,0}) | 2 (+2 top anim) |
| 41 | Magic Garden `OBJ_MAGIC_GARDEN` | OBJNMUL2 190 (anim 6) | 1 |
| 42 | Observation Tower `OBJ_OBSERVATION_TOWER` (grass) | OBJNMUL2 201 | 1 (+top 198) |
| 43 | Observation Tower (generic) | OBJNMUL2 235 | 1 |
| 44 | Observation Tower (desert) | OBJNMUL2 236 (+237 @{1,0} TERRAIN) | 1 |
| 45 | Observation Tower (snow) | OBJNMUL2 238 (+239 @{1,0} TERRAIN) | 1 |
| 46 | Alchemist's Tower `OBJ_ALCHEMIST_TOWER` (PoL) | X_LOC1 3 | 1 (+top) |
| 47 | Stables `OBJ_STABLES` (PoL) | X_LOC2 4 (+3 @{-1,0}) | 2 (+2 top) |
| 48 | Jail `OBJ_JAIL` (PoL) | X_LOC2 9 | 1 (+2 top) |
| 49 | Hut of the Magi `OBJ_HUT_OF_MAGI` (PoL) | X_LOC3 30 (+31 @{1,0} TERRAIN) | 1 (+2 top anim) |
| 50 | Eye of the Magi `OBJ_EYE_OF_MAGI` (PoL) | X_LOC3 50 (anim 8) | 1 (+top) |
| 51–58 | Barrier `OBJ_BARRIER` ×8 (PoL) | X_LOC3 60+6n (anim 4) | 1; metadata[0]=1..8 (Aqua, Blue, Brown, Gold, Green, Orange, Purple, Red) |
| 59–66 | Traveller's Tent `OBJ_TRAVELLER_TENT` ×8 (PoL) | X_LOC3 110+4n | 1 (+top); metadata[0]=1..8, same color order |
| 67 | Graveyard (grass, "ugly", compat) | OBJNMUL2 58 (+57 @{-1,0}) | 2 |
| 68 | Graveyard (snow, ugly) | OBJNSNOW 160 | 2 |
| 69 | Graveyard (desert, ugly) | OBJNDSRT 122 | 2 |
| 70 | Black Cat `OBJ_BLACK_CAT` (Resurrection) | OBJNMUL2 241 (anim 6) | 1 |

`addObjectToMap` auto-creates per-UID metadata for: `OBJ_EVENT` → adventureMapEventMetadata, `OBJ_SIGN` → signMetadata, `OBJ_SPHINX` → sphinxMetadata (`map_format_helper.cpp:1133-1170`); Bottle → signMetadata (1171-1184).

### 5.17 KINGDOM_HEROES (group 17) — lines 5811-5832; **42 entries**

```cpp
for ( int32_t color = 0; color < 6; ++color )
    for ( int32_t race = 0; race < 7; ++race ) {
        // MINIHERO icn: color * 7 + race, at {0,0}, OBJ_HERO, OBJECT_LAYER
        object.metadata[0] = color;
        if ( race == 6 ) { ++race; }   // 6 (multi) is stored as 7 (random)
        object.metadata[1] = race;
    }
```

**index = color·7 + raceSlot**, raceSlot 0..6. Color order = `Color::GetIndex`: 0 Blue, 1 Green, 2 Red, 3 Yellow, 4 Orange, 5 Purple. Race slots 0..5 = Knight, Barbarian, Sorceress, Warlock, Wizard, Necromancer; slot 6 = Random hero (metadata[1] stored as **7**). `Race::IndexToRace` (`race.cpp:150-174`): 0→KNGT(0x01), 1→BARB(0x02), 2→SORC(0x04), 3→WRLK(0x08), 4→WZRD(0x10), 5→NECR(0x20), 6→MULT(0x40), 7→RAND(0x80). Single-tile action object `OBJ_HERO`. On placement, `addObjectToMap` creates `heroMetadata[uid]` with `race = Race::IndexToRace(metadata[1])` (`map_format_helper.cpp:1100-1108`). Examples: Blue Knight = 0, Blue Random = 6, Green Knight = 7, Purple Random = 41.

### 5.18 KINGDOM_TOWNS (group 18) — lines 5834-5899; **14 entries**

Loop `i = 0..11`: `icnOffset = i*16`, `race = i/2`, `isCastle = (i%2==0)`; ICN `OBJNTOWN` (town) + `OBJNTWSH` (shadow). Then two random entries with `OBJNTWRD`.

| idx | object | objectType | metadata[0] (race) | metadata[1] (isCastle) |
|---|---|---|---|---|
| 0 | Knight castle | `OBJ_CASTLE` (163) | 0 | 1 |
| 1 | Knight town | `OBJ_CASTLE` | 0 | 0 |
| 2 | Barbarian castle | `OBJ_CASTLE` | 1 | 1 |
| 3 | Barbarian town | `OBJ_CASTLE` | 1 | 0 |
| 4 | Sorceress castle | `OBJ_CASTLE` | 2 | 1 |
| 5 | Sorceress town | `OBJ_CASTLE` | 2 | 0 |
| 6 | Warlock castle | `OBJ_CASTLE` | 3 | 1 |
| 7 | Warlock town | `OBJ_CASTLE` | 3 | 0 |
| 8 | Wizard castle | `OBJ_CASTLE` | 4 | 1 |
| 9 | Wizard town | `OBJ_CASTLE` | 4 | 0 |
| 10 | Necromancer castle | `OBJ_CASTLE` | 5 | 1 |
| 11 | Necromancer town | `OBJ_CASTLE` | 5 | 0 |
| 12 | Random castle | `OBJ_RANDOM_CASTLE` | 7 | 1 |
| 13 | Random town | `OBJ_RANDOM_TOWN` | 7 | 0 |

(Race indices as in §5.17; there is no race 6 "multi" town.)

Footprint (`addTown` lambda, lines 5838-5884). Ground parts (OBJECT_LAYER, non-action parts get `objectType − 128`, i.e. `OBJ_NON_ACTION_CASTLE`):
- main/action tile **{0,0}** = townIcnOffset+13 (the castle gate);
- {-2,0}+11, {-1,0}+12, {1,0}+14, {2,0}+15;
- {-1,-1}+7, {0,-1}+8, {1,-1}+9;
- 16 shadow parts from OBJNTWSH covering {-5..-1, -2..1};
- top parts: {-2,-1}+6, {2,-1}+10, {-2,-2}+1, {-1,-2}+2, {0,-2}+3, {1,-2}+4, {2,-2}+5, {0,-3}+0.

→ Blocked tiles: 5 wide × row y=0 plus 3 tiles at y=-1 (8 tiles); action/entrance tile {0,0}. Editor "occupied area" override for towns: rect `{-2,-3,5,5}` (`editor_interface.cpp:3568-3573`).

**Color/race variants:** race+castle/town is the table index; **color is not part of the object**. The editor composes a town as 4 stored objects sharing one UID (`EditorInterface::_placeCastle`, `editor_interface.cpp:3288-3348`):
1. `LANDSCAPE_TOWN_BASEMENTS[getTownBasementId(ground)]` on the entrance tile,
2. `KINGDOM_TOWNS[type]` on the same tile (UID counter rewound first),
3. `LANDSCAPE_FLAGS[Color::GetIndex(color)*2]` on tile −1, `[...*2+1]` on tile +1 (same UID),
4. `castleMetadata[uid]` created; `addObjectToMap` seeds `builtBuildings` with `BUILD_TENT` (town) or `BUILD_CASTLE` (castle) (`map_format_helper.cpp:1109-1117`); road below the entrance becomes road index 512.

### 5.19 MONSTERS (group 19) — lines 5901-5960; **71 entries**

```cpp
for ( int32_t monsterId = Monster::PEASANT; monsterId <= Monster::WATER_ELEMENT; ++monsterId ) {
    // MONS32 icn: monsterId - 1, OBJ_MONSTER, metadata[0] = monsterId
}
```

**index = Monster id − 1 for every entry, including the randoms.** Single tile, OBJECT_LAYER, no shadow part. `Monster::MonsterType` (`src/fheroes2/monster/monster.h:53-134`, sequential from `UNKNOWN=0`):

PEASANT=1, ARCHER=2, RANGER=3, PIKEMAN=4, VETERAN_PIKEMAN=5, SWORDSMAN=6, MASTER_SWORDSMAN=7, CAVALRY=8, CHAMPION=9, PALADIN=10, CRUSADER=11, GOBLIN=12, ORC=13, ORC_CHIEF=14, WOLF=15, OGRE=16, OGRE_LORD=17, TROLL=18, WAR_TROLL=19, CYCLOPS=20, SPRITE=21, DWARF=22, BATTLE_DWARF=23, ELF=24, GRAND_ELF=25, DRUID=26, GREATER_DRUID=27, UNICORN=28, PHOENIX=29, CENTAUR=30, GARGOYLE=31, GRIFFIN=32, MINOTAUR=33, MINOTAUR_KING=34, HYDRA=35, GREEN_DRAGON=36, RED_DRAGON=37, BLACK_DRAGON=38, HALFLING=39, BOAR=40, IRON_GOLEM=41, STEEL_GOLEM=42, ROC=43, MAGE=44, ARCHMAGE=45, GIANT=46, TITAN=47, SKELETON=48, ZOMBIE=49, MUTANT_ZOMBIE=50, MUMMY=51, ROYAL_MUMMY=52, VAMPIRE=53, VAMPIRE_LORD=54, LICH=55, POWER_LICH=56, BONE_DRAGON=57, ROGUE=58, NOMAD=59, GHOST=60, GENIE=61, MEDUSA=62, EARTH_ELEMENT=63, AIR_ELEMENT=64, FIRE_ELEMENT=65, WATER_ELEMENT=66.

Randoms (each its own object type):

| idx | id | objectType |
|---|---|---|
| 66 | 67 RANDOM_MONSTER | `OBJ_RANDOM_MONSTER` |
| 67 | 68 RANDOM_MONSTER_LEVEL_1 | `OBJ_RANDOM_MONSTER_WEAK` |
| 68 | 69 RANDOM_MONSTER_LEVEL_2 | `OBJ_RANDOM_MONSTER_MEDIUM` |
| 69 | 70 RANDOM_MONSTER_LEVEL_3 | `OBJ_RANDOM_MONSTER_STRONG` |
| 70 | 71 RANDOM_MONSTER_LEVEL_4 | `OBJ_RANDOM_MONSTER_VERY_STRONG` |

`metadata[0]` = monster id; count lives per-UID in `monsterMetadata[uid].count` (auto-created by `addObjectToMap`, `map_format_helper.cpp:1125-1132`). `setObjectOnTile` special-cases monsters: `setMonsterOnTile(tile, metadata[0], 0)` + count 0 (`maps_tiles_helper.cpp:2219-2223`).

### 5.20 MAP_EXTRAS (group 20) — `populateExtraBoatDirections`, lines 5962-6022; **7 entries**

Boats `OBJ_BOAT`, ICN BOAT32, single tile: 0 top (icn 0), 1 top-right (9), 2 bottom-right (27), 3 bottom (36), 4 bottom-left (155 = 27+128 mirror pseudo-index), 5 left (146 = 18+128), 6 top-left (137 = 9+128). (Boat facing right is ADVENTURE_WATER index 4.)

---

## 6. metadata[] semantics per group (summary)

| group | metadata[0] | metadata[1] |
|---|---|---|
| ADVENTURE_ARTIFACTS | artifact id (0 for randoms) | — |
| ADVENTURE_MINES | resource bit (0x04 ore … 0x40 gold; WOOD for sawmills, MERCURY for labs; none for abandoned) | daily income (2/1/1/1/1000/2/1) |
| ADVENTURE_TREASURES | resource bit for piles 0–6 | — |
| ADVENTURE_MISCELLANEOUS | Barrier/Tent color 1..8 (indices 51–66 only) | — |
| KINGDOM_HEROES | color index 0–5 | race slot 0–5 or 7(random) |
| KINGDOM_TOWNS | race slot 0–5 or 7(random) | 1=castle, 0=town |
| LANDSCAPE_FLAGS | color index 0–6 | — |
| MONSTERS | monster id | — |

---

## 7. How the editor computes occupied/blocked tiles

- The map format stores an object **once** on its main tile; on load `readAllTiles`/`readTileObject` (`map_format_helper.cpp:935, 1028-1048`) replays `Maps::setObjectOnTile( tile, objectInfos[object.index], false )` for each `TileObjectInfo`, which walks all `groundLevelParts`/`topLevelParts` and stamps `mainTilePos + part.tileOffset` into the world tiles (`placeObjectOnTile`, `maps_tiles_helper.cpp:195-…`). Parts whose target is off-map are silently skipped only if they are SHADOW/TERRAIN layer; an action object with an OBJECT/BACKGROUND part off-map fails placement (lines 203-227).
- Passability: a tile is blocked by parts on `OBJECT_LAYER`/`BACKGROUND_LAYER`; `SHADOW_LAYER`/`TERRAIN_LAYER` never affect passability (`maps_tiles.h:52-55`, `Maps::getGroundLevelOccupiedTileOffset`, `map_object_info.cpp:6266-6279`). World passability is recomputed wholesale via `world.updatePassabilities()` after placement (`maps_tiles_helper.cpp:2241-2280`).
- Placement validation uses `getGroundLevelUsedTileOffset` (everything except shadows) — every used tile must satisfy the terrain condition and be on-map for action objects (`checkConditionForUsedTiles`, `editor_interface.cpp:398-426`).
- Editor selection rectangle: `getObjectOccupiedArea` (`editor_interface.cpp:3568-3597`) = bounding box of occupied offsets; hard-coded `{-2,-3,5,5}` for KINGDOM_TOWNS.

## 8. Practical rules for a .fh2m generator

1. Write each object into `TileInfo.objects` of its **main tile only**, with a **unique, monotonically increasing non-zero UID** — except castle composites (basement, town, 2 flags: same UID) and any other multi-part composite you want removed as a unit.
2. Recommended ordering for a castle on tile T (matching the editor): basement(group 8) on T, town(group 18) on T, left flag on T−1, right flag on T+1 — all with the same UID; put road index 512 on T+width if you want the entrance road; add `castleMetadata[uid]` with `builtBuildings = {BUILD_TENT or BUILD_CASTLE}`.
3. Every group ≥ 10 object needs its own metadata entry when applicable: monsters → `monsterMetadata[uid]`, artifacts → `artifactMetadata[uid]`, heroes → `heroMetadata[uid]` (set `.race`!), events/signs/sphinx/bottle → respective maps, resources → `resourceMetadata[uid]` (optional; count 0 = random), captured mines → `capturableObjectsMetadata[uid].ownerColor`.
4. Table indices are a stable contract ("add only at the end" policy), but validate against `GROUP sizes`: ROADS 513, STREAMS 13, MOUNTAINS 63, ROCKS 40, TREES 69, LANDSCAPE_WATER 12, LANDSCAPE_MISC 147, BASEMENTS 8, FLAGS 14, ARTIFACTS 96, DWELLINGS 25, MINES 56, POWER_UPS 23, TREASURES 13, ADV_WATER 13, ADV_MISC 71, HEROES 42, TOWNS 14, MONSTERS 71, MAP_EXTRAS 7.
