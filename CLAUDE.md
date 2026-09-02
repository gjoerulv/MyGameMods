# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A workspace for authoring native fheroes2 maps (`.fh2m`) for Heroes of Might and Magic II. `mapgen/` is a headless C++ CLI that compiles the **entire fheroes2 engine except its `main()`** and drives the engine's real editor code paths (`world.generateUninitializedMap`, `setTerrainWithTransition`, `setObjectOnTile` + `addObjectToMap`, `updateMapPlayers`, the genuine `saveMap`) — so serialization, object tables, terrain transitions and validation are always the engine's own, never reimplemented.

## External dependency (required, not in this repo)

- fheroes2 clone at `C:\Users\gjoer\source\repos\fheroes2` — validated at commit `b086d1aa8b921163712aec2fb8188f4d0d375b09`. Treat the engine source as the format specification; when any document here disagrees with the source, the source wins. Do not modify the clone.
- The clone's prebuilt deps must exist (`VisualStudio/packages/`, created by `script/windows/install_packages.bat`).
- Installed game: fheroes2 release 1.1.17 with full HoMM2 assets; it reads/writes the same format v13 as the pinned commit.

## Commands

```bash
# Regenerate the project file — only needed after adding/removing mapgen/src/*.cpp (it globs them)
python mapgen/gen_vcxproj.py

# Build (VS 2026 MSBuild)
"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" mapgen/mapgen.vcxproj -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo

# Generate a map (deterministic; prints ASCII terrain/passability grids + validation)
mapgen/build/x64/Release/mapgen.exe generate <mapname> <out.fh2m> [seed]

# Validate through the game's own scenario-load path (players, win/loss wiring, world build)
mapgen/build/x64/Release/mapgen.exe gameload <file.fh2m>

# Dump header/metadata
mapgen/build/x64/Release/mapgen.exe inspect <file.fh2m>

# Print engine army strength of every guard/garrison, then compare to reference armies
mapgen/build/x64/Release/mapgen.exe strength <mapname> > strength.txt
python mapgen/guard_model.py strength.txt
```

There is no test suite; the `generate` + `gameload` output *is* the test. Install finished maps by copying to `%APPDATA%\fheroes2\maps\`.

## Architecture

- One source file per map: `mapgen/src/<name>_map.cpp` defines `void build<Name>(MapBuilder&)`, registered in `mapgen/src/map_registry.cpp`. `MapBuilder` (`mapgen.h`) wraps the fiddly parts: compound castle placement (basement + town + flags sharing one UID), per-object metadata creation, guard/mine/treasure placement, road/stream updates.
- Output is deterministic: the engine RNG is reseeded per run, so the same source + seed yields a byte-identical file. When refactoring the tool, prove safety by hash-comparing a regenerated known map (current `kings_ransom.fh2m`: sha256 `3c67381b9647e856f168315dd52337498d51f60f680e349d698ac21c2652cb0a`; `ashen_succession.fh2m`: sha256 `22c1d7c536c454036df3554de22aba52c77dc80ecf9e144dae7f829483f71d40`). King's Ransom deliberately stacks overlapping ridge mountains, so `MapBuilder::strictPlacement` stays off by default; new maps set it to `true` at the top of their build function.
- A map is not done until `generate` prints "all action objects reachable" and "round-trip byte comparison: IDENTICAL", its guard-sealed reachability list (what the first hero reaches without a fight) contains only the intended free objects, and `gameload` succeeds with the intended condition bits.
- Engine rules that shape every wall and terrain patch: a ground-object tile is enterable sideways/from below unless the tile below holds the same object or a same-sprite-family object (so vertical walls must be contiguous same-family mountain stacks; horizontal walls only need their centre row occupied), and any painted terrain tile without a same-terrain neighbour both horizontally and vertically is reverted (paint patches as overlapping >=2-row rectangles, never single tiles or rows).

## Where knowledge lives (don't re-derive, don't duplicate)

- `research_fh2m_and_homm2_design.md` — condensed engine ledger: format byte layout, object-table indices, placement recipes, economy numbers, AI behavior. Full tables with `file:line` citations in `research/notes/01..11_*.md`.
- `.claude/skills/homm2-map-maker/SKILL.md` — the workflow for creating a **new** map (interview → approved plan → implement → validate) plus the map-authoring gotchas. Use the skill for new maps instead of improvising.
- `kings_ransom_map.cpp` / `kings_ransom_design.md` / `kings_ransom_validation.md` — the worked example of a finished map and its documentation shape.
- `.localDocs/` — third-party community maps for reference only; never redistribute or copy them.
