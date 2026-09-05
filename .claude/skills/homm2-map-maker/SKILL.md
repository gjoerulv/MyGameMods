---
name: homm2-map-maker
description: Interactively design and generate native fheroes2 Heroes of Might and Magic II maps (.fh2m) using this repo's mapgen toolchain. Use this skill whenever the user asks to make, create, design, generate, or "build" a HoMM2 / Heroes 2 / fheroes2 map or scenario, wants a new .fh2m file, wants to modify, retune, rebalance, or regenerate an existing generated map (e.g. kings_ransom), or asks what kind of map could be made — even if they don't say ".fh2m" or "generator". The skill starts with a structured interview and does not write any code until the user approves a written plan.
---

# HoMM2 Map Maker (fheroes2 .fh2m)

Design and generate playable, polished fheroes2 maps through: **interview → approved plan → implement → validate → deliver**. The heavy machinery already exists in this repo; your job is to gather a clear design from the user, then drive the toolchain.

## Toolchain map (read this first)

| Thing | Where | What it is |
|---|---|---|
| Generator tool | `mapgen/` | C++ CLI linking the FULL fheroes2 engine (minus its `main`); drives the real editor code paths headlessly |
| Engine clone | `C:\Users\gjoer\source\repos\fheroes2` | validated at commit `d778cb44b30e4fcf81ee70ccf96354b355c81c4f`; format v13, compatible with installed release 1.1.17 |
| Builder API | `mapgen/src/mapgen.h` | `MapBuilder`: terrain painting, castles/heroes/mines/monsters/treasure/signs/roads/streams, placement checks, reachability and fairness helpers |
| Worked examples | `mapgen/src/kings_ransom_map.cpp`, `mapgen/src/ashen_succession_map.cpp` | a 36×36 two-player economy race and a 72×72 mirrored four-player conquest map — copy their structure for new maps |
| Map registry | `mapgen/src/map_registry.cpp` | one entry per map: `{ name, title, width, defaultSeed, buildFn }` |
| Research ledger | `research_fh2m_and_homm2_design.md` | format spec, object-table indices, placement recipes, economy numbers, AI behavior, design principles |
| Deep references | `research/notes/01..11_*.md` | full tables and citations (see "References" at the bottom) |
| Example docs | `kings_ransom_design.md` / `_validation.md`, `ashen_succession_design.md` / `_validation.md` | the shape of a good design doc and validation report |
| Models | `mapgen/economy_model.py`, `mapgen/guard_model.py` | gold-race pacing model (adapt for gold/timed victories); guard and garrison calibration against reference armies built from `mapgen.exe strength` output |

