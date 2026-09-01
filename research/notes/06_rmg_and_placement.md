# 06 — RMG helpers and editor object placement (fheroes2 @ b086d1aa, 2026-09-01)

All paths relative to `C:/Users/gjoer/source/repos/fheroes2/`.

Files studied:
- `src/fheroes2/maps/map_random_generator.{h,cpp}` (76 / 982 lines)
- `src/fheroes2/maps/map_random_generator_helper.{h,cpp}` (106 / 1236 lines)
- `src/fheroes2/maps/map_random_generator_info.{h,cpp}` (319 / 237 lines)
- `src/fheroes2/maps/map_format_helper.{h,cpp}` (144 / 2085 lines)
- `src/fheroes2/editor/editor_interface.cpp` (placement paths, `_placeCastle`, `saveMapToFile`)
- `src/fheroes2/maps/maps_tiles_helper.cpp` (`setObjectOnTile`, `placeObjectOnTile`, `setMonsterOnTile`)
- `src/fheroes2/maps/map_object_info.{h,cpp}` (object tables), `src/fheroes2/gui/ui_map_object.cpp` (basement/mine index helpers)
- `src/fheroes2/world/world_object_uid.{h,cpp}`, `src/fheroes2/world/world_loadmap.cpp` (`World::loadResurrectionMap`)

---

## 1. UID management (`world_object_uid`)

`src/fheroes2/world/world_object_uid.cpp` — the entire mechanism is one global counter:

```cpp
namespace { uint32_t objectCounter{ 0 }; }
void Maps::resetObjectUID()               { objectCounter = 0; }
uint32_t Maps::getNewObjectUID()          { ++objectCounter; return objectCounter; }
uint32_t Maps::getLastObjectUID()         { return objectCounter; }
void Maps::setLastObjectUID( const uint32_t uid ) { objectCounter = uid; }
```

- UIDs are **1-based**; UID 0 is invalid (`readTileObject` rejects `object.id == 0`, map_format_helper.cpp:1038-1042).
- `EditorInterface::generateNewMap` calls `Maps::resetObjectUID()` after clearing the map (editor_interface.cpp:3169).
- The UID is consumed inside `placeObjectOnTile` (maps_tiles_helper.cpp:229: `const uint32_t uid = Maps::getNewObjectUID();`) or, for monsters, inside `setMonsterOnTile` (maps_tiles_helper.cpp:1740: `mainObjectPart._uid = getNewObjectUID();`).
- **Compound objects share one UID** by rewinding the counter: `setLastObjectUID( uid - 1 )` before placing the next part, so the next `getNewObjectUID()` returns the same value again (see castle recipe below).
- On load, `readTileObject` (map_format_helper.cpp:1028-1048) does `setLastObjectUID( object.id - 1 )` then `setObjectOnTile(tile, objectInfos[object.index], false)` — so after a full load the counter equals the max UID present in the file and future placements never collide.

## 2. `Maps::addObjectToMap` (map_format_helper.cpp:1089-1195)

Signature: `void addObjectToMap( Map_Format::MapFormat & map, const int32_t tileId, const ObjectGroup group, const uint32_t index )`.

Behavior (exact):
1. `const uint32_t uid = getLastObjectUID(); assert(uid > 0);` — **precondition: the object was already put into `world` via `Maps::setObjectOnTile()`** (comment at line 1093), which bumped the counter. `addObjectToMap` only *reads* the counter.
2. `addObjectToTile(map.tiles[tileId], group, index, uid)` (lines 60-66): `map.tiles[tileId].objects.emplace_back()` with `object.id = uid; object.group = group; object.index = index;` — objects are always **appended** to the tile's vector (append order = render order for equal UIDs).
3. Metadata auto-creation, keyed by UID:
   - `KINGDOM_HEROES` → `map.heroMetadata.try_emplace(uid)`; `heroMetadata.race = Race::IndexToRace( objects[index].metadata[1] )` (lines 1100-1108).
   - `KINGDOM_TOWNS` → `map.castleMetadata.try_emplace(uid)`; `builtBuildings.push_back( objects[index].metadata[1] == 0 ? BUILD_TENT : BUILD_CASTLE )` (lines 1109-1117).
   - Jail (`isJailObject`) → `heroMetadata.try_emplace(uid)`, race = `Race::RAND` (lines 1118-1124).
   - `MONSTERS` → `map.monsterMetadata.try_emplace(uid)` (lines 1125-1132).
   - `ADVENTURE_MISCELLANEOUS`: `OBJ_EVENT` → `adventureMapEventMetadata`, `OBJ_SIGN` → `signMetadata`, `OBJ_SPHINX` → `sphinxMetadata` (lines 1133-1170).
   - `ADVENTURE_WATER` with `OBJ_BOTTLE` → `signMetadata` (lines 1171-1184).
   - `ADVENTURE_ARTIFACTS` → `artifactMetadata.try_emplace(uid)` (lines 1185-1194).
   - **NOT auto-created**: `resourceMetadata` (created by the editor by hand for `OBJ_RESOURCE`, editor_interface.cpp:2951-2956: `_mapFormat.resourceMetadata[insertedObject.id].count = 0;` — 0 = engine default amount) and `capturableObjectsMetadata` (created only via the ownership dialog, editor_interface.cpp:2558).

