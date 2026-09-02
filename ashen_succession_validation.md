# The Ashen Succession — Validation Report

Map: `ashen_succession.fh2m` — **SHA-256 `22c1d7c536c454036df3554de22aba52c77dc80ecf9e144dae7f829483f71d40`**, 14,226 bytes, FH2M format v13.
Engine revision used for all engine-level validation: fheroes2 upstream HEAD `b086d1aa8b921163712aec2fb8188f4d0d375b09`.
Also installed to `%APPDATA%\fheroes2\maps\ashen_succession.fh2m` (byte-identical).

## 1. Build results

- Generator `mapgen` rebuilt from `mapgen/gen_vcxproj.py` (248 engine sources + 4 generator sources) with VS 2026 MSBuild, Release x64. Zero errors, zero warnings.
- The map is produced by the real editor code paths (`setTerrainWithTransition`, `setObjectOnTile` + `addObjectToMap`, `setRoadOnTile`, `addStream`, `updateMapPlayers`, `saveMap`); nothing about the format is re-implemented.
- Builder additions made for this map (all in `mapgen/src/mapgen_main.cpp` / `mapgen.h`): strict overlap detection (`strictPlacement`, opt-in), a ground occupancy grid plus a cosmetic reservation mask (top-level sprite parts and the editor's town rectangle) honoured by `tryPlace` and the road router, a Dijkstra road router, a guard-sealed reachability test using the engine's own protection rule, an engine-rule movement-cost function for fairness tables, and a `strength` CLI mode. **Regression check:** with the new builder, `generate kings_ransom` still yields the known SHA-256 `3c67381b9647e856f168315dd52337498d51f60f680e349d698ac21c2652cb0a`.

## 2. Serializer / loader round-trip (engine code, automated)

Performed by `mapgen.exe generate` on every run:
- `saveMap` → file → `loadMap`: **OK** (version 13, width 72, 5184 tiles).
- Loaded map re-saved and byte-compared to the original: **IDENTICAL**.
- Determinism: two consecutive regenerations with the default seed (20260901) produce the same SHA-256; the repo copy and the installed copy are byte-identical.

## 3. Structural validation (automated, engine passability)

- 604 placed objects (1032 tile object records incl. roads/streams); 9 castles (4 player castles, 4 neutral toll towns, Kingsfall), 4 heroes, 58 monster stacks, 40 resource piles, 12 signs, 4 daily events, 6 rumors.
- **Reachability BFS over the engine's own passability**, monsters/pickups traversable, from the NW hero: **every action object is reachable**.
- **Guard-sealed reachability, per hero, engine rule** (a tile counts as protected exactly when `Maps::getMonstersProtectingTile(tile, false)` is non-empty, the check `Heroes::ActionNewPosition` performs on arrival): each of the four heroes reaches exactly **17** action objects without a fight — its own castle, hero, sawmill, ore mine, peasant hut, halfling hole, gazebo, fountain, windmill, water wheel, magic garden, one chest, three piles, the random resource and the sign — and nothing outside its home. The build aborts otherwise. This is the proof that all eight home passes, the four center gates and the four wall crossings are sealed and that no route bypasses a guard.
- Defects caught and fixed by these checks during development: a band row above each vertical-wall gap that was walkable sideways (corridor made 2 rows, guard moved to its top row); 1-tile fillers of mixed sprite families that made vertical walls sideways-permeable (walls rebuilt as contiguous same-family mountain stacks); a chest guard placed diagonally (moved directly below); a southern random-resource pile inside the mirrored gems guard's 3×3 (moved); a trading-post guard placed above its post in the south (guards removed); roads routed through rocks and through mine guards' 3×3 (roads authored explicitly).
- Terrain: the engine's transition fix-up reverts any painted tile that lacks a same-terrain neighbour both horizontally and vertically; all terrain patches are painted as overlapping 2-row rectangles. The final ASCII terrain grid shows the four wasteland ridges, the center scar, the four Greyfen fens, the four grass meadows and the two dirt islands under the Hill Forts.
- Full generation transcript (terrain grid, passability grid, placed-object list, sealed tests, fairness table): `mapgen/ashen_succession_generation_report.txt`.

## 4. Game-load validation (engine code, automated)

`mapgen.exe gameload` replicates the scenario-start sequence (`FileInfo::readResurrectionMap` → `Settings::setCurrentMapInfo` → `Players::Init`/`SetStartGame` → `World::loadResurrectionMap`):

```
FileInfo: name 'The Ashen Succession', size 72x72, difficulty 1
  human-only colors 0, comp+human 15, computer-only 0
  ConditionWins bits:  0x1  (WINS_ALL)
  ConditionLoss bits:  0x100 (LOSS_ALL)
World::loadResurrectionMap: OK. World 72x72, castles 9
  kingdom BLUE  : castles 1, heroes 1
  kingdom GREEN : castles 1, heroes 1
  kingdom RED   : castles 1, heroes 1
  kingdom YELLOW: castles 1, heroes 1
```

This proves: the map passes the scenario-list filter, all four colours are selectable as human or computer, victory is the standard defeat-everyone, and the full world build (random-race castles and heroes resolved, neutral garrisons, artifact choice lists, metadata cross-checks in `world_loadmap.cpp`) succeeds without assertion. An earlier build asserted in `maps_tiles_helper.cpp` because random artifacts had no `selected` list and the headless tool (no Price of Loyalty assets) rolled a PoL sprite; the lists are now explicit (ids 9–81 per level).

## 5. Fairness (engine movement costs, measured)

The build prints the cheapest movement cost from each castle to every objective under the engine's rules (source-tile ground penalty, 75 on road-to-road steps, diagonals ×1.5, fights ignored). Worst spread 238 movement points (the N/S seam artifact); every objective is within a quarter of a day across the four players. The full table is in `ashen_succession_design.md` section 2 and in the generation report. The residual spreads come from south-facing entrances (the southern hero walks round its castle to pass B, +161) and from the toll towns and Kingsfall standing on column 35, half a tile off the mirror axis (+75 for the eastern players).

## 6. Guard calibration (engine strength, modelled)

`mapgen.exe strength ashen_succession` prints every stack and garrison with the engine's `Troop::GetStrength`; `mapgen/guard_model.py` compares them (with neutral weekly growth for wandering stacks and the engine's neutral-town joins for garrisons) to reference armies built from the engine's dwelling growth numbers, flagging on the worst race. Every guard is inside its band except the alchemist lab, whose Warlock ratio (0.45) sits on the band floor. Output: `mapgen/ashen_succession_guard_model.txt`.

