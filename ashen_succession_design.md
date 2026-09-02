# The Ashen Succession — Scenario Design Document

Map file: `ashen_succession.fh2m` (FH2M v13, generated against fheroes2 @ `b086d1aa8b921163712aec2fb8188f4d0d375b09`)
Generator: `mapgen/src/ashen_succession_map.cpp` (deterministic, seed 20260901)

---

## 1. Premise and objective

> The High King of Vaelmark died at harvest with no heir. Four Margraves hold the four marches,
> and each means to be crowned. Between them lie the toll towns of the old roads and the dead
> capital, **Kingsfall**, where the Old Guard keeps an empty throne. Defeat the other three
> claimants to take the crown.

| Field | Value |
|---|---|
| Victory | `VICTORY_DEFEAT_EVERYONE` (type 0, no metadata). The engine forces normal victory on and lets the AI win. |
| Loss | `LOSS_EVERYTHING` (type 0, no metadata). |
| Difficulty label | Normal (field = 1). Balanced for Normal. |
| Size | 72×72 (Medium). |
| Players | 4: Blue NW, Green NE, Red SW, Yellow SE. All four are flagged human **and** computer (`0x0F` both masks) so any slot can be a person or an AI. |
| Race | Random castle + random hero for every player (`KINGDOM_TOWNS` index 12, `KINGDOM_HEROES` race slot 6). Buildings fixed: Castle + dwelling 1 + dwelling 2 (`customBuildings`), so all four starts are identical and deterministic. |
| Heroes | Placed on the tile below each castle entrance (the engine moves them into the castle at load), unnamed so the engine picks a name for the rolled race. |

## 2. Geometry — a mirrored 3×3 grid

One quadrant (NW) is authored; everything is reflected across both axes (`x' = 71 − x`, `y' = 71 − y`).
Objects that sit on a seam (column 35 / row 35) are placed once per border zone. Anything whose
entrance or guard must stay *below* the object (castles, mines, guards) is positioned in absolute
space after the transform, never by mirroring the tile below.

```
 x: 0         20  26        45  51         71
    +-----------+---+------------+---+-----------+  y0
    | NW home   |###|  N ZONE    |###|  NE home  |
    | Harrowfld |###|  Ravensgate|###|  Rookhaven|
    | Blue  [A]->   |  (Barbar.) |   <-[A] Green |
    +--[B]--########+###[gate N]#+########--[B]--+  y20..26
    | W ZONE    |###|  CENTER    |###|  E ZONE   |
    | Greyfen   |###|  Kingsfall |###|  Mirefall |
    | (Necro) [gate W] (Knight)  [gate E]        |
    +--[B]--########+###[gate S]#+########--[B]--+  y45..51
    | SW home   |###|  S ZONE    |###|  SE home  |
    | Duskmere  |###|  Ashford   |###| Cinderholt|
    | Red   [A]->   |  (Barbar.) |   <-[A] Yellow|
    +-----------+---+------------+---+-----------+  y71
```

**The ridges.** Four 7-wide wasteland bands (x 20–26, x 45–51, y 20–26, y 45–51) carry the mountain
walls. Their construction follows the engine's passability rule (`Tile::updatePassability`): a
ground-object tile can be entered and left sideways or downwards unless the tile *below* it holds the
same object or a non-short object of the same sprite family, in which case it is fully impassable.

- **Vertical walls** (x = 23, x = 48) are contiguous stacks of big wasteland mountains (alternating the
  two mirror sprites of the same family), 3 rows apart except across the three corridors, where the
  spacing is 5 rows. Every mountain's bottom row therefore has the next mountain below it and is
  sealed; the only sideways-walkable wall tiles are the bottom row of the mountain directly above a
  corridor, and the corridor guard stands on the top corridor row, which covers that row too.
- **Horizontal walls** (y = 23, y = 48) only need every tile of their centre row occupied, because no
  object tile can be entered from above or left upwards. Big mountains every 5 columns plus 1-tile
  wasteland rocks and dead trees seal that row outside the gaps; the ridge bands are dressed with
  scattered wasteland rocks, dead trees, skulls and cracks (mirrored, clear of corridors and roads).
- **Corridors and guards** (absolute rows/columns): pass A rows 6–7 (mirror 64–65) in the vertical
  walls, guard on row 6 (64); pass B columns 8–10 (mirror 61–63) in the horizontal walls, guard on
  column 9 (62); the four center gates at 35–36, guard on 35. Each guard's 3×3 covers the corridor.