`TileObjectInfo` fields in declaration order (map_format_info.h:42-49):
```cpp
struct TileObjectInfo {
    uint32_t id{ 0 };
    ObjectGroup group{ ObjectGroup::NONE };
    uint32_t index{ 0 };
};
```
`TileInfo` (map_format_info.h:51-57): `uint16_t terrainIndex{0}; uint8_t terrainFlags{0}; std::vector<TileObjectInfo> objects;`

## 3. Placement primitive `Maps::setObjectOnTile` (maps_tiles_helper.cpp:2214-2284)

`bool setObjectOnTile( Tile & tile, const ObjectInfo & info, const bool updateMapPassabilities )` — puts the object into the **live `world` tiles** (all ground/top parts, one new UID) and optionally recomputes passability. Special cases:
- `OBJ_MONSTER` → `setMonsterOnTile(tile, info.metadata[0], 0)` + `setMonsterCountOnTile(tile, 0)` (count 0 = "random amount" sentinel fix); UID consumed at maps_tiles_helper.cpp:1740.
- `OBJ_ARTIFACT` → `tile.metadata()[0] = info.metadata[0]` (artifact id).
- `OBJ_ALCHEMIST_LAB/OBJ_MINE/OBJ_SAWMILL` → `tile.metadata()[0] = info.metadata[0]` (resource), `[1] = info.metadata[1]` (daily income).
- `OBJ_MAGIC_GARDEN` → metadata 1/1.
- Everything else → plain `placeObjectOnTile`.

`placeObjectOnTile` (maps_tiles_helper.cpp:195-…): validates that for **action objects** every non-shadow ground part and every top part is inside the map, then `uid = getNewObjectUID()` and pushes every part into the corresponding `world` tile (out-of-map non-action parts are silently dropped).

The editor wrapper `EditorInterface::_setObjectOnTile` (editor_interface.cpp:3095-3113) = `Maps::setObjectOnTile(tile, objectInfo, true /*update passability*/)` + `Maps::addObjectToMap(_mapFormat, tile.GetIndex(), groupType, index)`.
The RMG wrapper `putObjectOnMap` (map_random_generator_helper.cpp:761-795) = same but `setObjectOnTile(..., false)` (passability deferred), plus two guards: refuses a second off-game action object on the same tile (scans `mapFormat.tiles[i].objects` via `MP2::isOffGameActionObject`), and refuses re-placing the identical action object type (`tile.getMainObjectType() == objectInfo.objectType`).

## 4. Occupancy / collision tracking

Two independent systems:

**(a) Editor** — checks against the live `world` (editor_interface.cpp):
- `isObjectPlacementAllowed(info, mainTilePos)` (lines 306-334): for action objects only, every non-shadow ground part and every top part must be inside the map (non-action objects may hang over the edge).
- `isActionObjectAllowed(info, mainTilePos)` (lines 336-396): for every non-shadow/non-terrain ground part, the target world tile must not already contain an off-game action object (`MP2::isOffGameActionObject(tile.getMainObjectType())`); *removable* action objects (OBJ_ARTIFACT, OBJ_BARREL, OBJ_BARRIER, OBJ_BOTTLE, OBJ_CAMPFIRE, OBJ_EVENT, OBJ_FLOTSAM, OBJ_HERO, OBJ_GENIE_LAMP, OBJ_JAIL, OBJ_MONSTER, OBJ_RESOURCE, OBJ_SEA_CHEST, OBJ_SHIPWRECK_SURVIVOR, OBJ_TREASURE_CHEST) additionally require `Maps::isClearGround(tile)`.
- `verifyTerrainPlacement` (lines 687-823): per-group water/land rule using `checkConditionForUsedTiles` over `Maps::getGroundLevelUsedTileOffset(info)` (heroes: any terrain; most land groups: `!isWater()`; ADVENTURE_WATER/LANDSCAPE_WATER: `isWater()`; river deltas & OBJ_EVENT: anywhere; KINGDOM_TOWNS: entrance + all used tiles of town *and* auto-chosen basement not water; Ultimate artifact: `isSuitableForUltimateArtifact()`).
- `verifyObjectPlacement` (lines 825-898) = the composition used before every editor placement; for towns also enforces `world.getCastleCount() < AllCastles::getMaximumAllowedCastles()`.
- Count limits: heroes ≤ `AllHeroes::getMaximumAllowedHeroes()` total incl. jails, ≤ `GameStatic::GetKingdomMaxHeroes()` per color (lines 2696-2733); 1 Random Ultimate Artifact; obelisks ≤ `numOfPuzzleTiles`.

