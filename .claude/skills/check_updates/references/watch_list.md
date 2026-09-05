# Watch list: which upstream paths feed which docs and tools

`scripts/upstream_report.py` parses the table (first four columns). A changed file takes the
row with the longest matching path prefix; `src/fheroes2/maps/map_format_info` covers `.h` and
`.cpp`. Buckets `format`, `objects`, `terrain`, `editor`, `scenario`, `economy`, `ai` carry
research (Phase B). `build` means re-run the toolchain steps. `irrelevant` compiles into mapgen
but can change neither a generated map nor a documented fact. A `src/` path matching no row is
UNCATEGORIZED: classify it and add a row before continuing; other unmatched paths are `other`.

| Prefix | Bucket | Re-check in | Tools affected |
|---|---|---|---|
| src/fheroes2/maps/map_format_info | format | notes 01, 04; ledger §2, §6 | format gate; `inspect`, `gameload` |
| src/engine/serialize | format | note 01; ledger §2 | — |
| src/engine/zzlib | format | note 01; ledger §2 | — |
| src/fheroes2/maps/map_object_info | objects | note 02; ledger §3 | object indices in `mapgen/src/*_map.cpp` comments; `placeMine`, `placeCastle` |
| src/fheroes2/maps/mp2 | objects | note 02 | — |
| src/fheroes2/maps/maps_objects | objects | note 02 | — |
| src/fheroes2/gui/ui_map_object | objects | notes 02, 06 | `getTownBasementId`, `getMineObjectInfoId` |
| src/fheroes2/resource/artifact | objects | notes 02, 04 | random-artifact `selected` lists in map .cpp |
| src/fheroes2/kingdom/color | objects | notes 02, 04 | flag indices |
| src/fheroes2/kingdom/race | objects | notes 02, 04 | race slots |
| src/fheroes2/monster/monster.cpp | objects | notes 02, 05 | monster ids in map .cpp, `guard_model.py` |
| src/fheroes2/maps/map_format_helper | terrain | notes 02, 03, 04, 06; ledger §4, §5 | every `Maps::` helper mapgen calls |
| src/fheroes2/maps/ground | terrain | note 03; ledger §4 | `movementCost` |
| src/fheroes2/maps/maps_tiles_render | irrelevant | — | — |
| src/fheroes2/maps/maps_tiles_helper | economy | notes 04, 05; ledger §7 | `count>0` guard rule, pickup rolls |
| src/fheroes2/maps/maps_tiles | terrain | notes 02, 03, 04 | passability, sealed test |
| src/fheroes2/maps/maps.cpp | terrain | notes 03, 07 | `getMonstersProtectingTile` (sealed test) |
| src/fheroes2/maps/position | terrain | note 03 | — |
| src/fheroes2/maps/visit | irrelevant | — | — |
| src/fheroes2/heroes/direction | terrain | note 03 | road masks |
| src/fheroes2/heroes/route | terrain | note 03 | — |
| src/fheroes2/world/world_pathfinding | terrain | notes 03, 07 | `movementCost`, AI wall rule |
| src/fheroes2/editor/ | editor | notes 02, 04, 06; ledger §5 | editor recipes mirrored by `MapBuilder` |
| src/fheroes2/maps/map_random_generator | editor | note 06 | — |
| src/fheroes2/world/world_object_uid | editor | note 06 | UID handling in `placeCastle` |
| src/fheroes2/maps/maps_fileinfo | scenario | note 04; ledger §6 | `gameload` |
| src/fheroes2/world/world.cpp | scenario | notes 04, 05; ledger §6 | `gameload` |
| src/fheroes2/world/world_loadmap | scenario | notes 04, 06 | `gameload` |
| src/fheroes2/world/world_regions | scenario | — | — |
| src/fheroes2/game/game_over | scenario | note 04 | `gameload` condition bits |
| src/fheroes2/game/game_startgame | scenario | notes 04, 07 | — |
| src/fheroes2/game/game_scenarioinfo | scenario | note 04 | `gameload` mirror comment in `mapgen_main.cpp` |
| src/fheroes2/kingdom/players | scenario | note 04 | `gameload` |
| src/fheroes2/kingdom/kingdom.cpp | scenario | notes 04, 05, 07; ledger §7, §8 | starting hero / resources |
| src/fheroes2/castle/castle.cpp | scenario | notes 04, 05 | neutral-town growth in `guard_model.py` |
| src/fheroes2/heroes/heroes.cpp | scenario | notes 04, 07 | — |
| src/fheroes2/heroes/heroes_action | economy | note 05 | pickup rolls |
| src/fheroes2/kingdom/profit | economy | note 05; ledger §7 | `economy_model.py` |
| src/fheroes2/kingdom/payment | economy | note 05 | `economy_model.py` |
| src/fheroes2/kingdom/resource_trading | economy | note 05; ledger §7 | — |
| src/fheroes2/kingdom/week | economy | note 05 | `guard_model.py` growth |
| src/fheroes2/castle/buildinginfo | economy | note 05; ledger §7 | `economy_model.py` |
| src/fheroes2/castle/castle_building | economy | note 05 | — |
| src/fheroes2/castle/mageguild | economy | note 05 | — |
| src/fheroes2/monster/monster_info | economy | note 05; ledger §7 | `guard_model.py`, `strength` mode |
| src/fheroes2/army/ | economy | notes 05, 07 | `Troop::GetStrength` (`strength` mode, `guard_model.py`) |
| src/fheroes2/heroes/skill | economy | notes 03, 05 | `movementCost` |
| src/fheroes2/game/game_static | economy | note 05 | — |
| src/fheroes2/ai/ | ai | note 07; ledger §8 | — |
| src/fheroes2/game/difficulty | ai | notes 05, 07; ledger §8 | — |
| src/fheroes2/game/game_auto_playtest | ai | notes 07, 11 §4 | `gameload` mirror comment in `mapgen_main.cpp` |
| src/engine/rand | build | note 06 (determinism), ledger §1 | reseeding in mapgen; hashes |
| src/engine/ | build | note 08 | — |
| src/thirdparty/ | build | — | — |
| VisualStudio/ | build | note 08 | `gen_vcxproj.py` (sources.props, *.props), prebuilt deps |
| script/windows/ | build | note 08 | `install_packages.bat` (deps archive) |
| CMakeLists.txt | build | note 08 | — |
| src/CMakeLists.txt | build | note 08 | — |
| fheroes2-vs2019.vcxproj | build | note 08 | — |
| src/fheroes2/agg/ | irrelevant | — | — |
| src/fheroes2/audio/ | irrelevant | — | — |
| src/fheroes2/battle/ | irrelevant | — | — |
| src/fheroes2/campaign/ | irrelevant | — | — |
| src/fheroes2/dialog/ | irrelevant | — | — |
| src/fheroes2/gui/ | irrelevant | — | — |
| src/fheroes2/h2d/ | irrelevant | — | — |
| src/fheroes2/image/ | irrelevant | — | — |
| src/fheroes2/spell/ | irrelevant | — | — |
| src/fheroes2/system/ | irrelevant | — | — |
| src/fheroes2/game/ | irrelevant | — | — |
| src/fheroes2/heroes/ | irrelevant | — | — |
| src/fheroes2/castle/ | irrelevant | — | — |
| src/fheroes2/kingdom/ | irrelevant | — | — |
| src/fheroes2/monster/ | irrelevant | — | — |
| src/fheroes2/resource/ | irrelevant | — | — |
| src/fheroes2/world/ | irrelevant | — | — |
| src/fheroes2/CMakeLists.txt | build | — | — |
| src/dist/ | irrelevant | — | — |
| src/resources/ | irrelevant | — | — |
| src/tools/ | irrelevant | — | — |

## Engine logic mirrored inside mapgen

`grep -n "fheroes2 @" mapgen/src/*.cpp` lists comments tying mapgen code to an engine revision:
the `gameload` mode mirrors `game_scenarioinfo.cpp:LoadNewMap` / `game_auto_playtest.cpp:prepareMap`,
and the object-index blocks in `*_map.cpp` cite `map_object_info.cpp`. When the cited engine file
changed, re-read both sides and refresh the comment stamp (comments are not serialized; string literals are).