- **Proof.** The build itself runs a guard-sealed reachability check per hero using the engine's own
  protection rule (`getMonstersProtectingTile` with the pickup-on-tile exception disabled, exactly what
  `Heroes::ActionNewPosition` evaluates on arrival): without any fight each hero reaches exactly the 17
  unguarded objects of its own home and nothing else, or the build aborts. With monsters passable,
  every action object on the map is reachable.

**Fairness by construction, measured.** Castles face south, so a perfect mirror is impossible; instead
the layout is tuned so that engine movement costs from each castle to every objective match:

- The southern castles sit five rows further north than the mirror image ((9,59) instead of (9,64)),
  with their sawmill north-east of the castle and their home gold mine two tiles further east, so the
  hero's walk round the castle body to pass B and to free wood costs the same as in the north.
- The N/S seam prizes (gold mine, Mercenary Camp, alchemist lab) are entered from below, so the
  northern copies sit further north than the mirror image (rows 8/12/15 vs 60/57/54): the walk from
  each zone's pass row to each prize is then equal.
- Kingsfall's entrance is on row 34 so the N and S gate walks match once its passable tower tiles are
  taken into account; the cache spur roads run on the cache guards' rows (30 in the N, 43 in the S).
- Roads are authored tile lists, mirrored per quadrant. Toll towns and Kingsfall stand on column 35,
  half a tile off the mirror axis, so the eastern approach to each is one tile longer.

The generator prints the resulting cost table (engine rules: source-tile ground penalty, 75 on
road-to-road steps, diagonals ×1.5, fights ignored). Final values, in movement points:

| Objective | NW | NE | SW | SE | spread |
|---|---|---|---|---|---|
| pass A guard | 1623 | 1623 | 1523 | 1523 | 100 |
| pass B guard | 1225 | 1225 | 1386 | 1386 | 161 |
| own sawmill | 850 | 850 | 700 | 700 | 150 |
| own ore mine | 1050 | 1050 | 825 | 825 | 225 |
| home gold guard | 987 | 987 | 1100 | 1100 | 113 |
| N/S toll town gate | 2485 | 2560 | 2548 | 2535 | 75 |
| W/E toll town gate | 2349 | 2349 | 2585 | 2585 | 236 |
| N/S seam gold guard | 2698 | 2773 | 2573 | 2648 | 200 |
| N/S seam artifact | 2923 | 2998 | 2798 | 2760 | 238 |
| W/E seam gold guard | 2762 | 2762 | 2848 | 2848 | 86 |
| N/S gate guard | 3585 | 3622 | 3472 | 3509 | 150 |
| W/E gate guard | 3274 | 3274 | 3422 | 3422 | 148 |
| Kingsfall gate | 4174 | 4249 | 4322 | 4397 | 223 |
| own centre cache guard | 4249 | 4261 | 4359 | 4471 | 222 |

A hero has 1000–1500 movement points per day, so every spread is under a quarter of a day.

**Zones.** Home (21×21 usable, corner) has exactly two exits: pass A east/west into the N/S zone and
pass B north/south into the W/E zone. Border zones are shared basins between two neighbours; seam
objects (town, gold mine, lab or stones, major artifact) are the contested prizes, each half holds
"yours by proximity" objects. One guarded gate leads from each zone into the 18×18 wasteland center
(the Ashen Scar) with Kingsfall at (35,34), four Cyclops-guarded gold-mine caches on the diagonals,
unguarded Trading Posts beside the N/S seam roads and Hill Forts (on small dirt islands) on the W/E seam.

**Roads.** Castle apron → both passes; pass A exit → toll town along the zone's pass row; a seam road
from each toll town's gate to its center gate (splitting into two mirrored branches around the N/S
seam prizes, stepping round the W/E seam gold mine on both sides) and on into Kingsfall, with a spur
to each cache. No road runs through a guard's 3×3 except the pass and gate guards themselves.