**(b) RMG** — a parallel occupancy grid, `MapStateManager` (map_random_generator_info.h:80-190): one `Node {int index; uint32_t region; NodeType type}` per tile, `NodeType : uint8_t { OPEN, BORDER, ACTION, OBSTACLE, CONNECTOR, PATH, COAST }` (map_random_generator_info.h:52-61). Supports nested transactions (`MapStateTransaction`) with rollback for speculative planning.
- `canPlaceObject` (map_random_generator_helper.cpp:239-281, file-local): iterates non-shadow/non-terrain ground parts (`iterateOverObjectParts`); each part's node must exist (`index != -1`), be in a region (`region != 0`), not be ACTION/PATH, and be OPEN (BORDER tolerated for non-action objects). Top parts only need to be on-map & in-region. For action objects the tile **below the main tile** `(0,+1)` must be OPEN or PATH — that is the action-tile accessibility check.
- `canPlaceBorderObstacle` (lines 660-687): like above but allows OPEN/BORDER/OBSTACLE.
- `canPlaceAllObjects` (689-695): all `ObjectPlacement`s of a set placeable (after `selectTerrainVariantForObject` remaps tree indices 0-5 and mountain indices 0-7 to the terrain-specific variants, lines 408-418; tree base offsets per ground at 128-156, mountain offsets at 158-186).
- `canFitObjectSet` (697-713): every `entranceCheck` offset must be OPEN, then obstacles+valuables+`randomMonsterSet` (`{{0,0}, MONSTERS, 0}` line 74) must all pass `canPlaceAllObjects`.
- `markObjectPlacement` (715-724): marks every non-shadow ground part OBSTACLE, then the main tile ACTION if `MP2::isOffGameActionObject(info.objectType)`. `markNodeIndexAsType` (221-230) never overrides BORDER/ACTION and never puts PATH on OBSTACLE.
- Additional accessibility guarantee: `placeActionObject` and the planners require a non-empty `findPathToNearestRoad(...)` result before committing (see §7), i.e. every action object must be road-reachable.

## 5. Castle compound — exact sequence

### RMG: `placeCastle` (map_random_generator_helper.cpp:824-895)

Constants (lines 62-64): `randomCastleIndex = 12`, `randomTownIndex = 13`, `randomHeroIndex = 7`.

```
tile = world.getTile(x, y);  if (tile.isWater()) fail;
basementId  = fheroes2::getTownBasementId( tile.GetGround() );
castleObjectId = isCastle ? 12 : 13;                       // KINGDOM_TOWNS index
canPlaceObject(basement) && canPlaceObject(castle) else fail;

putObjectOnMap(map, tile, LANDSCAPE_TOWN_BASEMENTS, basementId);   // consumes UID -> N
objectId = getLastObjectUID() - 1;   // N-1
setLastObjectUID(objectId);
putObjectOnMap(map, tile, KINGDOM_TOWNS, castleObjectId);          // re-consumes -> N (same UID)
     // addObjectToMap creates castleMetadata[N], builtBuildings = {BUILD_CASTLE or BUILD_TENT}
if (color == PlayerColor::NONE) setDefaultCastleDefenderArmy(map.castleMetadata[getLastObjectUID()]);
setLastObjectUID(objectId);
putObjectOnMap(map, world.getTile(idx-1), LANDSCAPE_FLAGS, Color::GetIndex(color)*2);     // left flag, UID N
setLastObjectUID(objectId);
putObjectOnMap(map, world.getTile(idx+1), LANDSCAPE_FLAGS, Color::GetIndex(color)*2 + 1); // right flag, UID N
bottomIndex = GetDirectionIndex(idx, Direction::BOTTOM);
if (color != NONE)
    putObjectOnMap(map, world.getTile(bottomIndex), KINGDOM_HEROES, Color::GetIndex(color)*7 + 6); // NEW UID N+1
markObjectPlacement(basementInfo); markObjectPlacement(castleInfo);
forceTempRoadOnTile(bottomIndex); forceTempRoadOnTile(bottomIndex+BOTTOM);   // seed roads out of the castle
```

### Editor: `EditorInterface::_placeCastle(posX, posY, color, type)` (editor_interface.cpp:3288-3348)

Identical UID dance, with differences:
1. `_setObjectOnTile(tile, LANDSCAPE_TOWN_BASEMENTS, getTownBasementId(tile.GetGround()))` (line 3300).
2. `objectId = getLastObjectUID() - 1; setLastObjectUID(objectId);` then `_setObjectOnTile(tile, KINGDOM_TOWNS, type)` (3307-3312) — `type` is the *unpacked* town index; the panel packs it as `packedType = color*townObjects.size() + type` and `EditorPanel::getTownObjectProperties` unpacks with `type = packed % size; color = packed / size;` (editor_interface_panel.cpp:1512-1525).
3. `Maps::updateRoadOnTile(_mapFormat, bottomIndex)` — refresh the road in front of the entrance (3316-3321).
4. Neutral only: `setDefaultCastleDefenderArmy(_mapFormat.castleMetadata[getLastObjectUID()])` (3324-3326).
5. `setLastObjectUID(objectId)`; left flag `LANDSCAPE_FLAGS, Color::GetIndex(color)*2` on `tile.GetIndex()-1`; `setLastObjectUID(objectId)`; right flag `*2+1` on `tile.GetIndex()+1` (3330-3340).
6. `world.addCastle(tile.GetIndex(), Race::IndexToRace(townObjectInfo.metadata[0]), color)` (3342-3345) — live-world Castle instance for editing (not stored in MapFormat).
No hero is auto-placed by the editor (unlike the RMG). Caller then runs `Maps::updateMapPlayers(_mapFormat)` when it's a new object (editor_interface.cpp:2844-2846).

### Object tables involved