Build command (VS 2026 MSBuild):
```
python mapgen/gen_vcxproj.py        # only needed when adding/removing source files
"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" mapgen/mapgen.vcxproj -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo
```
Tool usage:
```
mapgen/build/x64/Release/mapgen.exe generate <mapname> <out.fh2m> [seed]   # deterministic; prints ASCII grids, reachability, guard-sealed lists, fairness table
mapgen/build/x64/Release/mapgen.exe gameload <file.fh2m>                   # runs the game's own load path + condition checks
mapgen/build/x64/Release/mapgen.exe inspect  <file.fh2m>                   # header/metadata dump
mapgen/build/x64/Release/mapgen.exe strength <mapname> > strength.txt       # engine army strength of every guard and garrison
python mapgen/guard_model.py strength.txt                                   # compare guards with reference armies (calibration bands)
```
Install a finished map by copying it to `%APPDATA%\fheroes2\maps\`.

## Phase 1 — Interview (adaptive rounds, then a plan gate)

Interview the user with `AskUserQuestion` in **rounds of at most 4 questions**, each with concrete options plus room for free-text. Ask follow-up rounds only where earlier answers leave real ambiguity — the goal is a plan you could hand to another engineer, not a completed questionnaire. If the user's opening message already answers something, don't re-ask it; confirm it in the plan instead.

**Round 1 — Concept** (always ask what's not already known):
- **Size**: 36 (Small) / 72 (Medium) / 108 (Large) / 144 (XL). Advise against oversizing: content must fill the map, and a 72×72 needs ~4× the objects of a 36×36. Small suits tight timed/economic scenarios; Medium suits 3–4-player conquest.
- **Players**: how many (2–6), which are human-playable, which AI. (Engine limit: 6 players, 8 heroes per kingdom.)
- **Victory condition**: defeat everyone / capture a specific town / defeat a specific hero / find an artifact (or any Ultimate) / one side defeats the other (alliances) / accumulate N gold. And whether normal victory (defeat-everyone) should ALSO count, and whether the AI may win the special condition.
- **Loss condition**: standard (lose everything) / lose a specific town or hero / out of time (N days).

**Round 2 — World** (shaped by round 1):
- **Factions & colors**: race per player (Knight/Barbarian/Sorceress/Warlock/Wizard/Necromancer/Random), who gets which color. Mention balance folklore if relevant: Warlock/Necromancer scale hard on big rich maps; Knight/Barbarian/Sorceress favor small maps and early contact.
- **Terrain identity**: which terrains define the map's regions (grass/snow/desert/swamp/lava/wasteland/dirt/beach+water) and the rough geography (e.g. "snowy north vs desert south", "volcanic island ring"). Water/naval yes or no — naval maps need boats, shipyards and AI-safe coastlines, so default to landlocked unless asked.
- **Topology archetype**: lanes/ridges with guarded passes (like King's Ransom) / open field with distance as the only gate / ring around a central prize / mirrored halves for fairness (like The Ashen Succession). Shortest routes should be the most dangerous.
- **AI role**: economic rival racing the same objective / aggressor applying military pressure / turtle to be conquered. Contested zones or separated starts?

**Round 3 — Content & pacing**:
- **Economy density**: sparse (every mine matters) / standard (wood+ore free near each start, gold behind guards) / rich (multiple gold sources each). Every start needs unguarded wood+ore within ~1 turn or hard difficulties break.
- **Guard curve**: when should the player break out of the start (week 1 / 2 / 3), and expected completion timeline in days.
- **Special objects**: any wishes — sphinx riddles, dwellings, named artifacts (a victory artifact?), Stone Liths (teleporters), obelisk puzzle + Ultimate Artifact, Trading Post, XP objects.
- **Story & flavor**: setting, castle/hero names, sign texts, timed message events, rumors — authored by the user or invented by you to fit the theme. (Never use recurring resource-penalty events for difficulty; the community hates them — tune guards and access instead.)

**Follow-ups**: keep asking single tight rounds until nothing ambiguous remains that would change the layout, the economy, or the win/loss wiring. Typical stragglers: exact gold target and day limit for economic maps; whether a neutral town exists and its garrison strength; difficulty label; whether determinism/seed matters to them.

**The plan gate** — before writing any code, present a written plan and get explicit approval:
1. One-paragraph premise + exact victory/loss configuration (types, metadata values, AI-applicability, normal-victory flag).
2. Region sketch (ASCII or bullet geography) with castle positions, lanes, and pass/gate locations.
3. Object & guard table: every mine/dwelling/treasure with its guard type+count, themed to its region (guard species should fit the terrain — dwarves in snow, nomads in desert).
4. Economy targets when the map is economic or timed: passive-income ceiling, expected win day per skill tier, and the main tuning levers. Adapt `mapgen/economy_model.py` and show its output for gold-victory or out-of-time maps — passive play must lose, and one setback must not doom the run.
5. Flavor: names, signs, events, rumors.

Revise the plan until the user approves it. Then save it as `<mapname>_design.md` — it becomes the map's design doc.

## Phase 2 — Implement

1. Create `mapgen/src/<mapname>_map.cpp` defining `void build<Name>( MapBuilder & b )`. Model it on `kings_ransom_map.cpp` (single-player economy) or `ashen_succession_map.cpp` (mirrored multi-player): `b.strictPlacement = true;` first, a guard-tuning constants block at the top (single place to rebalance), then `terrain()`, `ridge()/walls()`, one function per region, `roadsAndStreams()`, `decorations()`, `configureScenario()`.
2. Declare the function and add a `MapDefinition` entry in `mapgen/src/map_registry.cpp`.
3. `python mapgen/gen_vcxproj.py` (it globs `src/*.cpp`), then build with MSBuild.

Key `MapBuilder` calls (see `mapgen/src/mapgen.h` for the full API): `init(width, seed)` · `strictPlacement` (set it true first) · `paintRect`/`paintBlob` · `place(x, y, group, index, label)` / `tryPlace` (returns 0 instead of aborting when the spot is occupied or reserved) / `canPlace` · `placeCastle(x, y, color, raceSlot, isCastle, name)` (handles the basement+town+flags compound and metadata) · `placeHero` · `placeMine(x, y, resource, label)` (auto-picks the terrain variant) · `placeMonster(x, y, monsterId, count, label)` · `placeResourcePile` / `placeChest` / `placeArtifact` / `placeSign` · `addRoad` / `addStreamTile` / `routeRoad(x0, y0, x1, y1)` (Dijkstra; refuses occupied, reserved and guard-protected tiles — author roads explicitly where symmetry matters) · `movementCost(x0, y0, x1, y1)` (engine movement rules, for fairness tables) · `computeReachability(x, y, reachable, monstersBlock)` + `checkActionReachability` (plain BFS, or the guard-sealed test with `monstersBlock = true`) · `printStrengthReport()` · `finalize()`. Object-group indices for anything not wrapped: `research/notes/02_tiles_objectgroups_tables.md`.

### Hard-won gotchas (each of these cost real debugging time)

- **Terrain**: paint bulk rectangles FIRST, ragged fringes after. The engine's transition fix-up reverts any painted tile that lacks a same-terrain neighbour both horizontally and vertically, so a single tile or a 1-tile-wide strip silently repaints back — an entire desert once vanished this way. Paint patches as overlapping rectangles at least 2 rows high; random-walk edges (see the snow/desert code in `kings_ransom_map.cpp`) beat symmetric blobs for organic borders.
- **Castles**: entrance tile is (x,y), entered from below; keep x in [2, W−3], y ≥ 3, and the tile below the entrance clear. A hero placed on that below-tile starts inside the castle. Set `customBuildings` + an explicit `builtBuildings` list for deterministic starts (otherwise the 2nd dwelling is a 50% coin flip). All entrances face south, so a mirrored layout is never tile-exact: shift the southern castles north, author roads per quadrant, and prove fairness with the movement-cost table rather than by symmetry.
- **Guards**: a monster protects only the neighbours its passability bits connect to — a guard diagonally below a chest does not protect it. Put pass guards ON the corridor/road tile and object guards orthogonally adjacent, or the pass leaks. `count > 0` is deterministic and always hostile; `count == 0` rolls a random stack. Use authored counts for strategically critical fights.
- **Walls leak sideways**: a ground-object tile is enterable sideways/from below unless the tile below holds the same object or a same-sprite-family object. Vertical walls must be contiguous same-family mountain stacks (mixed-family 1-tile fillers leak); horizontal walls only need their centre row occupied. After any wall change, run the guard-sealed test (`computeReachability(..., monstersBlock = true)` from every starting hero) and assert that the free objects are exactly the intended home inventory — the plain BFS shows a bypassable guard as fully reachable.
- **Mines**: the tile below the mine entrance must stay open; `placeMine` picks the sprite for the actual ground under it, so paint terrain before placing.
- **Random artifacts** need an explicit `selected` id list (ids 9–81 per level); without one the headless tool, which has no Price of Loyalty assets, can roll a PoL sprite and assert in `maps_tiles_helper.cpp`.
- **Neutral towns grow**: gray towns/castles gain base creatures weekly from week 2 (`Castle::_joinRNDArmy`), so a garrison meant to fall in month 2 must be tuned for its week-8 size — `guard_model.py` models this.
- **AI viability**: give every AI a placed starting hero (FH2M never auto-spawns one), free wood+ore, and guards on its expansion path breakable by a week-2–3 AI army — the AI pathfinder treats over-strong guards as walls and will stall. At Normal the AI has no economic or vision cheats.
- **Big mountains** are 5×3 footprints; footprint details for everything are in notes/02. Multi-tile sprites have no vertically flipped variants, so mirrored footprints differ by a tile between north and south halves.
- **Overlaps**: with `strictPlacement` on, any footprint overlap aborts the build (King's Ransom predates the flag and deliberately stacks ridge mountains, so it stays off there). Use `tryPlace` for decorations and `place` for anything that must exist.
- Files under ~512 bytes are rejected by the loader — never an issue for a real map.

## Phase 3 — Validate (all of this is cheap; do all of it)

1. `mapgen.exe generate <mapname> <out>.fh2m` — read the ASCII terrain and passability grids; iterate coordinates until the topology is right, "reachability: all action objects reachable" prints, the guard-sealed list of every starting hero contains only the intended free objects, and "round-trip byte comparison: IDENTICAL" holds.
2. `mapgen.exe gameload <out>.fh2m` — the game's own load path; verify the printed win/loss bits, gold/day values, per-kingdom castle+hero counts match the plan.
3. `mapgen.exe inspect <out>.fh2m` — final metadata dump; record the SHA-256 (`certutil -hashfile <file> SHA256` or Python).
4. Re-run `generate` once more to confirm determinism (same hash).
5. For economic/timed maps, re-run the adapted economy model and sanity-check the tuned guard values against the target win-day curve. For multi-player maps, print the per-player movement-cost table to every objective and keep the spreads small; run `mapgen.exe strength <mapname> > strength.txt` then `python mapgen/guard_model.py strength.txt` and keep every guard inside its band for the weakest race.
6. Copy the map to `%APPDATA%\fheroes2\maps\`, then tell the user how to playtest: in fheroes2, New Game → Standard Game (the map appears by title); for an AI-vs-AI balance check, open it in the Editor and press **A** (Auto Playtest, disable Animation for speed). Don't drive the game UI yourself unless the user asks — it's their desktop.
7. Write `<mapname>_validation.md` recording what was verified and the hash (mirror `ashen_succession_validation.md`).

Deliver: the `.fh2m` (installed + in repo root), the design doc, the validation note, and a short summary of the strategies the map supports. Offer one round of post-playtest retuning — guard constants are one block at the top of the map's `.cpp`.

## Quality bar (from the research; don't ship without these)

- Passive play must not achieve the objective; the shortest route should be the most dangerous; at least two genuinely viable routes/strategies.
- No large empty areas and no object soup — density gradients, varied sprites (never the same tree twice in a row), environmentally logical placement, one memorable landmark per region, a readable minimap.
- Starting area interesting inside 15 seconds: free wood/ore, a couple of pickups, one hook.
- Guard stepladder scaled to prize value; no guards on self-defending objects (graveyards, dwellings that fight).
- Description text states the objective, the deadline, and any non-obvious rule (e.g. "defeating the enemy is not enough").

## References (load on demand)

- `research_fh2m_and_homm2_design.md` — the condensed ledger; start here for any engine question.
- `research/notes/02_tiles_objectgroups_tables.md` — every object group/index/footprint.
- `research/notes/03_terrain_transitions_roads.md` — terrain image ranges, transition algorithm, roads/streams, movement costs.
- `research/notes/04_metadata_players_victory.md` — metadata layouts, win/loss wiring, load-time validation, neutral-town garrisons (`_postLoad`).
- `research/notes/05_economy_mechanics.md` — incomes, building/creature costs, marketplace rates, pickup values, growth.
- `research/notes/06_rmg_and_placement.md` — placement recipes, UID semantics, what load validates.
- `research/notes/07_ai_and_autoplaytest.md` — AI thresholds and the auto-playtest.
- `research/notes/10_design_aesthetics.md` + `11_community_wisdom.md` — the design-principles source material.
