# 08 — Build integration: linking real fheroes2 map serialization into a small offline CLI tool

Researched against local clone `C:/Users/gjoer/source/repos/fheroes2` at upstream HEAD `b086d1aa8b921163712aec2fb8188f4d0d375b09` (2026-09-01).
**Everything below was verified by actually compiling and linking with the machine's MinGW g++ 8.1.0 — a working `maptool.exe` round-tripped a generated 36x36 map AND loaded/resaved the bundled `maps/4_dimensions.fh2m` (v10, 144x144, 40 castles, 8 heroes).**

---

## 1. Project build structure

### CMake
- Root `CMakeLists.txt`:
  - `cmake_minimum_required(VERSION 3.24)` (line 21) — our CMake 3.25.1 qualifies.
  - `project(fheroes2 ... LANGUAGES C CXX)` (line 26), `set(CMAKE_CXX_STANDARD 17)` (line 28), `set(CMAKE_CXX_EXTENSIONS OFF)` (line 29).
  - `find_package(ZLIB REQUIRED)` (line 59) — **zlib is an external system/vcpkg package, not vendored**. SDL2 + SDL2_mixer also `find_package`d (lines 57-58). SDL2_image/PNG only when `ENABLE_IMAGE` (lines 63-64).
  - `option(ENABLE_TOOLS ...)` gates `src/tools` (src/CMakeLists.txt line 40).
- `src/CMakeLists.txt` adds subdirs: `thirdparty` (libsmacker only), `engine`, `fheroes2`, `tools` (lines 36-40).
- `src/engine/CMakeLists.txt`: globs all engine `*.cpp` into `add_library(engine STATIC ...)` (lines 21, 27); links `smacker`, SDL2, SDL2_mixer, Threads, `ZLIB::ZLIB` (lines 45-54). Engine's include dir is PUBLIC (lines 39-43).
- `src/fheroes2/CMakeLists.txt`: the game target's private include dirs are **all 22 subdirs**: `agg ai army audio battle campaign castle dialog editor game gui h2d heroes image kingdom maps monster resource spell system world` (lines 84-108). Headers are scattered (e.g. `color.h` in `kingdom/`, `game_language.h` in `game/`, `direction.h` in `heroes/`, `settings.h` in `system/`), so a tool touching map headers needs the full `-I` set.
- `src/tools/CMakeLists.txt`: each existing tool is `add_executable(<name> <name>.cpp)` + `target_link_libraries(<name> engine)` (lines 29-46). They link the whole engine static lib (which pulls SDL at link time), so **the upstream tools pattern is NOT SDL-free** — do not copy it blindly.

### Visual Studio
- Buildable via root `fheroes2-vs2019.vcxproj` (works in VS2022/VS2026; docs/DEVELOPMENT.md line 17).
- `VisualStudio/common.props`: `<LanguageStandard>stdcpp17</LanguageStandard>`, warnings-as-errors, `_CRT_SECURE_NO_WARNINGS`.
- `VisualStudio/fheroes2/common.props` line 5: same 22 include dirs + `src\thirdparty\libsmacker`.
- **zlib/SDL for VS**: `script/windows/install_packages.bat` downloads `windows.zip` from `https://github.com/fheroes2/fheroes2-prebuilt-deps/releases/download/windows-deps/windows.zip` (SHA256-pinned) and unpacks into `VisualStudio/packages/`. `VisualStudio/SDL2.props` then adds `packages\sdl2\include` (+`\SDL2`) to includes and links `SDL2main.lib;SDL2.lib;SDL2_mixer.lib;SDL2_image.lib;zlib.lib` from `packages\sdl2\lib\$(PlatformTarget)`. **On this machine `VisualStudio/packages/` does not exist** (script not yet run).
- VS tool projects (e.g. `src/tools/icn2img-vs2019.vcxproj` + `VisualStudio/tools/icn2img/sources.props`) use the **cherry-picked-.cpp pattern**: they compile a hand-listed subset of engine .cpp files (`agg_file.cpp image.cpp image_color_conversion.cpp image_palette.cpp image_tool.cpp logging.cpp serialize.cpp system.cpp zzlib.cpp`) directly into the exe instead of referencing a library. This is exactly the pattern to imitate.