- `LANDSCAPE_TOWN_BASEMENTS` (map_object_info.cpp:3772-3793): 8 entries, one per ground in order **grass=0, snow=1, swamp=2, lava=3, desert=4, dirt=5, wasteland=6, beach=7** (`getTownBasementId`, ui_map_object.cpp:295-326). Footprint: ICN `OBJNTWBA`, ground parts at (0,0),(−2..2,0),(−2..2,1); the part at (0,1) (icnOffset+7) is `SHADOW_LAYER` → passable entrance below the gate. Object type `OBJ_NON_ACTION_CASTLE`.
- `KINGDOM_TOWNS` (map_object_info.cpp:5834-5899): 14 entries — `for i in 0..11: race=i/2, isCastle=(i%2==0)` (0=Knight castle, 1=Knight town, 2=Barbarian castle, … 11=Necromancer town), then 12=Random castle, 13=Random town. Main object type `OBJ_CASTLE` (`OBJ_RANDOM_CASTLE`/`OBJ_RANDOM_TOWN` for 12/13). `metadata[0] = race` (index for `Race::IndexToRace`: 0 KNGT … 5 NECR, 6 MULT, 7 RAND — race.cpp:150-174), `metadata[1] = isCastle ? 1 : 0` (5880-5881). Ground action part at (0,0); other OBJECT_LAYER parts at (−2,0),(−1,0),(1,0),(2,0),(−1,−1),(0,−1),(1,−1); 16 shadow parts reaching (−5,−1); top parts up to (0,−3).
- `LANDSCAPE_FLAGS` (map_object_info.cpp:3795-3823): 14 entries = 7 colors × {left,right}: `index = colorIndex*2 (+1 for right)`, ICN `FLAG32`, `metadata[0] = colorIndex`. Color index mapping (color.cpp:102-169): 0 BLUE, 1 GREEN, 2 RED, 3 YELLOW, 4 ORANGE, 5 PURPLE, **6 = NONE/neutral (gray)**.
- `KINGDOM_HEROES` (map_object_info.cpp:5811-5832): 42 entries = 6 colors × 7 "races"; `index = color*7 + race`, ICN `MINIHERO` icnIndex `color*7+race`, `metadata[0] = color`, `metadata[1] = race` where the 7th slot (race index 6) is stored as `metadata[1] = 7` = `Race::RAND` (lines 5822-5827). **A hero's color is bound purely by which object index you place** — `updatePlayerRelatedObjects` reads `heroObjects[object.index].metadata[0]` (map_format_helper.cpp:1306-1316); no per-instance color metadata exists.

### Castle ownership resolution

`Maps::getTownColorIndex(map, tileIndex, uid)` (map_format_helper.cpp:1670-1717): scans `map.tiles[tileIndex-1].objects` and `[tileIndex+1].objects` for a `LANDSCAPE_FLAGS` object **with the same UID as the town**, reads `flagObjects[index].metadata[0]` from both sides; if they differ → `assert(0)` and return 0. So ownership = flag color, linkage = shared UID. Changing owner in the editor is done by replacing both flag objects' indices (`_updateObjectTypesInMap`/flag replacement around editor_interface.cpp:1299).

## 6. `placeMine`, `placeMonster`, `placeActionObject`, `placeSimpleObject`, treasures

- `placeActionObject(mapFormat, data, tile, groupType, type)` (map_random_generator_helper.cpp:797-822): `canPlaceObject` → transaction → `markObjectPlacement` → `findPathToNearestRoad` (must be non-empty) → `putObjectOnMap` → lay temp road along the path → commit.
- `placeMine(mapFormat, data, economy, tileOptions, resource, monsterStrength)` (897-912): for each candidate tile: `mineType = fheroes2::getMineObjectInfoId(resource, mineTile.GetGround())`, `placeActionObject(..., ADVENTURE_MINES, mineType)`; on success `economy.increaseMineCount(resource)` and **guard**: `placeMonster(map, data, GetDirectionIndex(nodeIndex, Direction::BOTTOM), getMonstersByValue(monsterStrength, mineValue))` — i.e. the guard stands **one tile below the mine entrance**; mineValue from the fake-MP2 mapping `WOOD/ORE→OBJ_SAWMILL(1000), SULFUR/CRYSTAL/GEMS/MERCURY→OBJ_MINE(3500), GOLD→OBJ_ABANDONED_MINE(7500)` (lines 188-207, table 82-126). Returns the mine tile index or −1.
  - `getMineObjectInfoId(resource, ground)` (ui_map_object.cpp:328-365) over the 56-entry `ADVENTURE_MINES` table: ore/sulfur/crystal/gems/gold = `groundIndex*5 + {0,1,2,3,4}` with groundIndex = generic/beach 0, grass 1, snow 2, swamp 3, lava 4, desert 5, dirt 6, wasteland 7; abandoned = `40 + groundIndex`; sawmill = `48 + {grass/swamp 0, snow 1, lava 2, desert/beach 3, dirt 4, wasteland 5}`; alchemist (mercury) = snow ? 55 : 54. Mine `ObjectInfo.metadata` = {resource, income}: ORE {ORE,2}, SULFUR {SULFUR,1}, CRYSTAL {CRYSTAL,1}, GEMS {GEMS,1}, GOLD {GOLD,1000} (map_object_info.cpp:4253-4282). Mine action tile (0,0); OBJECT_LAYER parts (1,−1),(−1,0),(1,0); shadows to (−3,−1); tops (−1,−1),(0,−1); the resource cart is an `EXTRAOVR` part at (0,0).
