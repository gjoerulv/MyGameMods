# The King's Ransom — Validation Report

Map: `kings_ransom.fh2m` — **SHA-256 `3c67381b9647e856f168315dd52337498d51f60f680e349d698ac21c2652cb0a`**, 4,770 bytes, FH2M format v13.
Engine revision used for all engine-level validation: fheroes2 upstream HEAD `b086d1aa8b921163712aec2fb8188f4d0d375b09` (2026-09-01).
Also installed to `%APPDATA%\fheroes2\maps\kings_ransom.fh2m` (byte-identical).

## 1. Build results

- **Full fheroes2 engine at HEAD**: built cleanly with MSVC (VS 2026 MSBuild, `v143` toolset, `Release-SDL2|x64`, project's own SHA256-pinned prebuilt deps) → `fheroes2/build/x64/Release-SDL2/fheroes2.exe`. Zero errors.
- **Generator `mapgen`** (`mapgen/mapgen.vcxproj`, produced by `mapgen/gen_vcxproj.py` from the engine's own `sources.props`): compiles all 248 engine sources except the game's `main()` plus 2 generator sources; links SDL2/SDL2_mixer/SDL2_image/zlib. Builds cleanly, runs headless (initializes the global `world`, never touches the display).
- The generator drives the **actual editor code paths**: `Maps::setTerrainWithTransition`, `Maps::setObjectOnTile` + `Maps::addObjectToMap`, `Maps::setRoadOnTile`, `Maps::addStream`, `Maps::updateMapPlayers`, and the genuine `Maps::Map_Format::saveMap`. No serialization, object-table, or transition logic was re-implemented.

## 2. Serializer / loader round-trip (engine code, automated)

Performed by `mapgen.exe generate` on every run:
- `saveMap` → file → `loadMap` (the real engine loader): **OK** (version 13, width 36, 1296 tiles).
- Loaded map re-saved and byte-compared to the original: **IDENTICAL**.
- Determinism: regeneration from source with the default seed (20260901) reproduces the exact same SHA-256; the delivered file, a fresh regeneration, and the installed copy are all byte-identical.

## 3. Structural validation (automated, engine passability)

- 188 placed objects; UIDs unique except the intended castle compounds (basement + town + 2 flags sharing one UID each).
- **Reachability BFS over the engine's own computed passability** (after `world.updatePassabilities()`), starting at Blue's hero, treating removable action objects (monsters, heroes, pickups) as traversable: **every action object on the map is reachable**. Zero unreachable passable tiles outside the one deliberately sealed (and decorated) hollow behind the King's Mine.
- Earlier iterations caught and fixed real defects this way: a porous ridge flank that bypassed the Pass A guard, a wagon camp sealed into a pocket, and a desert region erased by the engine's transition-recovery when painted in 1-tile strips.
- Full generation transcript with ASCII terrain/passability grids: `mapgen/generation_report.txt`.

## 4. Game-load validation (engine code, automated)

`mapgen.exe gameload` replicates the exact scenario-start sequence from `game_scenarioinfo.cpp` / `game_auto_playtest.cpp` (`FileInfo::readResurrectionMap` → `Settings::setCurrentMapInfo` → `Players::Init`/`SetStartGame` → `World::loadResurrectionMap`):

```
FileInfo: name 'The King's Ransom', size 36x36, difficulty 1
  human-only colors 1, comp+human 0, computer-only 2
  ConditionWins bits:  0x20 (expect 0x20 = WINS_GOLD only)
  ConditionLoss bits:  0x800 (expect 0x800 = LOSS_TIME)
  gold target: 100000   loss days: 56   comp also wins: 0
World::loadResurrectionMap: OK. World 36x36, castles 3
  kingdom BLUE : castles 1, heroes 1
  kingdom GREEN: castles 1, heroes 1
```

This proves: the map passes the scenario-list filter, Blue is human-only, Green AI-only, normal victory is disabled (`WINS_ALL` absent from the win bits), the AI cannot satisfy the gold condition, and the full world build (castles, heroes, metadata cross-checks in `world_loadmap.cpp`) succeeds.

## 5. Runtime testing (real game, HEAD build, real HoMM2 assets)

Performed by launching the freshly built `fheroes2.exe` with the machine's installed game assets and driving it via scripted input + screenshots (in `validation/`):

| Evidence | Verified |
|---|---|
| `01_scenario_list.png` | Map appears in Standard Game list as "The King's Ransom", 2 players, Small, **difficulty Normal**, description renders, win/loss icons shown |
| `02_ingame_day1_sign.png` | Game starts (Month 1 Week 1 Day 1); hero movement works; a wood pile was collected (+6 wood); the custom sign displays its authored text |
| `03_castle_screen_start.png` | Highmarch: authored buildings exact (Thatched Hut built, Archery Range built → upgrade offered), starting resources exactly 7500g/20w/20o/5 rares, Shipyard correctly disabled (landlocked), Knight build-dependency tree correct |
| `04_adventure_map_home.png` | Home region renders with no graphical corruption: castle + blue flags, roads, stream + water wheel, pond, themed decorations, coherent minimap |
| `05_editor_loaded.png` | The in-game **Editor** loads the map (terrain, objects, passability overlay all correct; whole-map minimap readable) |
| `fheroes2.log` | Sawmill capture works ("two units of wood per day"), Gazebo works, sign dialog works, clean quit — no crash, no assert |

The user's installed release 1.1.17 is format-compatible by construction (its `map_format_info.h/.cpp` are byte-identical to HEAD; both read/write v13), and the installed map copy is byte-identical to the deliverable.

## 6. Auto-playtest

The editor's Auto Playtest (AI plays all sides) was reached — the map was open in the editor and the playtest hotkey was issued — but the run was **aborted intentionally**: the desktop turned out to be in active use by the user (the game window was moved aside mid-automation), and the feature is editor-UI-only with no headless mode (verified in source: no CLI arguments exist). **No auto-playtest results are claimed.** To run it yourself: Editor → load `kings_ransom` → press **A** → disable Animation → OK; playthroughs reload the map fresh each time and report per-color win rates.

## 7. Economic model

`mapgen/economy_model.py` (frictionless day-by-day treasury simulation over verified engine income/cost constants): passive play tops out at 62.5k (loss); mediocre play wins ~day 44–48; good play ~day 41; excellent play ~day 38–40 — with real-play friction (travel, AI contest of the vale, combat RNG) the expected outcomes land on the commissioned targets (excellent 40–44, good 45–50, imperfect 51–56, mistakes → loss).

## 8. Known limitations / honest caveats

1. **No full human playtest**: the map has been played only a few in-game days interactively. Guard counts rest on the economic model plus creature-stat arithmetic, not on completed playthroughs. The single-block guard-tuning constants at the top of `kings_ransom_map.cpp` make rebalancing a one-line-per-guard exercise.
2. **No completed auto-playtest** (see §6).
3. The frictionless economy model deliberately ignores Green's interference; the ~3–5-day friction estimate is judgment, not measurement.
4. Cosmetic-only nondeterminism exists *inside the engine at load time* (e.g. random monster sprite jitter); the map file itself is byte-deterministic.
5. A ~8-tile decorated hollow behind the King's Mine is intentionally unreachable.
6. `availableToHireMonsterCount` in `CastleMetadata` is serialized but unused by the current engine (left at defaults).

## 9. Reproduction

```
python mapgen/gen_vcxproj.py
msbuild mapgen/mapgen.vcxproj /p:Configuration=Release /p:Platform=x64
mapgen\build\x64\Release\mapgen.exe generate kings_ransom kings_ransom.fh2m   # deterministic
mapgen\build\x64\Release\mapgen.exe gameload kings_ransom.fh2m                # engine-level game-load check
mapgen\build\x64\Release\mapgen.exe inspect  kings_ransom.fh2m                # header/metadata dump
```
(The tool now takes a map name from `mapgen/src/map_registry.cpp`; the old
`generate <out.fh2m>` form still works and builds `kings_ransom`. The registry
refactor was hash-verified to produce a byte-identical map.)