### C++ standard / flags
C++17 everywhere: CMake root:28, VS common.props (`stdcpp17`), Android `-std=c++17` (android/app/jni/*/Android.mk:37/43). No exotic flags required; `-std=c++17` alone compiles all needed TUs with g++ 8.1.

---

## 2. Dependency analysis of the serialization core

### map_format_info.cpp (853 lines) — the save/load implementation
Direct includes (lines 21-36): `map_format_info.h`, std headers, `artifact.h`, `direction.h`, `mp2.h`, `rand.h`, `serialize.h`, `zzlib.h`.
Header `map_format_info.h` includes `color.h`, `game_language.h`, `map_object_info.h`, `resource.h` (lines 32-35).

**No SDL, no AGG, no image code is included.** Verified undefined-symbol list of the compiled object (`nm -C map_format_info.o`):
- serialize.cpp: `OStreamBase::operator<<`/`IStreamBase::operator>>` for all int types, `RWStreamBuf`, `StreamFile`, `StreamBase::setBigendian/setFail`
- zzlib.cpp: `Compression::zipData`, `Compression::unzipData`
- rand.cpp: `Rand::Get(uint32_t, uint32_t)` (used at line 406 for road image variant: `roadObjectIndex += Rand::Get( 1 ) * 256;`)
- map_object_info.cpp: `Maps::getObjectsByGroup(Maps::ObjectGroup)` (used in `convertFromV9ToV10`, lines 264-265 — load-path version conversion)
- Everything else (`Artifact::SPELL_SCROLL`, `Direction::*`, `MP2::OBJ_*`) is **enum-only, header-only, no link dep**.

Format facts (for context): `magicWord = { 'h','2','m','a','p','\0' }` (line 101), `minFileSize{ 512 }` (line 105 — **loadMap rejects any file < 512 bytes**, keep generated maps above this), `minimumSupportedVersion{ 2 }` (107), `currentSupportedVersion{ 13 }` (110). File streams use `setBigendian( true )`. Body after the plain-text BaseMapFormat header is zlib-compressed (`saveToStream` lines 485-508).

### map_object_info.cpp (6295 lines) — object tables: **SDL-free, self-contained**
Includes (lines 20-33): own header, std, `artifact.h`, `monster.h`, `resource.h`. Uses only enum constants from those (e.g. `Artifact::`, `Monster::`, `Resource::`) — verified no constructor/method calls.
`nm` of compiled object shows exactly ONE external symbol: `MP2::isOffGameActionObject(MP2::MapObjectType)` → requires **mp2.cpp**.
Tables are **lazily populated**: `populateObjectData()` guarded by `static bool isPopulated` (lines 6024-6027, 6213), called from `getObjectsByGroup` (line 6223) and `getObjectPartByIcn` (line 6242). No init call needed by the tool.
Caveat: its header `map_object_info.h` includes `maps_tiles.h` (line 27), which includes `heroes.h` — a *heavy header chain, but compile-only*; it compiles fine and adds no link deps.

### mp2.cpp — needed by map_object_info; small deps
`nm`: needs `Settings::Get()` + `Settings::isPriceOfLoyaltySupported() const` (used only in `MP2::getIcnIdFromObjectIcnType`, mp2.cpp lines 164-174) and `Translation::gettext/ngettext` (used only in `MP2::StringObject`). The map tool never calls either function, but the linker still demands the symbols (MinGW ld reports undefined refs even with `-ffunction-sections -Wl,--gc-sections` — **tested, gc-sections does NOT rescue undefined refs on PE/COFF**). Fix: link real `translations.cpp` + a 6-line `Settings` stub (below).

### map_format_helper.cpp (2085 lines) — **NOT linkable standalone**
Includes `army.h castle.h heroes.h players.h world.h ...` (lines 34-55). `nm` confirms undefined refs to: global `world`, `World::generateUninitializedMap/updatePassabilities/CaptureObject`, `Maps::Tile::*`, `Maps::setObjectOnTile`, `Maps::GetDirection*` (maps.cpp — itself world+AI-dependent), `Army`, `Castle` vtable, `HeroBase` vtable, `AllHeroes::GetHeroForHire`, `Race::IndexToRace`, `Color::IndexToColor`... i.e. the whole game core.

Function-level triage (line numbers in map_format_helper.cpp):
- **Pull `world` (heavy, unusable offline)**: internal `setTerrain` (line 191; writes `world.getTile( tileId ).setTerrain(...)` at line 207) — and therefore ALL public terrain APIs (`setTerrainOnTile` 1197, `setTerrainWithTransition` 1050); `updateStreamObjectOnMapTile` (812, world at 838/849) hence `addStream`/`updateStreamsAround`/`updateStreamsToDeltaConnection`; `placeNewRoadObjectOnTile` (884, world at 895) hence `setRoadOnTile` 1805 / `updateRoadOnTile` 1848 (world at 1840/1875) / `updateAllRoads`; `readMapInEditor`/`readAllTiles`/`readTileObject` (920-1048); `updatePlayerRelatedObjects` (1287, `world.addCastle` 1304, `GetHeroForHire` 1310); `updateMapPlayers` (1336, `world.CaptureObject` 1653); `captureObject` (1739, world at 1744).
- **Logically standalone (operate only on MapFormat + object tables)** but trapped in the same TU: `addObjectToMap` (1089 — no `world` in body; uses `getLastObjectUID()`, `Race::IndexToRace`, `getObjectsByGroup`, and auto-emplaces the right metadata entry per object group — this is the best reference for what metadata a generator must create per object); `getRiverDeltaDirectionByIndex` (1251); `isRiverDeltaObject` (1282); `doesContainRoad` (1899); `isJailObject` (1719); `isCapturableObject` (1724); `getBuildingsFromVector` (1749); `setDefaultCastleDefenderArmy`/`isDefaultCastleDefenderArmy` (1759/1769).
- Because linking any of it requires the world, **do not link map_format_helper.cpp**; replicate the needed logic (tile writes + `addObjectToMap`'s metadata-emplacement rules) in the tool, using `world_object_uid.cpp` for UIDs.

### ground.cpp (291 lines) — **linkable, tiny deps**
Includes (lines 24-31): `ground.h`, `maps_tiles.h`, `rand.h`, `skill.h`, `translations.h`. `nm`: only `Rand::Get` (rand.cpp) and `Translation::gettext` (translations.cpp; used by `Maps::Ground::String`, line 143+). Useful offline: `Ground::getRandomTerrainImageIndex( groundId, allowEmbedded )`, `getGroundByImageIndex`, `getTerrainStartImageIndex`, `doesTerrainImageIndexContainEmbeddedObjects` — everything needed to fill `TileInfo::terrainIndex` without map_format_helper.

### maps.cpp — avoid
Includes `ai_planner.h`, `game.h`, `world.h` (lines 33-50); uses global `world` throughout (e.g. lines 58, 68, 102). Do not link.

### Translation / gettext — **no libintl anywhere**
`_( str )` is `#define _( str ) Translation::gettext( str )` (src/engine/translations.h line 60). fheroes2 ships its **own** .mo parser in src/engine/translations.cpp. With no language loaded, `current` is null and gettext returns the (context-stripped) input string: `return current ? current->ngettext( str, 0 ) : stripContext( str );` (translations.cpp line 625). Grep confirms zero `libintl` references in the repo. So linking translations.cpp costs nothing (it additionally needs `StringLower`/`StringSplit` from engine/tools.cpp, and tools.cpp needs `crc32` from zlib).

### zzlib.cpp / serialize.cpp
- `zzlib.cpp` includes `<zconf.h> <zlib.h>` + `logging.h serialize.h` (lines 29-33); calls `compress2`, `compressBound`, `uncompress` → **link zlib**. It also defines `Compression::CreateImageFromZlib` referencing `fheroes2::Image` (zzlib.h includes `image.h`, line 30) → pulls **image.cpp** (+ `image_palette.cpp` for `fheroes2::getRGBGamePalette` and `image_color_conversion.cpp` for `getColorConversionTable`). All three are **SDL-free** (engine/image.h includes only std + `math_base.h`).
- `serialize.cpp` needs `Logging::GetTimeString/logFile/logMutex` (from logging.h macros).
- `logging.cpp` **fails to compile with g++ 8.1.0** — engine/system.h includes `<filesystem>` and MinGW-W64 8.1's libstdc++ has the known broken `fs_path.h` (`no match for operator!=` on `path`). Workaround: a 10-line stub (below) supplying `Logging::logFile` (`std::ofstream`, declared extern at logging.h line 75), `Logging::logMutex` (`std::mutex`, line 77), `Logging::GetTimeString()` (line 85). An unopened ofstream makes all log writes harmless no-ops. (On MSVC or newer GCC you could compile the real logging.cpp + system.cpp instead.)

### Funds stream operators — one unavoidable 2-line duplication
`operator<<(OStreamBase&, const Funds&)` / `operator>>` are defined in `src/fheroes2/resource/resource.cpp` **lines 649-658**:
```cpp
OStreamBase & operator<<( OStreamBase & stream, const Funds & res )
{
    return stream << res.wood << res.mercury << res.ore << res.sulfur << res.crystal << res.gems << res.gold;
}
IStreamBase & operator>>( IStreamBase & stream, Funds & res )
{
    return stream >> res.wood >> res.mercury >> res.ore >> res.sulfur >> res.crystal >> res.gems >> res.gold;
}
```
But resource.cpp also contains UI code referencing `Assets::getImage`, `fheroes2::Display::instance`, `fheroes2::Blit`, `fheroes2::Text` (+vtable) — i.e. AGG/screen/SDL. Cheapest fix: copy those two operators verbatim into the tool's stub TU instead of linking resource.cpp. (`Funds` itself is fine: default ctor is `= default` with `int32_t` field initializers, resource.h lines 64-77; `Funds::operator==` lives in resource.cpp but nothing in the save/load path calls it.)

---

## 3. Verified minimal file list (MinGW g++ 8.1.0, tested end-to-end)

fheroes2 sources compiled unmodified (`FH = C:/Users/gjoer/source/repos/fheroes2/src`):

| # | File | Why |
|---|------|-----|
| 1 | `$FH/engine/serialize.cpp` | stream classes, StreamFile, RWStreamBuf |
| 2 | `$FH/engine/zzlib.cpp` | Compression::zipData/unzipData |
| 3 | `$FH/engine/rand.cpp` | Rand::Get (road variant randomization) |
| 4 | `$FH/engine/translations.cpp` | Translation::gettext/ngettext (self-contained .mo reader; no-op untranslated) |
| 5 | `$FH/engine/tools.cpp` | StringLower/StringSplit for translations.cpp (needs zlib crc32) |
| 6 | `$FH/engine/image.cpp` | fheroes2::Image referenced by zzlib's CreateImageFromZlib |
| 7 | `$FH/engine/image_palette.cpp` | getRGBGamePalette for image.cpp |
| 8 | `$FH/engine/image_color_conversion.cpp` | getColorConversionTable for image.cpp |
| 9 | `$FH/fheroes2/maps/map_format_info.cpp` | saveMap/loadMap |
| 10 | `$FH/fheroes2/maps/map_object_info.cpp` | object tables (getObjectsByGroup; lazily populated) |
| 11 | `$FH/fheroes2/maps/mp2.cpp` | MP2::isOffGameActionObject for map_object_info |
| 12 | `$FH/fheroes2/maps/ground.cpp` | (optional but recommended) terrain image-index helpers |
| 13 | `$FH/fheroes2/world/world_object_uid.cpp` | (optional) getNewObjectUID/setLastObjectUID — zero deps |

Plus one project-local `stubs.cpp` (verified content, ~40 lines):
1. `Logging::logFile` (std::ofstream, never opened), `Logging::logMutex`, `Logging::GetTimeString()` returning `{}` — replaces logging.cpp (g++ 8.1 can't compile it).
2. `Settings & Settings::Get() { std::abort(); }` and `bool Settings::isPriceOfLoyaltySupported() const { return true; }` (include `settings.h`; settings.h compiles standalone) — satisfies mp2.o; never called by the tool.
3. The two Funds operators copied verbatim from resource.cpp:649-658 (include `resource.h`, `serialize.h`) — avoids resource.cpp's SDL/AGG chain.

External lib: **zlib only** (`-lz`). MinGW-from-CodeBlocks ships it: `C:/Program Files/CodeBlocks/MinGW/x86_64-w64-mingw32/include/zlib.h` and `.../lib/libz.a` — no extra include/lib paths needed.

**Not needed**: SDL2, SDL2_mixer, libsmacker, libpng, libintl, threads lib (winpthread is implicit in this posix-threads MinGW), logging.cpp, system.cpp, resource.cpp, maps.cpp, maps_tiles.cpp, map_format_helper.cpp, anything from agg//audio//gui/.

### Exact verified commands (this machine)
```sh
FH=C:/Users/gjoer/source/repos/fheroes2/src
INC="-I $FH/engine"
for d in agg ai army audio battle campaign castle dialog editor game gui h2d heroes image \
         kingdom maps monster resource spell system world; do INC="$INC -I $FH/fheroes2/$d"; done

g++ -std=c++17 -O2 $INC -c main.cpp stubs.cpp \
    $FH/engine/serialize.cpp $FH/engine/zzlib.cpp $FH/engine/rand.cpp \
    $FH/engine/translations.cpp $FH/engine/tools.cpp \
    $FH/engine/image.cpp $FH/engine/image_palette.cpp $FH/engine/image_color_conversion.cpp \
    $FH/fheroes2/maps/map_format_info.cpp $FH/fheroes2/maps/map_object_info.cpp \
    $FH/fheroes2/maps/mp2.cpp $FH/fheroes2/maps/ground.cpp \
    $FH/fheroes2/world/world_object_uid.cpp

g++ -o maptool.exe *.o -lz
```
(All 22 `-I` dirs are genuinely required: e.g. players.h → `ai/ai_personality.h`, color.h is in `kingdom/`, direction.h in `heroes/`.)
Result: `maptool.exe` ~900 KB; `saveMap`+`loadMap` round trip OK; loads bundled `maps/4_dimensions.fh2m` (reports v10, 144x144, correct name/castle/hero counts) and resaves it as v13.

### Test-run gotchas discovered
- A 36x36 map of identical tiles compressed to a **130-byte** file, which `loadMap` then **rejected** (`minFileSize` 512, map_format_info.cpp:105). The game would reject it too. Ensure generated maps are ≥ 512 bytes (any realistic varied-terrain map is).
- `map.tiles.size()` must equal `width*width` or load fails (map_format_info.cpp:540).
- All compiles were warning-clean enough at default; upstream's own warn flags not needed.

---

## 4. Toolchain on this machine & recommended path

| Tool | Version / location | Status |
|------|--------------------|--------|
| MinGW g++ | 8.1.0 x86_64-posix-seh, `C:\Program Files\CodeBlocks\MinGW\bin\g++.exe` (on PATH) | **Verified working** for the whole file list; ships zlib; only logging.cpp breaks (stubbed) |
| CMake | 3.25.1, `C:\Program Files\CMake\bin` | Meets root requirement (3.24) |
| mingw32-make | present in CodeBlocks MinGW bin | usable as CMake "MinGW Makefiles" generator |
| VS2022 Community | `C:\Program Files\Microsoft Visual Studio\2022\Community` | untested here; needs zlib (run `script/windows/install_packages.bat` to get `VisualStudio/packages`, or vcpkg `zlib`) |
| VS2026 (18) Community | `C:\Program Files\Microsoft Visual Studio\18\Community` | same note |

**Recommendation (least friction): option (1) — tiny standalone project, MinGW g++, direct compile of the 13 fheroes2 .cpp files + stubs, `-lz`.** It is fully verified above, needs zero downloads, zero changes inside the fheroes2 clone, and no CMake configure of the game (which would `find_package(SDL2 REQUIRED)` and fail).
- Option (2) (add a tool under `src/tools`) is worse: upstream tools link the entire `engine` target → SDL2/SDL2_mixer required, and CMake configure requires SDL even to generate.
- If MSVC is later preferred (e.g. to drop the logging stub): same file list, `/std:c++17 /EHsc`, all 22 include dirs, plus zlib headers+lib from `VisualStudio/packages/sdl2` (after install_packages.bat) or vcpkg; logging.cpp+system.cpp can then be compiled for real, but the Settings and Funds stubs are still required.

Optional CMakeLists for the standalone tool (untested but same compiler/flags as the verified direct build; note `target_link_libraries(... z)` rather than `find_package(ZLIB)`, which is unreliable on MinGW):
```cmake
cmake_minimum_required(VERSION 3.24)
project(fh2m_tool CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(FH ${CMAKE_SOURCE_DIR}/../fheroes2/src)
add_executable(maptool main.cpp stubs.cpp
  ${FH}/engine/serialize.cpp ${FH}/engine/zzlib.cpp ${FH}/engine/rand.cpp
  ${FH}/engine/translations.cpp ${FH}/engine/tools.cpp
  ${FH}/engine/image.cpp ${FH}/engine/image_palette.cpp ${FH}/engine/image_color_conversion.cpp
  ${FH}/fheroes2/maps/map_format_info.cpp ${FH}/fheroes2/maps/map_object_info.cpp
  ${FH}/fheroes2/maps/mp2.cpp ${FH}/fheroes2/maps/ground.cpp
  ${FH}/fheroes2/world/world_object_uid.cpp)
foreach(d agg ai army audio battle campaign castle dialog editor game gui h2d heroes image
          kingdom maps monster resource spell system world)
  target_include_directories(maptool PRIVATE ${FH}/fheroes2/${d})
endforeach()
target_include_directories(maptool PRIVATE ${FH}/engine)
target_link_libraries(maptool z)
```

## 5. Working artifacts (scratch)
Verified build lives in `C:/Users/gjoer/AppData/Local/Temp/claude/C--Users-gjoer-source-repos-MyGameMods/5d0123ac-5c99-4d7f-b772-f51768b91f47/scratchpad/buildtest/` — `main.cpp` (generate+roundtrip), `main2.cpp` (load real map + resave), `stubs.cpp` (the exact working stub file), `maptool.exe`, `maptool2.exe`.