- `placeMonster(mapFormat, data, index, monster)` (936-956): `putObjectOnMap(map, world.getTile(index), MONSTERS, Monster(monster.monsterId).GetSpriteIndex())` (`GetSpriteIndex() = id - 1`, monster.h:301-304; MONSTERS table index i = monster id i+1, map_object_info.cpp:5901-5960), then marks node ACTION; if `monster.allowedMonsters` non-empty, finds the MONSTERS object on the tile and sets `mapFormat.monsterMetadata[info.id].selected = monster.allowedMonsters`. `getMonstersByValue(strength, value)` (304-363) buckets gold value (±1500/2500 by strength) into RANDOM_MONSTER_LEVEL_1..4 sprites plus a curated `selected` list for values ≥ 5500.
- `placeSimpleObject(mapFormat, data, centerNode, placement, ground)` (958-973): position = `GetPoint(centerNode.index) + placement.offset`; terrain-variant remap; `putObjectOnMap`; `markObjectPlacement`. Used for obstacles, decorations, treasures.
- Treasures: prefab `ObjectSet`s (obstacles / valuables / entranceCheck offsets) declared in map_random_generator.cpp:113-211 (valuable placeholder = `ADVENTURE_TREASURES, 9` = Treasure Chest); planning via `planObjectPlacement` (1014-1078, transactional; requires road path; decrements `region.treasureLimit`); actual placement `placeValidTreasures` (1080-1112): obstacles via `placeSimpleObject`, each non-power-up valuable replaced by `getRandomTreasure(valueLimit, rng)` (365-381: chest/random-resource/campfire/random artifact T/m/M filtered by remaining value; `convertMP2ToObjectInfo` maps MP2 type → (group,index) by lazily scanning ADVENTURE_{ARTIFACTS,DWELLINGS,MINES,POWER_UPS,TREASURES}+MONSTERS tables, 383-406), then a guard monster on the set's center tile via `getMonstersByValue(strength, groupValue)`. Free pickups (generateMap step 11, map_random_generator.cpp:936-957): `putObjectOnMap(map, world.getTile(idx), ADVENTURE_TREASURES, 8 /*OBJ_RANDOM_RESOURCE*/)` near PATH tiles.
  - `ADVENTURE_TREASURES` index map (map_object_info.cpp:4743-4823): 0..6 = `OBJ_RESOURCE` wood, mercury, ore, sulfur, crystal, gems, gold (`metadata[0]` = resource id); 7 = Genie's Lamp; 8 = Random resource; 9 = Treasure chest; 10..12 = campfires (multi-terrain variants).
- `placeBorderObstacle` (914-934): shuffled per-ground `LANDSCAPE_TREES` indices (`obstaclesPerGround`, lines 68-72: grass {0-5}, lava {6-11}, wasteland {12-17}, swamp/dirt {18-23}, desert/beach {24-29}, snow {30-35}) with `canPlaceBorderObstacle` + `putObjectOnMap` + `markObjectPlacement`.
- `placeDecorations` (1178-1235): `DecorationSet` prefabs on evenly spaced OPEN tiles away from non-OPEN neighbors.

## 7. RMG road generation

- Region connections: `Region::checkNodeForConnections` (map_random_generator_info.cpp:163-225) — a BORDER node adjacent to exactly one other region, not water, whose two-away tile is OPEN, becomes a CONNECTOR and is recorded in both regions' `connections` maps (regionId → tileIndex).
- Castles seed PATH: `placeCastle` forces temp road on the two tiles below the entrance (map_random_generator_helper.cpp:887-892); `adjustRegionToFitCastle` clamps the castle to ≥4 tiles from map edges and moves `centerIndex` 2 tiles below the entrance (map_random_generator_info.cpp:227-236).
- `findPathToNearestRoad(nodes, mapWidth, regionId, start)` (map_random_generator_helper.cpp:420-537): Dijkstra-ish BFS within the region over OPEN/PATH/CONNECTOR nodes; cost 100 straight / 150 diagonal (`Ground::defaultGroundPenalty = 100`, ground.h:73); target = nearest node of type PATH reached by a straight (non-diagonal) final step; explores up to bestCost+300 margin; forces straight (non-diagonal, non-TOP) exit from ACTION start tiles; forbids tight diagonal zigzags; inserts an extra 0-cost fix-up tile after BOTTOM_LEFT/BOTTOM_RIGHT steps to compensate for missing road sprites.
- `forceTempRoadOnTile(data, mapFormat, tileIndex)` (727-759): marks PATH; if no road on the tile: strips embedded-object terrain image, `Maps::getNewObjectUID()` (just to advance the counter) then `Maps::addObjectToMap(mapFormat, tileIndex, ROADS, 2)` — a **placeholder road** (index 2 = straight vertical), *not* written into `world` ("Wouldn't render correctly but will speed up placement", line 726).
- generateMap step 7 (map_random_generator.cpp:873-881): for each region connection, mark connector PATH and lay a temp road along `findPathToNearestRoad`; step 10 places guard monsters on connector tiles (strongGuard if same RegionType on both sides, else weakGuard; thresholds 3500/4500 & 6000/7500 by map size, lines 921-934).
- Final pass: `Maps::updateAllRoads(mapFormat)` (map_format_helper.cpp:1891-1897) → `updateRoadOnTile` per tile (1848-1889): computes the correct `ROADS` index via `getRoadObjectIndex` (858-882): `512` if the tile above holds a castle entrance (`doesContainCastleEntrance` = any KINGDOM_TOWNS object on that tile); otherwise a bitmask of `Maps::GetDirection` toward all neighboring road tiles, plus `Rand::Get(1) * 256` (sprite variants 256-511 duplicate 0-255). It then swaps the object in `world` while preserving the UID: `removeObjectFromMapByUID`, `setLastObjectUID(iter->id - 1)`, `setObjectOnTile(...)`, restore counter, `iter->index = roadObjectIndex` (1868-1886). The `ROADS` table has 513 entries; parts are `TERRAIN_LAYER`, ICN `ROAD` (map_object_info.cpp:48-88).
- Editor equivalents: `setRoadOnTile` (map_format_helper.cpp:1805-1824, places properly-indexed road + `updateRoadObjectsAround`), `removeRoadFromTile` (1826-1846).

