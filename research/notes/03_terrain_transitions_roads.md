# fheroes2 research notes 03 — Terrain encoding, transitions, roads, streams

Source: local clone `C:/Users/gjoer/source/repos/fheroes2` @ upstream HEAD `b086d1aa8b921163712aec2fb8188f4d0d375b09` (2026-09-01).
All paths below are relative to `src/fheroes2/` unless stated otherwise. Line numbers are from this exact commit.

---

## 1. Ground enum & terrainIndex encoding

### 1.1 Ground enum (`maps/ground.h:34-47`)

```cpp
enum : int32_t
{
    UNKNOWN   = 0x0000,
    DESERT    = 0x0001,
    SNOW      = 0x0002,
    SWAMP     = 0x0004,
    WASTELAND = 0x0008,
    BEACH     = 0x0010,
    LAVA      = 0x0020,
    DIRT      = 0x0040,
    GRASS     = 0x0080,
    WATER     = 0x0100,
    ALL = DESERT | SNOW | SWAMP | WASTELAND | BEACH | LAVA | DIRT | GRASS   // note: excludes WATER
};
```

### 1.2 Image index ranges in TIL::GROUND32 (`maps/ground.h:49-62`)

`TileInfo.terrainIndex` is an index into the `GROUND32.TIL` image set (0..431). Ground type is derived *purely* from the index range (`getGroundByImageIndex`, `ground.cpp:61-94` — simple range comparisons):

| Ground    | start enum constant           | range (inclusive) | count |
|-----------|-------------------------------|-------------------|-------|
| WATER     | `WATER_START_IMAGE_INDEX=0`   | 0–29              | 30    |
| GRASS     | `GRASS_START_IMAGE_INDEX=30`  | 30–91             | 62    |
| SNOW      | `SNOW_START_IMAGE_INDEX=92`   | 92–145            | 54    |
| SWAMP     | `SWAMP_START_IMAGE_INDEX=146` | 146–207           | 62    |
| LAVA      | `LAVA_START_IMAGE_INDEX=208`  | 208–261           | 54    |
| DESERT    | `DESERT_START_IMAGE_INDEX=262`| 262–320           | 59    |
| DIRT      | `DIRT_START_IMAGE_INDEX=321`  | 321–360           | 40    |
| WASTELAND | `WASTELAND_START_IMAGE_INDEX=361` | 361–414       | 54    |
| BEACH     | `BEACH_START_IMAGE_INDEX=415` | 415–431           | 17    |
| (max)     | `MAX_IMAGE_INDEX=432`         |                   |       |

`Maps::Tile::isWater()` = `_terrainImageIndex < Ground::GRASS_START_IMAGE_INDEX` (i.e. `< 30`) — `maps/maps_tiles.h:161-166`.
`Tile::GetGround()` = `Ground::getGroundByImageIndex( _terrainImageIndex )` — `maps_tiles.h:156-159`.

### 1.3 Sub-layout per ground (relative offset from start index)

Derived from `isTerrainTransitionImage` (`ground.cpp:96-117`), `doesTerrainImageIndexContainEmbeddedObjects` (`ground.cpp:119-141`), `getRandomTerrainImageIndex` (`ground.cpp:244-291`), and the offsets used in `setTerrainBoundaries` (`maps/map_format_helper.cpp:221-557`):

**Six "full" land terrains (GRASS, SNOW, SWAMP, LAVA, DESERT, WASTELAND):**

| rel. offset | meaning |
|-------------|---------|
| +0..+3      | straight edge transition **to dirt** on TOP (no flip) / BOTTOM (vflip); random pick of 4 |
| +4..+7      | corner (two adjacent sides) transition to dirt |
| +8..+11     | straight edge transition to dirt on RIGHT (no flip) / LEFT (hflip) |
| +12..+15    | ¾ tile: ground everywhere except one diagonal corner (dirt corner) |
| +16..+19    | straight edge transition **to water/beach** TOP/BOTTOM (`+0+16`) |
| +20..+23    | corner transition to water/beach (`+4+16`) |
| +24..+27    | straight edge transition to water/beach RIGHT/LEFT (`+8+16`) |
| +28..+31    | ¾ tile, water/beach diagonal corner (`+12+16`) |
| +32         | mixed: diagonal beach + straight dirt (bottom-left beach / left dirt family) |
| +33         | mixed: diagonal beach (top-right family) + straight dirt |
| +34         | mixed: straight-top dirt with diagonal beach (top-right/top-left) |
| +35         | mixed: straight-side dirt with diagonal beach |
| +36         | mixed corner: beach on one straight side + dirt on the adjacent straight side |
| +37         | mixed corner: beach/dirt swapped vs +36 |
| +38..+45    | **plain tiles** (8 variants) — `Rand::Get(7)+38` |
| +46..end    | plain tiles with **embedded decorative objects**: GRASS/SWAMP 46–61 (16), SNOW/WASTELAND/LAVA 46–53 (8), DESERT 46–58 (13) |