**Terrain.** Dirt base; wasteland ridges and center scar; a small wasteland fen in every half of the
W/E zones (rows 28–31 / 40–43, one dirt row from the ridge so the minimap separates them); a grass
meadow around each home castle. Every patch is painted as overlapping two-row rectangles because the
engine reverts any painted tile that lacks a same-terrain neighbour both horizontally and vertically.
Decoration sprites match their ground (dirt/grass objects on dirt/grass, wasteland objects on
wasteland, generic dead trees and stumps anywhere) and are kept off every object's tower and crown
tiles (the builder reserves top-level sprite parts and the editor's town rectangle).

## 3. Content per region

### Home ×4 (authored NW coordinates; southern castles at (9,59) with sawmill (14,58) and gold mine (18,59))

| Object | Tile | Guard |
|---|---|---|
| Castle (random race, built) | (9,7), hero (9,8) | — |
| Sawmill (13,4) on the meadow, ore mine (4,15) | — | free: wood on day 1, ore on day 2 (day 1 for a Warlock) |
| Gold mine | (16,12) | 28 Rogues, days 4–7 |
| Gems mine | (2,3) | 6 Wolves, days 3–5 |
| Peasant Hut (15,17), Halfling Hole (15,19), Gazebo (18,8), Fountain (2,9), Windmill (18,16), Water Wheel (16,15) on a stream, Magic Garden (2,12) | — | — |
| Chest (18,2); chest (6,19) | — | second chest: 10 Boars directly below it |
| Random minor artifact | (1,20) | 20 Boars |
| Piles: wood, ore, gold, random resource | | — |
| Sign | (14,11) | names both roads and both toll towns for that quadrant |
| Pass A / Pass B | (23,6) / (9,23) | 14 Wolves / 30 Zombies, week 2 |

### N/S zone ("Ravens' Road", hill-clan country) — seam objects once, half objects ×2

| Object | Tile (N / S) | Guard |
|---|---|---|
| Barbarian town Ravensgate / Ashford (tent + DW1 + DW2) | (35,4) / (35,67) | garrison 55 Goblins, 32 Orc Chiefs, 18 Wolves, 11 Ogre Lords, 4 Trolls — month 2 |
| Gold mine | (35,8) / (35,60) | 30 Nomads, week 3 |
| Mercenary Camp | (35,12) / (35,57) | — |
| Alchemist lab (mercury) | (35,15) / (35,54) | 50 Orcs, week 3 |
| Random major artifact ("the toll town's vault") | (35,0) / (35,71) | 25 Ogres |
| Half: sulfur mine (29,3) | | 25 Orcs, week 2 |
| Half: Wagon Camp (31,7), Witch Doctor's Hut (27,13), Fort (29,17), Observation Tower (27,5), campfire (27,15), ore + wood piles | | — |
| Half: chest + gold pile (30,13)/(31,13) | | 15 Nomads, week 2 |
| Half: random treasure artifact (30,19) | | 25 Nomads, week 3 |
| Half: sign (29,10) | | — |
| Gate N / S | (35,23) / (35,48) | 25 Ogre Lords, month 2 |

### W/E zone ("the Greyfen", dead country) — seam objects once, half objects ×2

| Object | Tile | Guard |
|---|---|---|
| Necromancer town Greyfen / Mirefall (tent + DW1 + DW2) | (6,33) | garrison 70 Skeletons, 36 Mutant Zombies, 22 Royal Mummies, 9 Vampires, 3 Liches — month 2 |
| Gold mine | (16,35) | 30 Mummies, week 3 |
| Standing Stones | (11,35) | — |
| Random major artifact | (1,35) | 30 Mummies |
| Half: crystal mine (5,29) in the fen | | 30 Zombies, week 2 |
| Half: Graveyard (11,28) | | self-defending |
| Half: Ruins/Medusae (16,31), Shrine of the 2nd Circle (18,32), Tree of Knowledge (14,30), chest (13,33), gold/crystal/wood piles, campfire (1,27), sign (11,31) | | — |
| Gate W / E | (23,35) / (48,35) | 22 Minotaur Kings, month 2 |

### Center ("the Ashen Scar")

| Object | Tile | Guard |
|---|---|---|
| Kingsfall — Knight castle, built: Castle, Tavern, Well, Marketplace, Mage Guild 1, DW1–DW4 | (35,34) | Old Guard: 40 Rangers, 30 Veteran Pikemen, 18 Master Swordsmen, 12 Champions, 5 Crusaders — month 2 late for a human, month 3 for the AI |
| Caches ×4: gold mine (29,29) + gold pile + chest + random treasure artifact | diagonals | 8 Cyclops each, month 2 |
| Trading Post ×2 | (32,26) / (32,45) | — |
| Hill Fort ×2 | (30,36) / (41,36) | — |

Random artifacts carry an explicit choice list of the original game's artifacts (ids 9–81) of the
right level, so they resolve without Price of Loyalty assets.

## 4. Economy and pacing

Per player: castle 1000/day, home gold mine by day ~5, one contested seam gold mine per border zone
(week 3), one center cache mine (month 2). Rare resources: gems at home, sulfur in the N/S half,
crystal in the W/E half, mercury contested on the N/S seam. That is the "Rich" tier the brief asked for.

| Phase | Days | What happens |
|---|---|---|
| Week 1 | 1–7 | Clear home: gems, gold, chests, dwellings, XP objects. |
| Week 2 | 8–14 | Break both passes, take the half mines in the border zones. |
| Week 3 | 15–21 | Seam gold mines and lab; first contact with neighbours in the basins. |
| Month 2 | 29–45 | Gates, toll towns, center caches; the border wars. |
| Month 2–3 | 45+ | Kingsfall and the decisive campaign. |

**Calibration.** `mapgen strength ashen_succession` prints every stack with the engine's
`Troop::GetStrength`; `mapgen/guard_model.py` compares them to reference armies built from the
engine's own growth numbers (DW1+DW2 at start, DW3 day 3, DW4 day 6, DW5 day 12, DW6 day 24,
everything recruited), with neutral weekly growth for wandering stacks and the engine's neutral-town
joins for garrisons (`Castle::_joinRNDArmy`: one join per week from week 2, a second with 40% for
towns and always for castles; only the town race's base creatures with an existing stack land, so the
toll towns grow by ~200 strength by month 2 while Kingsfall, all upgraded creatures, never grows).
Every guard sits inside its band on the worst race except the lab (Warlock 0.45, the band floor):