## 8. `generateMap` overall pipeline (map_random_generator.cpp:621-981)

1. `Interface::EditorInterface::Get().generateNewMap(width)` — resets `_mapFormat`, `world.generateUninitializedMap(width)`, all-water terrain, `Maps::resetObjectUID()` (editor_interface.cpp:3133-3176). **This is the RMG's only hard Editor/Interface dependency.**
2. Region layout on 2 rings (players on outer ring), `Region::regionExpansion` competition growth (map_random_generator_info.cpp:108-161).
3. `Maps::setTerrainOnTile` per node; `setTerrainWithTransition` on borders; player regions: `placeCastle(..., isCastle=true)`; primary WOOD+ORE mines from jittered rings (`findOpenTilesSortedJittered`); large neutral regions get castles only when density = ABUNDANT.
4. `Maps::updatePlayerRelatedObjects(mapFormat)`; connectors; border obstacles.
5. Secondary/gold mines (`pickEvenlySpacedTiles` + `findTilesForPlacement`); roads to connections.
6. Power-ups + planned treasure sets; decorations; connector guard monsters; random-resource pickups.
7. Finalize: `Maps::updateAllRoads(mapFormat)`, `Maps::updatePlayerRelatedObjects(mapFormat)`, `Maps::updateMapPlayers(mapFormat)`, `world.updatePassabilities()` (lines 959-971); sets `mapFormat.name/description`.

## 9. Finalize pass before saving

- Editor `saveMapToFile` (editor_interface.cpp:3204-3273): **only** `Maps::updateMapPlayers(_mapFormat)` is mandatory before `Maps::Map_Format::saveMap(fullPath, _mapFormat)`. No render-order sort and no passability write-back happen at save time (passability lives only in `world` and is recomputed on load; render order is reconstructed at load by sorting all TileObjectInfo by UID — `readAllTiles` uses a `std::multiset` keyed on `info->id`, map_format_helper.cpp:948-1023, stable for equal UIDs → tile-index/append order preserved within a compound).
- `Maps::updateMapPlayers` (map_format_helper.cpp:1336-1668) recomputes from the tile objects: `availablePlayerColors`, `playerRace[6]` (hero colors from `heroObjects[index].metadata[0/1]`; town colors from `getTownColorIndex` flags; multiple races → MULT; RAND wins), keeps/extends `humanPlayerColors`/`computerPlayerColors`, sanitizes alliances, resets invalid VICTORY_CAPTURE_TOWN / VICTORY_KILL_HERO / LOSS_TOWN / LOSS_HERO metadata, masks event colors, and erases `capturableObjectsMetadata` whose owner color is no longer available (this branch calls `world.CaptureObject(tileIndex, PlayerColor::NONE)` — the **only** `world` touch in the function, lines 1643-1665). It contains `static_assert`s pinning PlayerColor bits (BLUE=1<<0 … PURPLE=1<<5) and Race bits.
- `map_format_info.cpp` (serialization) has **zero** references to `world` — `saveMap`/`loadMap` are pure MapFormat I/O.

## 10. Loading into the game world; what breaks without flags