## 7. Adversarial review

An eight-lens review (engine semantics, topology, fairness, AI viability, aesthetics, text, builder code, plan completeness) with two independent refuters per finding was run on the generated map, source and docs: 33 raw findings, 14 after triage, 13 confirmed, 1 rejected. Resolutions:

| # | Finding | Resolution |
|---|---|---|
| 0 | Router laid roads over mine/treasure guards and through their 3×3 | All roads are now authored tile lists mirrored per quadrant; the router (kept as a utility) refuses monsters, protected tiles and reserved tiles |
| 1 | Roads not mirror-symmetric; centre decor blocked passability asymmetrically | Authored mirrored roads; centre decor placed in x-mirrored pairs; per-player cost table added to the build |
| 2 | Southern trading-post guard above its post, bypassable | Trading-post guards removed (they also skewed the southern cache walks) |
| 3 | Sealed test stricter than the engine, one hero only, never failed the build | Uses the engine's arrival rule, runs from all four heroes, aborts the build on a leak or a count ≠ 17 |
| 4 | Southern players farther from pass B | Southern castles five rows further north, sawmill and gold mine re-sited; spreads measured |
| 5 | Neutral toll towns grow weekly, docs said they do not | Growth modelled in `guard_model.py` (engine join rules), garrisons retuned, docs corrected |
| 6 | Kingsfall out of AI reach until month 4 | Garrison reduced to 40/30/18/12/5 (human month 2 late, AI month 3), documented |
| 7 | Calibration model dropped W/E guards and hid over-band cases | De-dup keyed on label+creature+count, strict worst-race flag, W seam gold 40→30 Mummies, home stepladder ordered (gold 28 Rogues > gems 6 Wolves), pass A 14 Wolves |
| 8 | Decor and roads under towers/crowns | Cosmetic reservation mask honoured by `tryPlace` and the router; roads re-authored clear of tower tiles |
| 9 | Sprites on the wrong terrain family; mines straddling patches | Sprites swapped to their ground's family, gems mine to (2,3), crystal mine into the fen, fen resized, massif mountains removed, centre decor moved |
| 10 | Roads pull basins inside the AI's castle-threat radius | Rejected by both refuters (true of any road-linked map, not a defect); noted in the design doc |
| 11 | Bare ridge bands | Mirrored ridge dressing (rocks, dead trees, skulls, cracks) at ~16% density, clear of corridors and roads |
| 12 | Signs named the wrong fen town; zone signs in one half only; description omitted capturable towns | Quadrant-aware home signs, one sign per zone half, description extended |
| 13 | Doc claims not matching the map (one-turn ore, 20×20 centre, equal gate walks, stream rock) | Sawmill re-sited for day-1 wood (ore is day 2), docs corrected (18×18, measured walks), rock moved |

## 8. Auto-playtest and human playtest

Not performed in this session: the editor's Auto Playtest is UI-only (no headless mode). To run it: fheroes2 Editor → load `ashen_succession` → press **A** → disable Animation → OK. Guard counts rest on the calibration model and creature-stat arithmetic, not on completed playthroughs. All guard and garrison constants sit in one block at the top of `mapgen/src/ashen_succession_map.cpp`.

## 9. Known limitations / honest caveats

1. Multi-tile sprites have no vertically flipped variants, so mirrored footprints differ by one tile between the north and south halves; fairness is by construction of the walls (verified by the sealed test in every quadrant), by identical object inventories, and by the measured cost table, not by tile-exact mirroring. Some decorations are skipped in one quadrant when a mirrored footprint or a tower reservation is in the way (cosmetic only; listed in the generation report).
2. Town and castle entrances all face south; the residual cost spreads above are the price of that.
3. Reference armies in the calibration model assume everything grown is recruited and ignore hero stats and spells; real human armies are stronger than the printed numbers, the AI needs 1.5× a guard's strength to attack.
4. Random artifacts, resource pile amounts and neutral-town joins resolve per game at load; the file itself is byte-deterministic.

## 10. Reproduction

```
python mapgen/gen_vcxproj.py
msbuild mapgen/mapgen.vcxproj /p:Configuration=Release /p:Platform=x64
mapgen\build\x64\Release\mapgen.exe generate ashen_succession ashen_succession.fh2m   # deterministic; prints sealed checks + fairness table
mapgen\build\x64\Release\mapgen.exe gameload ashen_succession.fh2m                    # engine-level game-load check
mapgen\build\x64\Release\mapgen.exe inspect  ashen_succession.fh2m                    # header/metadata dump
mapgen\build\x64\Release\mapgen.exe strength ashen_succession > strength.txt          # guard/garrison strengths
python mapgen\guard_model.py strength.txt                                             # calibration bands
```