| Guard | Ratio (Knight … Warlock) | Band |
|---|---|---|
| Home gold 28 Rogues / gems 6 Wolves / chest 10 Boars, week 1 (day 5) | 0.87 / 0.83 / 0.71 … 0.58 / 0.55 / 0.47 | 0.30–0.90 |
| Pass A 14 Wolves / pass B 30 Zombies, week 2 | 0.69 / 0.52 … 0.50 / 0.38 | 0.35–0.70 |
| Sulfur 25 Orcs / crystal 30 Zombies / cache 15 Nomads, week 2 | 0.50 / 0.52 / 0.61 … 0.36 / 0.38 / 0.45 | 0.35–0.70 |
| Seam gold 30 Nomads / 30 Mummies, week 3 | 0.71 / 0.70 … 0.55 / 0.54 | 0.45–0.80 |
| Lab 50 Orcs / treasure 25 Nomads / seam artifacts 25 Ogres, 30 Mummies, week 3 | 0.58 / 0.59 / 0.78 / 0.70 … 0.45 / 0.46 / 0.60 / 0.54 | 0.45–0.80 |
| Gates 25 Ogre Lords / 22 Minotaur Kings, month 2 | 0.80 / 0.80 … 0.59 / 0.59 | 0.55–0.95 |
| Cache 8 Cyclops, month 2 | 0.86 … 0.63 | 0.55–0.95 |
| Toll towns (with growth), month 2 | 0.85–0.91 … 0.63–0.67 | 0.55–0.95 |
| Kingsfall, month 2 late | 0.87 … 0.63 | capstone |

The AI attacks a neutral only above 1.5× its strength (a built castle's garrison also gets the tower
and a ×1.25 melee penalty), so every "week 2" guard falls to a human in week 2 and to the AI in
week 3, and the AI reaches Kingsfall in month 3.

**AI.** Every AI slot has a placed hero, free wood + ore, and week-2 breakable passes so it expands
and fights. The AI pathfinder treats stronger guards as walls, so gates and towns stay closed to it
until month 2, exactly as for humans. AI hero limit for a 72-wide map is 4. As on any road-linked map,
an enemy hero within about two days of an AI castle (the shared basins qualify) puts that castle into
the engine's defensive mode until the threat moves on.

## 5. Flavor

- **Signs**: one per home naming its two roads and the toll towns they lead to (quadrant-aware);
  one per border-zone half beside the road into the zone.
- **Events** (all colours): day 1 the succession; day 8 the toll towns shut their gates; day 29
  Kingsfall stops answering letters; day 57 the crown mines pay for the war.
- **Rumors**: the Old Guard's oath, the crown mines and their one-eyed guards, the hill-clans, the
  fen-wardens, the wolves since the harvest, the last king's walls.
- The description states the objective, the garrison warning, and that the toll towns and Kingsfall
  are capturable seats of power. No recurring resource penalties.

## 6. Strategies the map supports

- **Border rush**: break pass A first, take the N/S seam gold mine and lab before the neighbour, then
  hold the basin.
- **Fen route**: break pass B, take crystal and the W/E seam gold, use the Graveyard artifact and Ruins
  Medusae for an early edge.
- **Turtle and scale**: clear home, take both halves, build to DW6, then go through a gate for the
  Cyclops caches and Kingsfall's Old Guard recruits.
- **Kingsfall**: whoever takes the capital gains a second built castle with four dwellings, a
  Marketplace and a Mage Guild in the middle of the map, plus up to four gold mines around it.