`World::loadResurrectionMap` (world_loadmap.cpp:711-…):
1. `Map_Format::loadMap` → translations → `vec_tiles.resize(w*h)` → `Maps::readAllTiles(map)` (fails → abort) → `Maps::updateMapPlayers(map)` (fails → abort) → abort if `map.availablePlayerColors == 0`.
2. Per tile object: KINGDOM_TOWNS → color from `Maps::getTownColorIndex(map, tileId, object.id)`, race from `townObjects[index].metadata[0]` (random race resolved via kingdom), `Castle` created + `castle->loadFromResurrectionMap(castleInfo)`, `map_captureobj.Set(tileId, MP2::OBJ_CASTLE, color)`; debug-only asserts that `builtBuildings` contains BUILD_CASTLE/BUILD_TENT matching `metadata[1]` (819-822). KINGDOM_HEROES → color = `metadata[0]`, `GetHeroForHire(race)`, `SetCenter/SetColor/applyHeroMetadata` (845-892). MONSTERS → `tile.metadata()[0] = monsterMetadata.count`, random-monster `selected` list validated against unit level and one entry randomly baked into `tileData[1]` (894-940). Jail → hero with `PlayerColor::NONE`.
3. **Compound validation is essentially absent**: nothing verifies that a KINGDOM_TOWNS object has a basement or flags. Consequences of missing flags: `getTownColorIndex` finds neither flag → both colors stay 0 → returns 0 → `Color::IndexToColor(0)` = **BLUE**; a "neutral" castle silently becomes Blue and `updateMapPlayers` will register Blue as an available player with that town's race. One flag missing (or mismatched colors) → `assert(0)` in debug, returns 0 (Blue) in release. `updateMapPlayers` also `assert( tileIndex > 0 && tileIndex < map.tiles.size() - 1 )` for towns (map_format_helper.cpp:1403) — a castle entrance on the very first/last tile is a hard error. Missing basement merely loses graphics/passability; missing `castleMetadata` for the UID trips `assert` in debug builds (world_loadmap.cpp:796) and creates an empty entry in release (`map.castleMetadata[object.id]` via `operator[]`).
- `readAllTiles` debug builds additionally log **duplicate UIDs** across tiles for all groups except LANDSCAPE_TOWN_BASEMENTS and LANDSCAPE_FLAGS (which legitimately share the town UID) (map_format_helper.cpp:952-1016).
- Editor load path: `readMapInEditor` = `world.generateUninitializedMap(map.width)` + `readAllTiles` + `world.updatePassabilities()` + `updatePlayerRelatedObjects` (map_format_helper.cpp:920-933).

## 11. Dependency analysis per function (can an offline CLI call it?)

The global `world` singleton is woven in deeply — even coordinate helpers use it: `Maps::isValidAbsPoint/isValidAbsIndex/GetIndexFromAbsPoint/GetDirectionIndex/isValidDirection/getAroundIndexes(tile,dist)` all read `world.w()/world.h()` (maps.cpp:282-408). **No function in these paths needs AGG image assets** — `getObjectInfo/getObjectsByGroup` are lazily-built static C++ tables (`populateObjectData`, map_object_info.cpp:6024-6238). Requirements per function:

| Function | MapFormat only? | Needs live `world`? | Notes |
|---|---|---|---|
| `Map_Format::saveMap/loadMap` | yes | no | pure serialization (map_format_info.cpp) |
| `addObjectToMap` | yes + UID counter | no (but see precondition) | reads `getLastObjectUID()`; caller must have advanced the counter |
| `setObjectOnTile`/`placeObjectOnTile` | no | **yes** (writes world tiles; world sized) | no assets |
| `setTerrainOnTile`/`setTerrainWithTransition` | mostly | **yes** (`setTerrain` mirrors into `world.getTile().setTerrain`, map_format_helper.cpp:207; transition logic reads only MapFormat) | needs `world` sized = map |
| `setRoadOnTile`/`updateRoadOnTile`/`updateAllRoads` | no | **yes** (world tile add/remove/replace) | |
| `addStream`/`updateStreamsAround` | no | **yes** (setObjectOnTile) | |
| `getTownColorIndex`, `doesContainRoad`, `isJailObject`, `isCapturableObject`, `getBuildingsFromVector`, `set/isDefaultCastleDefenderArmy`, `load/saveCastleArmy`, `load/saveHeroArmy` | yes | no | pure MapFormat + object tables (+Army class) |
| `updateMapPlayers` | yes* | *only* to `CaptureObject(NONE)` when erasing stale capturable metadata (1648-1657) | safe offline if `world` is at least sized; no-op branch if metadata consistent |
| `updatePlayerRelatedObjects` | no | **yes** (`world.addCastle`, `GetHeroForHire`, `CaptureObject`) | game/editor runtime only; not needed to produce a valid file |
| `readMapInEditor`/`readAllTiles` | no | **yes** (populates world) | |
| RMG `canPlace*`/`canFit*`/`markObjectPlacement`/`findPathToNearestRoad`/`pickEvenlySpacedTiles`/`planObjectPlacement` | MapStateManager + tables | indirectly (coordinate helpers use `world.w()`) | logic itself never touches world tiles except via helpers |
| RMG `putObjectOnMap`, `placeCastle`, `placeMine`, `placeMonster`, `placeActionObject`, `placeSimpleObject`, `placeBorderObstacle`, `placeObjectSet`, `placeValidTreasures`, `placeDecorations`, `forceTempRoadOnTile` | no | **yes** (`world.getTile`) | |
| `generateMap` | no | **yes + EditorInterface + Settings** (`EditorInterface::Get().generateNewMap`, editor_interface.cpp:632-635; Settings PoL check) | plus `Rand`, translations for name strings |

Bottom line for a CLI tool: you must initialize the `world` singleton (`world.generateUninitializedMap(width)` — which itself calls `Settings::Get()` and `World::Defaults()` → `vec_kingdoms.Init(); vec_heroes.Init(); vec_castles.Init()` (world.cpp:284-297, 358-384)) if you reuse these functions verbatim; alternatively, replicate only the **MapFormat mutations** (append `TileObjectInfo`s with correct UIDs + metadata maps), since the .fh2m file itself stores nothing derived from `world` — passability, castle instances and render sorting are all rebuilt at load.

## 12. Recipes (as MapFormat content; tile index `i = y*width + x`)