`isTerrainTransitionImage` = `rel < 38` for these six; embedded-object images = `rel > 45`.

**WATER:** rel 0–15 = transitions (to any land; water only has "beach-style" transitions), rel 16–19 = plain (`Rand::Get(3)+16`). rel 20–29 exist in the TIL but are never chosen by the engine. No embedded-object images (`doesTerrainImageIndexContainEmbeddedObjects` returns false).

**DIRT:** rel 0–15 = transitions (**only to water/beach**), rel 16–23 = plain (`Rand::Get(7)+16`), rel 24–39 = plain w/ embedded objects (16 variants, `Rand::Get(15)+24`). `isTerrainTransitionImage` = `rel < 16`.

**BEACH:** **no transition images at all** (`isTerrainTransitionImage` → false, `ground.cpp:110-111`). rel 0–7 = plain (`Rand::Get(7)+0`), rel 8–16 = embedded objects (9 variants, `Rand::Get(8)+8`).

### 1.4 Random plain image selection (`ground.cpp:244-291`)

`getRandomTerrainImageIndex( groundId, allowEmbeddedObjectsAppearOnTerrain )`:
- WATER: always `start + 16 + Rand::Get(3)` → 16..19.
- Otherwise, with probability 1/7 (`Rand::Get(6) == 0`) *and* `allowEmbeddedObjectsAppearOnTerrain==true`: an embedded-object image:
  - GRASS/SWAMP: `start + 46 + Rand::Get(15)`; SNOW/WASTELAND/LAVA: `start + 46 + Rand::Get(7)`; DESERT: `start + 46 + Rand::Get(12)`; DIRT: `start + 24 + Rand::Get(15)`; BEACH: `start + 8 + Rand::Get(8)`.
- Else plain: GRASS/SNOW/SWAMP/LAVA/DESERT/WASTELAND `start + 38 + Rand::Get(7)`; DIRT `start + 16 + Rand::Get(7)`; BEACH `start + 0 + Rand::Get(7)`.

`Rand::Get(from, to=0)` is **inclusive** uniform `[min,max]` (`src/engine/rand.cpp:93-100`), so `Rand::Get(3)` yields 0..3.

New blank editor map: every tile gets `setTerrainOnTile(map, i, Ground::WATER)` (`editor/editor_interface.cpp:3133-3176`, esp. 3164-3167), i.e. terrainIndex 16..19, terrainFlags 0.

---

## 2. TileInfo / terrainFlags / serialization

### 2.1 Structs (`maps/map_format_info.h:42-57`), declaration order

```cpp
struct TileObjectInfo
{
    uint32_t id{ 0 };                       // object UID (nonzero, unique per logical object; shared by all tiles of a multi-tile object)
    ObjectGroup group{ ObjectGroup::NONE }; // enum class : uint8_t
    uint32_t index{ 0 };                    // index inside the object group table
};

struct TileInfo
{
    uint16_t terrainIndex{ 0 };
    uint8_t terrainFlags{ 0 };
    std::vector<TileObjectInfo> objects;
};
```

### 2.2 terrainFlags semantics

Set in `setTerrain` (`map_format_helper.cpp:196`):
```cpp
mapTile.terrainFlags = ( verticalFlip ? 1 : 0 ) + ( horizontalFlip ? 2 : 0 );
```
- **bit 0 (value 1) = vertical flip**
- **bit 1 (value 2) = horizontal flip**
- Renderer uses only the low 2 bits: `Assets::getTileImage( TIL::GROUND32, tile.getTerrainImageIndex(), ( tile.getTerrainFlags() & 0x3 ) )` (`maps/maps_tiles_render.cpp:1325`). Shape generation confirms bit semantics: `horizontalFlip = (shapeId & 2) != 0; verticalFlip = (shapeId & 1) != 0` (`game/game_assets.cpp:5731-5732`).
- **No road bit, no other flags** — roads/streams are separate `TileObjectInfo` objects, never encoded in terrainFlags.
- Plain (non-transition) tiles are always written with flags 0 ("In original editor these tiles are never flipped", `map_format_helper.cpp:1062, 1080`).

Mirrored into the live world tile via `Tile::setTerrain( terrainImageIndex, terrainFlags )` (`maps_tiles.h:334-338`).

### 2.3 Serialization operators (`maps/map_format_info.cpp:589-607`)

```cpp
OStreamBase & operator<<( OStreamBase & stream, const TileObjectInfo & object )
{ return stream << object.id << object.group << object.index; }

OStreamBase & operator<<( OStreamBase & stream, const TileInfo & tile )
{ return stream << tile.terrainIndex << tile.terrainFlags << tile.objects; }
```
- Enum classes serialize as their underlying type (`ObjectGroup` → u8) — `src/engine/serialize.h:344-348`.
- `std::vector<T>` = u32 count then elements (`serialize.h:370-377`).
- The whole file is written **big-endian** (`saveMap` sets `fileStream.setBigendian(true)`, `map_format_info.cpp:830-842`; `put16/put32` honor the flag, `serialize.cpp:149-157`).
- File layout (`map_format_info.cpp:438-504, 824-842`): 6-byte magic `"h2map\0"` → **uncompressed** BaseMapFormat header (version=13 current, `currentSupportedVersion` at line 110; fields in order at lines 440-443: version, isCampaign, difficulty, availablePlayerColors, humanPlayerColors, computerPlayerColors, alliances, playerRace[6], victoryConditionType, isVictoryConditionApplicableForAI, allowNormalVictory, victoryConditionMetadata, lossConditionType, lossConditionMetadata, width, mainLanguage, name, description, creatorNotes, translations) → single **zlib-compressed blob** (`Compression::zipData`, `engine/zzlib.h:51`) containing, in order (line 495-497): `additionalInfo, tiles, dailyEvents, rumors, castleMetadata, heroMetadata, sphinxMetadata, signMetadata, adventureMapEventMetadata, selectionObjectMetadata, capturableObjectsMetadata, monsterMetadata, artifactMetadata, resourceMetadata, translationInfo`.
- `MapFormat.tiles` is `std::vector<TileInfo>` of exactly width×width entries (checked on load, line 540-543).

### 2.4 ObjectGroup values (`maps/map_object_info.h:127-168`, sequential from 0)

`NONE=0, ROADS=1, STREAMS=2, LANDSCAPE_MOUNTAINS=3, LANDSCAPE_ROCKS=4, LANDSCAPE_TREES=5, LANDSCAPE_WATER=6, LANDSCAPE_MISCELLANEOUS=7, LANDSCAPE_TOWN_BASEMENTS=8, LANDSCAPE_FLAGS=9, ADVENTURE_ARTIFACTS=10, ADVENTURE_DWELLINGS=11, ADVENTURE_MINES=12, ADVENTURE_POWER_UPS=13, ADVENTURE_TREASURES=14, ADVENTURE_WATER=15, ADVENTURE_MISCELLANEOUS=16, KINGDOM_HEROES=17, KINGDOM_TOWNS=18, MONSTERS=19, MAP_EXTRAS=20, GROUP_COUNT=21`.

---

## 3. Terrain painting & the transition algorithm (Editor)

### 3.1 Editor brush flow (`editor/editor_interface.cpp`)