Assume you maintain your own UID counter `next()` (1-based). Order within a tile's `objects` vector matters for equal-UID render order; the sequences below match the engine's own append order.

### Blue Knight castle on grass at (x,y)
Let `U = next()`.
1. Tile `i`: append `{id:U, group:LANDSCAPE_TOWN_BASEMENTS, index:0}` (grass basement).
2. Tile `i`: append `{id:U, group:KINGDOM_TOWNS, index:0}` (Knight castle; 1 would be Knight town).
3. Tile `i-1`: append `{id:U, group:LANDSCAPE_FLAGS, index:0}` (left blue flag = colorIdx0*2).
4. Tile `i+1`: append `{id:U, group:LANDSCAPE_FLAGS, index:1}` (right blue flag).
5. `castleMetadata[U] = {}` with `builtBuildings = [BUILD_CASTLE]` (BUILD_CASTLE per castle.h; town would be BUILD_TENT). Neutral towns additionally get `defenderMonsterType = {-1×5}, defenderMonsterCount = {0×5}` (`setDefaultCastleDefenderArmy`).
6. Constraints: entrance tile not water; whole 5×2 basement + town used tiles on land & on-map; `0 < i < w*h-1`; y must allow parts up to (0,−3)/(−5,−1) only for action-object bounds (non-shadow ground parts: −2..+2 x, −1..0 y ⊕ basement row +1). Owner = Blue purely because both flags have index 0/1 (metadata[0]=0).
7. Optional starting hero (what the RMG does): tile `i+width`: `{id:next(), group:KINGDOM_HEROES, index:0*7+6 = 6}` (blue random-race hero) + `heroMetadata[thatUID] = { race: Race::RAND }`.

### Neutral mine (e.g. gold mine on grass) at (x,y)
`U = next()`; tile `i`: `{id:U, group:ADVENTURE_MINES, index: getMineObjectInfoId(GOLD, GRASS) = 1*5+4 = 9}`. No metadata at all (neutral = absence of `capturableObjectsMetadata[U]`; owned mine = `capturableObjectsMetadata[U].ownerColor = PlayerColor::X`). Land-only; footprint x −3..+1, y −1..0; tile below entrance should stay passable.

### Monster guard for it
`U = next()`; tile `i+width` (one below the mine entrance — RMG convention, placeMine line 907): `{id:U, group:MONSTERS, index: monsterId − 1}` (e.g. RANDOM_MONSTER_LEVEL_2 → `Monster::RANDOM_MONSTER_LEVEL_2 - 1`), plus `monsterMetadata[U] = { count: 0 }` (0 = random size; optional `selected: [ids]` restricts what a random monster becomes).

### Resource pile (gold pile) at (x,y)
`U = next()`; tile `i`: `{id:U, group:ADVENTURE_TREASURES, index:6}` (0 wood,1 mercury,2 ore,3 sulfur,4 crystal,5 gems,6 gold); editor also writes `resourceMetadata[U] = { count: 0 }` (0 = default random amount). Random resource = index 8 (no metadata).

### Treasure chest at (x,y)
`U = next()`; tile `i`: `{id:U, group:ADVENTURE_TREASURES, index:9}`. No metadata. Must be on clear, non-water ground (removable action object rule, editor_interface.cpp:368-388).

### After all objects: finalize
Run the equivalent of `updateMapPlayers`: set `availablePlayerColors |= BLUE`, `playerRace[0] = Race::KNGT` (bit), ensure `humanPlayerColors`/`computerPlayerColors` include Blue, then `saveMap`. Nothing else (no passability, no sorting) is persisted.

## 13. Caveats / gotchas

- `addObjectToMap` asserts (debug) that metadata emplacement succeeded — reusing a UID for two metadata-bearing objects is a bug.
- Duplicate UIDs across *different* objects (other than town basement/flags) are only diagnosed in `WITH_DEBUG` builds (map_format_helper.cpp:952-1016) — a generator bug will load silently in release, with render-order anomalies.
- Roads/streams strip "embedded object" terrain images (`Ground::doesTerrainImageIndexContainEmbeddedObjects`) — do the same when putting roads on decorated terrain tiles.
- ROADS index 2 is used by the RMG as a temporary placeholder; final indices are direction bitmasks (0-255), +256 variants, 512 for the castle-entrance apron; `updateRoadOnTile` picks 512 when the tile **above** contains any `KINGDOM_TOWNS` object.
- The neutral color index is 6 (`neutralColorIndex = Color::GetIndex(PlayerColor::UNUSED)` → default branch → 6, map_random_generator_info.h:50 + color.cpp:102-123); `Color::IndexToColor(6) = PlayerColor::NONE`; neutral castle flags are indices 12/13 (gray).
- KINGDOM_TOWNS editor panel uses packed `color*14 + type`; MapFormat stores the **unpacked** type (0-13).
- `setMonsterOnTile` count 0 ⇒ `mons.GetRNDSize()` random army in world, but MapFormat truth is `monsterMetadata[uid].count` (0 = random) applied at game load (world_loadmap.cpp:901-902).
- `placeCastle` in the RMG places the hero **before** `markObjectPlacement` of castle/basement and forces roads below the entrance; the basement part at (0,1) is SHADOW_LAYER so the entrance stays reachable.