- Terrain brush (`isTerrainEdit`, lines 1817-1845): on LMB over a new tile, computes brush rect indices (`getBrushAreaIndicies`, lines 286-304) and calls **`Maps::setTerrainWithTransition( _mapFormat, indices.x, indices.y, groundId )`** (line 1830), then `_validateObjectsOnTerrainUpdate()` (line 1831) which removes objects that became terrain-invalid (`verifyTerrainPlacement`, lines 687+; e.g. STREAMS/most objects can't be on water) and swaps town basements to match new ground (lines 3350-3391).
- Area-fill variant (drag-select with size-0 brush): same call with the selection rectangle corners (lines 1969-1982).
- Road brush (`isRoadDraw`, lines 1863-1880): LMB, skips water tiles, calls `Maps::setRoadOnTile( _mapFormat, _tileUnderCursor )`.
- Stream brush (`isStreamDraw`, lines 1881-1898): calls `Maps::addStream( _mapFormat, tileIndex )`.
- Erasing roads calls `Maps::removeRoadFromTile` (line 535); erasing a stream erases the object then `Maps::updateStreamsAround` (lines 544-549); erasing a river delta calls `Maps::updateStreamsToDeltaConnection` (line 645). Placing a river delta triggers the same delta-connection update (lines 2882-2885).

### 3.2 `setTerrainWithTransition` (`maps/map_format_helper.cpp:1050-1087`)

1. Asserts `map.width == world.w()` (needs live world).
2. Fills every tile of the rectangle with `setTerrain( map, tileId, Ground::getRandomTerrainImageIndex( groundId, true ), false, false )` — i.e. random plain image, no flips.
3. Calls `updateTerrainTransitionOnAreaBoundaries( map, groundId, startX, endX, startY, endY )`.

`setTerrainOnTile` (`map_format_helper.cpp:1197-1200`) is the transition-free primitive: one random plain tile.

### 3.3 `setTerrain` helper (`map_format_helper.cpp:191-208`)

```cpp
void setTerrain( map, tileId, imageIndex, horizontalFlip, verticalFlip )
{
    mapTile.terrainFlags = ( verticalFlip ? 1 : 0 ) + ( horizontalFlip ? 2 : 0 );
    if ( ( newGround != WATER ) && ( doesContainRoad(mapTile) || doesContainStreams(mapTile) )
         && doesTerrainImageIndexContainEmbeddedObjects( imageIndex ) )
        mapTile.terrainIndex = getRandomTerrainImageIndex( groundOf(imageIndex), false ); // re-roll: no décor under roads/streams
    else
        mapTile.terrainIndex = imageIndex;
    world.getTile( tileId ).setTerrain( mapTile.terrainIndex, mapTile.terrainFlags );   // world mirror
}
```

### 3.4 Neighborhood scan — `getGroundDirecton` (`map_format_helper.cpp:158-183`)

Returns a `Direction` bitmask of the 8 neighbors (+`CENTER` if the center tile itself matches `groundId`). **Out-of-map neighbors are clamped to the nearest valid tile** (x and y clamped to `[0, map.width-1]`; square maps assumed) — the map border behaves as a mirror of the edge tile. Ground identity is always tested via `Ground::getGroundByImageIndex( map.tiles[...].terrainIndex )`, i.e. reads **MapFormat only**.

Direction bit values (`heroes/direction.h:35-47`): `TOP_LEFT=0x01, TOP=0x02, TOP_RIGHT=0x04, RIGHT=0x08, BOTTOM_RIGHT=0x10, BOTTOM=0x20, BOTTOM_LEFT=0x40, LEFT=0x80, CENTER=0x100`; `DIRECTION_ALL` includes CENTER (macros at lines 59-66).

### 3.5 Per-tile decision — `updateTerrainTransitionOnTile` (`map_format_helper.cpp:560-610`)

```
ground = getGroundByImageIndex(tile.terrainIndex)
if ground == BEACH: return true                       // beach never gets transition images
tileGroundDirection =
    (ground == DIRT) ? DIRECTION_ALL - (dir(WATER) | dir(BEACH))   // dirt: only water/beach count as "foreign"
                     : dir(ground) | CENTER
if tileGroundDirection == DIRECTION_ALL:
    if current image is a transition image → replace with random plain image; return true
switch ground:
  WATER, DIRT:  setTerrainBoundaries( map, tileGroundDirection, /*beachDirection=*/0, tileId, startIndexOf(ground) )
  GRASS..WASTELAND:
      beachDirection = dir(WATER) | dir(BEACH)
      setTerrainBoundaries( map, tileGroundDirection, beachDirection, tileId, startIndexOf(ground) )
```

Key semantics (**Dirt/Beach special roles**):
- **Beach** has no transitions; it acts as a universal connector to water.
- **Dirt** treats every non-water/non-beach terrain as "same ground": dirt only ever draws transitions toward water/beach (its 16 transition images). Conversely, every other land terrain draws its land-land boundary using its "to dirt" images (offsets +0..+15) regardless of what the neighboring land terrain actually is — a grass|snow border renders as grass→dirt + snow→dirt edges.
- **Water** transitions to everything using its 16 images (beachDirection forced 0).

### 3.6 Boundary image choice — `setTerrainBoundaries` (`map_format_helper.cpp:221-557`)

Given `groundDirection` (where the *same* ground is), `beachDirection` (where water/beach neighbors are; 0 for water/dirt), and `imageOffset` = ground's start index. All `Rand::Get(3)` picks add 0..3. Order of checks matters; first match wins. Complete rule table (offsets relative; flips as (hflip,vflip) → terrainFlags = v + 2*h):

| groundDirection condition | neighbor situation | image | flips |
|---|---|---|---|
| `== DIRECTION_ALL` | uniform | none needed | — |
| all except TOP_LEFT | ¾ | `+12 (+16 if beach at TOP_LEFT) + R3` | (true,false) |
| all except TOP_RIGHT | ¾ | same | (false,false) |
| all except BOTTOM_RIGHT | ¾ | same (+16 if beach at BOTTOM_RIGHT) | (false,true) |
| all except BOTTOM_LEFT | ¾ | same | (true,true) |
| has LEFT|TOP|BOTTOM (& for water also TOP_LEFT&BOTTOM_LEFT) — nothing right | beach RIGHT → `+8+16+R3` (f,f); no ground right & beach TOP_RIGHT → `+35` (f,f); beach BOTTOM_RIGHT → `+35` (f,t); else dirt → `+8+R3` (f,f) |
| has RIGHT|TOP|BOTTOM — nothing left | mirrored with hflip=true (beach LEFT → `+8+16+R3` (t,f); `+35` (t,f)/(t,t); dirt `+8+R3` (t,f)) |
| has BOTTOM|LEFT|RIGHT — nothing top | beach TOP → `+16+R3` (f,f); beach TOP_RIGHT → `+34` (f,f); beach TOP_LEFT → `+34` (t,f); dirt → `+0+R3` (f,f) |
| has TOP|LEFT|RIGHT — nothing bottom | beach BOTTOM → `+16+R3` (f,t); `+34` (f,t)/(t,t); dirt → `+0+R3` (f,t) |
| has RIGHT|BOTTOM_RIGHT|BOTTOM — corner missing top+left | beach on top&left combos → `+4+16+R3` (t,f); mixed: beach TOP→`+36`(t,f), beach LEFT→`+37`(t,f), beach TOP_RIGHT→`+33`(t,f), beach BOTTOM_LEFT→`+32`(t,f); all-dirt → `+4+R3` (t,f) |
| has LEFT|BOTTOM_LEFT|BOTTOM — corner missing top+right | same family, flips (f,f) |
| has TOP|TOP_LEFT|LEFT — corner missing right+bottom | same family, flips (f,t) |
| has TOP|TOP_RIGHT|RIGHT — corner missing left+bottom | same family, flips (t,t) |
| has TOP|RIGHT|BOTTOM|LEFT (all 4 sides) but ≥2 diagonal corners missing, ground != WATER | fallback: random plain image, (f,f) — "barely noticeable" (lines 540-550) |
| anything else | **return false** — no image exists |

For **water tiles** the straight-edge/corner cases additionally require the adjacent diagonal bits (e.g. `tileIsNotWater() || hasBits(groundDirection, TOP_LEFT|BOTTOM_LEFT)`, lines 263-264) — water has no images for the missing-diagonal edge cases; land grounds just ignore the diagonals there (TODO comments, lines 266-269 etc.).

### 3.7 Failure recovery — `updateTerrainTransitionOnArea` (`map_format_helper.cpp:612-701`)

Iterates tiles `tileStart..tileEnd` step `tileStep`; for each tile where `updateTerrainTransitionOnTile` failed, it tries replacing the tile's ground with: the painted `newGroundId`, then each distinct neighbor ground, then BEACH (if any water around) or DIRT (otherwise) as last resort (lines 628-659). Each candidate: set random plain image, retry transition; on success re-run transitions on all 8 neighbors (recursing into this function for any neighbor that fails, line 684). If everything fails, revert to original ground (lines 694-699).

### 3.8 Area orchestration — `updateTerrainTransitionOnAreaBoundaries` (`map_format_helper.cpp:703-756`)

Runs `updateTerrainTransitionOnArea` over: the rectangle's inner boundary rows/cols; then the 1-tile-outside rows/cols; then the 4 outside corners. (Interior tiles of a filled rect need no update.) Note `mapHeight = map.width` (line 707) — square maps only.

---

## 4. Roads

### 4.1 Data model

- A road is a `TileObjectInfo` with `group = ObjectGroup::ROADS (=1)` and `index` in **0..512** in the tile's `objects` vector. Serialized exactly like any object (section 2.3). One road object max per tile (`doesContainRoad`, `map_format_helper.cpp:1899-1902`).
- The ROADS object table (`maps/map_object_info.cpp:48+`, `populateRoads`) has **513 entries**: for `index < 256`, index **is the Direction bitmask of neighboring road tiles** (bits per §3.4: TOP_LEFT=1 … LEFT=128). Indices **256..511** are duplicates ("some road sprites have 2 variants", line 53; many `objects[256+i] = objects[i]`). Index **512** = castle entrance road (single part, `OBJ_ICN_TYPE_ROAD` icnIndex 31, line 56-57).
- Each entry consists of `groundLevelParts` on `Maps::TERRAIN_LAYER (=3)` (`maps_tiles.h:50-56`) with ICN `MP2::OBJ_ICN_TYPE_ROAD`; the main sprite is at offset {0,0} and there are auxiliary "spur" sprites placed on *neighboring* tiles (offsets like {0,-1}, {-1,0}, {0,1}) — e.g. index 0 ("no roads around") = icn 2 at {0,0} + icn 22/23 at {∓1,0}... etc. So one road object can paint sprites onto adjacent tiles (same UID).

### 4.2 Placement — `setRoadOnTile` (`map_format_helper.cpp:1805-1824`)

1. `placeNewRoadObjectOnTile` (lines 884-903): no-op if tile already has a road; computes `roadObjectIndex = getRoadObjectIndex(...)`; `setObjectOnTile( world.getTile(tileIndex), objectInfo, false )` (renders into world, allocates new UID via `getNewObjectUID`); `Maps::addObjectToMap( map, tileIndex, ObjectGroup::ROADS, index )` (records TileObjectInfo with `id = getLastObjectUID()`).
2. `updateRoadObjectsAround` → `Maps::updateRoadOnTile` for all 8 neighbors (lines 905-915).
3. If the tile's terrain image has embedded décor → re-roll a plain image (lines 1817-1821).

Editor forbids roads on water (`editor_interface.cpp:1865`).

### 4.3 Index computation — `getRoadObjectIndex` (`map_format_helper.cpp:858-882`)

```cpp
if ( mainTileIndex > map.width && doesContainCastleEntrance( map.tiles[mainTileIndex - map.width] ) )
    return 512;                       // tile directly below a castle entrance (KINGDOM_TOWNS object above)
int roadDirection = 0;
for ( tileIndex : getAroundIndexes( mainTileIndex, map.width, map.width, 1 ) )   // 8 neighbors, map-clipped
    if ( doesContainRoad( map.tiles[tileIndex] ) )
        roadDirection |= GetDirection( mainTileIndex, tileIndex );
return roadDirection + Rand::Get(1) * 256;   // random variant selection
```
`Maps::GetDirection(from,to)` maps index-diffs to Direction bits (`maps/maps.cpp:246-280`). Off-map neighbors simply don't contribute bits (no clamping here, unlike terrain).

### 4.4 Re-sync — `updateRoadOnTile` (`map_format_helper.cpp:1848-1889`)

If the tile has a road object and the freshly computed index differs from the stored one: remove the old world object by UID, temporarily rewind the global UID counter (`setLastObjectUID( iter->id - 1 )`) so the replacement gets the *same* UID, `setObjectOnTile(world tile, newInfo, false)`, restore counter, update `iter->index`. Note: because of the random `+256` variant, "differs" can trigger spuriously/not trigger — cosmetic only. `updateAllRoads` (lines 1891-1897) runs it for the entire map (used by the random map generator, `maps/map_random_generator.cpp:959`, after it placed placeholder road objects with fixed index 2 via `forceTempRoadOnTile`, `map_random_generator_helper.cpp:727-759`).

### 4.5 Removal — `removeRoadFromTile` (`map_format_helper.cpp:1826-1846`)

`removeObjectFromMapByUID` + `world.getTile(tileIndex).updateRoadFlag()` + erase TileObjectInfo + `updateRoadObjectsAround`.

### 4.6 What makes movement treat a tile as road

`Tile::isRoad()` returns `_isTileMarkedAsRoad` (`maps_tiles.h:190-193`), which is set when any ground object part sprite passes `Tile::isSpriteRoad` (`maps_tiles.cpp:801-822`, set in `pushGroundObjectPart` at 824-831 and recomputed in `updateRoadFlag` at 1305-1319):
- `OBJ_ICN_TYPE_ROAD` with icnIndex in `{0,2,3,4,5,6,7,9,12,13,14,16,17,18,19,20,21,26,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48}` (excluded: 1,8,10,11,15,22,23,24,25,27 — these are the spur sprites drawn on neighbor tiles, which must NOT mark that neighbor as road);
- `OBJ_ICN_TYPE_OBJNTOWN` icnIndex in `{13,29,45,61,77,93,109,125,141,157,173,189}` (castle entrances);
- `OBJ_ICN_TYPE_OBJNTWRD` icnIndex `{13,29}` (random town/castle entrances).

So in .fh2m terms: the flag is derived at load time from the ROADS object's sprites (via `readAllTiles` → `readTileObject` → `setObjectOnTile` → `pushGroundObjectPart`), not stored explicitly.

---

## 5. Streams

### 5.1 Data model

- `TileObjectInfo` with `group = ObjectGroup::STREAMS (=2)`, `index` **0..12**. STREAMS table = 13 single-part objects, part = `MP2::OBJ_ICN_TYPE_STREAM` icnIndex == object index, offset {0,0}, `TERRAIN_LAYER` (`map_object_info.cpp:1113-1123`). Purely decorative for gameplay (terrain layer, passability-transparent).
- River deltas are separate objects in `LANDSCAPE_MISCELLANEOUS (=7)`; recognized by first ground part = `OBJ_ICN_TYPE_OBJNMUL2` icnIndex 2 (delta pointing TOP), 11 (BOTTOM), 220 (LEFT), 229 (RIGHT) (`getRiverDeltaDirectionByIndex`, `map_format_helper.cpp:1251-1280`).

### 5.2 Placement — `addStream` (`map_format_helper.cpp:1202-1224`)

Rejects if tile already has a stream or ground is WATER. Then `updateStreamObjectOnMapTile( map, tileId, /*force*/ true )` (computes sprite & adds object), `updateStreamsAround( map, tileId )` (re-syncs the 4 orthogonal neighbors, lines 1226-1234), and re-rolls terrain image if it had embedded décor.

### 5.3 Direction & sprite choice

`getStreamDirecton` (`map_format_helper.cpp:128-156`): bitmask = CENTER + each of LEFT/TOP/RIGHT/BOTTOM whose neighbor has a stream **or** needs a delta connection (`isStreamToDeltaConnectionNeeded`, lines 108-125: the neighbor tile in that direction holds a river delta whose direction is the reflection of the direction of travel). Only orthogonal directions matter.

`getStreamIndex( streamDirection )` (`map_format_helper.cpp:758-810`) — STREAM icn/object index:

| neighbors present (orthogonal) | index | shape |
|---|---|---|
| LEFT+BOTTOM only | 0 | `\` corner |
| RIGHT+BOTTOM only | 1 | `/` corner |
| TOP+RIGHT only | 4 | `\` corner |
| TOP+LEFT only | 7 | `/` corner |
| LEFT+TOP+RIGHT (no BOTTOM) | 8 | `_|_` T |
| TOP+RIGHT+BOTTOM (no LEFT) | 9 | `|-` T |
| TOP+LEFT+BOTTOM (no RIGHT) | 10 | `-|` T |
| LEFT+RIGHT+BOTTOM (no TOP) | 11 | `\/` T |
| all four | 6 | `-|-` cross |
| LEFT or RIGHT only (no TOP/BOTTOM) | 2 or 5 (random `Rand::Get(1)`) | horizontal |
| anything else (incl. isolated tile) | 3 or 12 (random) | vertical |

`updateStreamObjectOnMapTile` (`map_format_helper.cpp:812-851`): if the tile has no stream yet → `setObjectOnTile( world tile, STREAMS objectInfo, true )` + `addObjectToMap`; else just update `streamIter->index` and mirror to the world via `Tile::updateTileObjectIcnIndex( worldTile, uid, objectIndex )`.

`updateStreamsToDeltaConnection` (`map_format_helper.cpp:1236-1249`): after placing/removing a delta at `tileId`, refreshes the stream two tiles away along `deltaDirection` (the tile beyond the delta's center).

Streams can't be placed on water; erasing terrain to water removes them via `_validateObjectsOnTerrainUpdate` (STREAMS listed in the "cannot be placed on water" group, `editor_interface.cpp:728-733`).

---

## 6. Movement costs

### 6.1 Terrain penalty — `Maps::Ground::GetPenalty( tile, pathfindingLevel )` (`ground.cpp:169-242`)

Constants (`ground.h:72-76`): `roadPenalty = 75`, `defaultGroundPenalty = 100`, `fastestMovePenalty = 75`, `slowestMovePenalty = 200`. Skill levels (`heroes/skill.h:57-60`): NONE=0, BASIC=1, ADVANCED=2, EXPERT=3.

Result = 100 + surcharge (comment table at ground.cpp:171-181):

| Ground | NONE | BASIC | ADVANCED | EXPERT |
|---|---|---|---|---|
| Desert | 200 | 175 | 150 | 100 |
| Swamp | 175 | 150 | 125 | 100 |
| Snow | 150 | 125 | 100 | 100 |
| Wasteland | 125 | 100 | 100 | 100 |
| Beach | 125 | 100 | 100 | 100 |
| Lava / Dirt / Grass / Water | 100 | 100 | 100 | 100 |
| Road (any pathfinding) | 75 | 75 | 75 | 75 |

### 6.2 Pathfinder — `WorldPathfinder::getMovementPenalty` (`world/world_pathfinding.cpp:332-367`)

```cpp
uint32_t penalty = fromTile.isRoad() && toTile.isRoad() ? Maps::Ground::roadPenalty
                                                        : Maps::Ground::GetPenalty( fromTile, _pathfindingSkill );
// Diagonal movement costs 50% more
if ( Direction::isDiagonal( direction ) ) penalty = penalty * 3 / 2;
```
- Road rate (75) applies only when **both** source and destination tiles are roads; otherwise the **source** tile's ground penalty is charged.
- Diagonal = ×1.5 (integer `*3/2`), applied after road/ground choice (diagonal road step = 112).
- "Last move" rule (lines 344-364): if remaining MP ≥ cost of straight move over the source tile but < the computed penalty, the move is allowed consuming all remaining MP.
- AI override (`AIWorldPathfinder::getMovementPenalty`, lines 814+) adds an extra estimated penalty when passing through pickup/stay-front objects — heuristic only.

---

## 7. Feasibility of an offline .fh2m generator (question 8)

**Decision logic is MapFormat-pure; execution is world-entangled.**

- Every *decision* in the terrain/road/stream pipeline reads only `map.tiles[i].terrainIndex` and `map.tiles[i].objects` (`getGroundDirecton`, `updateTerrainTransitionOnTile`, `setTerrainBoundaries`, `getRoadObjectIndex`, `getStreamDirecton`, `getStreamIndex`). None of it reads world tiles for decisions.
- But the *mutation* helpers also mirror into the global `world` singleton: `setTerrain` calls `world.getTile(tileId).setTerrain(...)` (`map_format_helper.cpp:207`); road/stream placement calls `Maps::setObjectOnTile( world.getTile(...), ... )` and the global UID counter (`world_object_uid`): `getNewObjectUID` / `getLastObjectUID` / `setLastObjectUID`. `setTerrainWithTransition` even asserts `map.width == world.w()` (line 1052).
- Therefore two viable strategies:
  1. **Link against the engine** and do what the editor/RMG does: `world.generateUninitializedMap(width)`; fill `MapFormat.tiles`; then reuse `setTerrainOnTile` everywhere + `setTerrainWithTransition` (or per-tile 1×1 calls on region borders exactly like the built-in random map generator: plain `setTerrainOnTile` per region node, then `setTerrainWithTransition(map, idx, idx, ground)` per border node — `maps/map_random_generator.cpp:735-775`), then `setRoadOnTile`/`addStream`, or place placeholder ROADS index-2 objects and finish with `updateAllRoads` (RMG pattern, `map_random_generator_helper.cpp:727-759` + `map_random_generator.cpp:959`). Finally `Maps::Map_Format::saveMap(path, map)`.
  2. **Re-implement offline** (straightforward): the algorithm is fully specified in this note — paint random plain indices (§1.4), then run §3.5-3.8 over boundary tiles; compute road indices as neighbor-direction masks (+optional 256, or just always use base 0..255) and stream indices per §5.3; assign object UIDs yourself as any unique nonzero uint32 (load path sorts objects by ascending id and replays them via `setLastObjectUID(id-1)` + `setObjectOnTile` — `map_format_helper.cpp:1018-1047`; uniqueness matters, order = id order).
- **Critical caveat: nothing fixes bad data on load.** The game renders exactly the stored `terrainIndex/terrainFlags` and road/stream indices; `updateRoadOnTile`/transition code runs only during editing. An offline generator that writes plain tiles everywhere without computing transitions produces a playable but visually unblended map; wrong road indices render disconnected road sprites (passability/road flag still works, since the flag derives from the road sprites placed, §4.6).
- Randomness uses the global engine RNG (`Rand::Get`), so byte-identical reproduction of the editor's choices is not required — any value in the documented random ranges is valid.
- Also remember `setTerrain`'s embedded-décor guard (§3.3) and the road/stream rule: terrain under roads/streams must use non-embedded plain images.

---

## 8. Quick reference — key functions & locations

| What | Where |
|---|---|
| Ground enum, start indices, penalties | `maps/ground.h:34-76` |
| getGroundByImageIndex / isTerrainTransitionImage / embedded check / random image | `maps/ground.cpp:61-141, 244-291` |
| TileInfo/TileObjectInfo structs | `maps/map_format_info.h:42-57` |
| TileInfo serialization | `maps/map_format_info.cpp:599-607` |
| fh2m save (magic, zlib, field order, version=13) | `maps/map_format_info.cpp:101-110, 438-504, 824-842` |
| setTerrain (flags encoding, world mirror) | `maps/map_format_helper.cpp:191-208` |
| setTerrainBoundaries (transition images) | `maps/map_format_helper.cpp:221-557` |
| updateTerrainTransitionOnTile | `maps/map_format_helper.cpp:560-610` |
| updateTerrainTransitionOnArea (+recovery) | `maps/map_format_helper.cpp:612-701` |
| updateTerrainTransitionOnAreaBoundaries | `maps/map_format_helper.cpp:703-756` |
| setTerrainWithTransition / setTerrainOnTile | `maps/map_format_helper.cpp:1050-1087, 1197-1200` |
| getRoadObjectIndex / placeNewRoadObjectOnTile | `maps/map_format_helper.cpp:858-903` |
| setRoadOnTile / removeRoadFromTile / updateRoadOnTile / updateAllRoads / doesContainRoad | `maps/map_format_helper.cpp:1805-1902` |
| ROADS object table (513 entries) | `maps/map_object_info.cpp:48-...` (populateRoads) |
| STREAMS object table (13 entries) | `maps/map_object_info.cpp:1113-1123` |
| getStreamDirecton / getStreamIndex / updateStreamObjectOnMapTile | `maps/map_format_helper.cpp:128-156, 758-851` |
| addStream / updateStreamsAround / delta connection | `maps/map_format_helper.cpp:1202-1285` |
| isSpriteRoad / updateRoadFlag / isStream | `maps/maps_tiles.cpp:801-822, 1305-1319, 1149-1161` |
| Tile terrain accessors / _isTileMarkedAsRoad | `maps/maps_tiles.h:156-166, 190-196, 324-338, 400-415` |
| Ground::GetPenalty | `maps/ground.cpp:169-242` |
| Pathfinder penalty (road, diagonal ×1.5) | `world/world_pathfinding.cpp:332-367` |
| Editor terrain/road/stream brushes | `editor/editor_interface.cpp:1817-1898, 1967-1982` |
| Object validation after terrain change | `editor/editor_interface.cpp:3350-3391, 687-...` |
| RMG offline-style painting pattern | `maps/map_random_generator.cpp:735-775, 959`; `maps/map_random_generator_helper.cpp:727-759` |
| Direction bit values | `heroes/direction.h:35-73` |
| Big-endian streams, vector u32-count, enum-as-underlying | `engine/serialize.h:194-203, 344-377`; `engine/serialize.cpp:149-157` |
