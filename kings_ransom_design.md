# The King's Ransom — Scenario Design Document

Map file: `kings_ransom.fh2m` (FH2M v13, generated against fheroes2 @ `b086d1aa8b921163712aec2fb8188f4d0d375b09`)
Generator: `mapgen/` (C++ tool linking the real fheroes2 engine; deterministic, seed 20260901)

---

## 1. Premise and objective

> The King is taken. The Iron Company demands **100,000 gold** before the second month ends.
> As Steward of Highmarch you must raise the full sum and **hold it in your treasury on day 56**.
> Lord Renfrew of Ironvale covets the empty throne: he pays nothing and profits from your failure.
> Defeating him alone will not save the King — only the gold will.

- **Victory**: possess 100,000 gold at any moment (engine checks *current treasury*, `VICTORY_COLLECT_ENOUGH_GOLD = 5`, metadata `[100000]`).
- **Loss**: out of time after day 56 (`LOSS_OUT_OF_TIME = 3`, metadata `[56]`; the engine check is `CountDay() > 56`, so day 56 is fully playable and defeat lands at the start of day 57).
- `isVictoryConditionApplicableForAI = false` — Green cannot win by hoarding gold.
- `allowNormalVictory = false` — eliminating Green does **not** win the game.
- Difficulty: designed and balanced for **Normal** (map difficulty field = 1). The description tells the player the terms plainly.

Because the win condition reads the *current* treasury, every gold spent is a real decision: troops, heroes, buildings and losses all push the win day back; income assets pull it forward. That tension — *how much do I spend now to be richer later?* — is the entire scenario.

## 2. Size and players

- **36×36 (Small)** — deliberate. The clock is 56 days; travel time must season the economy problem, not drown it. Community research (Ururam Tururam, Kuzmanov) warns against oversizing; a 72×72 version would either dilute density fourfold or double the object count without adding decisions. A small, dense, 3-lane map keeps every turn meaningful.
- **Blue** — human, Knight, castle **Highmarch** (8,28), hero **Sir Aldric** placed at the gate (moved into the castle at game start by the engine).
- **Green** — AI, Knight, castle **Ironvale** (28,6), with an explicitly placed starting hero (no reliance on the engine's no-hero fallback — FH2M maps never auto-spawn a first hero).
- Both castles start with identical authored buildings: Castle + Thatched Hut + Archery Range (`customBuildings`), removing the engine's 50% coin-flip on the second dwelling and keeping the start symmetric and deterministic.
- Neutral Knight **town** "Marketstead" (19,14) in the map's centre, garrisoned (35 Peasants / 12 Archers / 15 Pikemen / 8 Swordsmen / 3 Cavalry), holding only a Tent + Thatched Hut — capturing it yields 250/day and a 5000-gold upgrade decision.

## 3. Topology — three lanes

A mountain ridge runs NW→SE and splits the map into Blue's south-west and Green's north-east. Three lanes cross it, plus a contested basin at its heart:

```
   snow          north corridor          Green home
   (dwarf     (dead country, zombies,   (Ironvale, sawmill, ore,
    country)    graveyard, ToK)          lightly-guarded gold mine)
      |               |                        |
   West Road     [ridge + vale]           east lane (sulfur mine)
   gold mine  <- King's Vale basin ->          |
   crystal mine  (Marketstead, King's     Pass C (medusas)
      |           Mine, gems, trading          |
   Blue home      post, rogue camp)       Southern Sands
   (Highmarch)        ^                   (sphinx, hoard,
      \            Pass A (wolves)         Endless Purse)
       \______________|_______________________/
              south strip (campfire, sign)
```

- **Blue home (SW, grass)**: free sawmill and ore mine (Kristo's rule: every start needs unguarded wood+ore inside ~1 turn), water wheel on a stream, two chests (one rogue-guarded), gazebo, fountain, faerie ring, peasant hut, archer's house, pond, sign pointing east. The first fifteen seconds show a working march, not an empty field.
- **West Road (snow)**: the *safe, slow* lane. Roads all the way. Western gold mine (35 Dwarves), crystal mine (14 Dwarves), windmill, observation tower, graveyard (artifact + 1000g fight), chest, piles. Continues north to the corridor.
- **North corridor**: dead-country flavor (stumps, dead trees) with a 28-Zombie midway guard, Witch Doctor's Hut, Tree of Knowledge, free pickups — the long way to Green's flank.
- **King's Vale (central dirt basin)**: the contested heart. The **King's Mine** (gold, 50 Nomads — "the Iron Company"), gems mine (25 Nomads), **Trading Post** (15 Rogues — grants 3-marketplace exchange rates), Rogue Wagon Camp (buy cheap chaff), magic garden, gold/sulfur/crystal piles, chest. Entrances: **Pass A** from Blue (16 Wolves, on the road) and the **NE gate** from Green (24 Rogues). The ridge's west flank is sealed so Pass A cannot be skipped.
- **Green home (NE)**: mirror economy — sawmill, ore mine free; gold mine behind 12 Rogues (the AI cracks it in week 2); magic garden, piles, chests; a sulfur mine (10 Rogues) on its east lane. Green is a real economic actor and a raid target.
- **Southern Sands (SE desert)**: the *optional treasure route*. Desert movement itself costs 200% — distance is part of the price. Sphinx (riddle answer "ransom" → 3000 gold), two campfires, chests, Skeleton remains, Desert Tent (Nomad dwelling), the SE hoard (2 chests + 2 gold piles around 35 Rogues), and the capstone: **Endless Purse of Gold** (+500 gold/day) behind 50 Nomads. **Pass C** (30 Medusas) links the desert to Green's lane — a backdoor in both directions.

Roads telegraph the two main arteries (Highmarch→vale→Ironvale; Highmarch→west mine) and nothing else; the desert deliberately has no road.

## 4. Economy design

Baseline facts (all verified in engine source at the pinned commit):
- Start: 7,500 gold, 20 wood, 20 ore, 5 of each rare. No income on day 1.
- Castle 1,000/day; town 250/day; gold mine 1,000/day; statue +250/day; hero 2,500; town→castle 5,000+20w+20o.

**Passive ceiling ≈ 62,500 gold** (castle income alone) — waiting loses. Even castle + statue + weekly objects tops out ≈ 87k. To win you must *acquire income* and *spend well*:

| Asset | Where | Cost to take | Return |
|---|---|---|---|
| Western gold mine | west road | 35 Dwarves (~wk 2) | 1,000/day |
| King's Mine | vale | 50 Nomads (~wk 3) | 1,000/day |
| Marketstead | vale | real garrison (~wk 3) | 250/day → 1,000/day for 5,000g |
| Green's gold mine | NE | 12 Rogues + AI pressure | 1,000/day |
| Ironvale | NE | Green's army | 1,000/day |
| Endless Purse | desert | 50 Nomads + desert travel | 500/day |
| Statues (x2 possible) | castles | 1,250g each | 250/day each (5-day payback) |
| Trading post / marketplaces | vale / build | fight / 500g | rares → gold at up to 100g/unit |
| Chests (9) | everywhere | varies | ~1,000–2,000g **or** XP — the classic choice |

The rare-resource economy exists on purpose: crystal/gems/sulfur mines and piles are worth little to a Knight's build order beyond Mage Guild — their real value is **late liquidation** through the Marketplace/Trading Post ("Marketstead's traders will turn anything into gold — for a cut"). Knight's Cathedral (20 crystal) gives the crystal mine a second use for players investing in Crusaders.

### Modeled outcomes (`mapgen/economy_model.py`, frictionless; real play adds ~3–5 days)

| Play | Frictionless win | Expected real win |
|---|---|---|
| Passive / bad | LOSS (62k–91k) | LOSS |
| Mediocre (1 mine d17, town d28) | day 44–48 | **day 50–56** |
| Good, conservative west | day 41 | **day 45–48** |
| Excellent, vale rush | day 38 | **day 40–44** |
| Excellent, aggression vs Green | day 40 | **day 40–44** |

This matches the commissioned difficulty curve: excellent ≈ 40–44, good ≈ 45–50, imperfect ≈ 51–56, mistakes → failure. There is ~35–40k of slack above the threshold in a competent line, so the scenario is not mathematically brittle and no single lucky chest decides it.

## 5. The three strategies

- **A. Conservative expansion (west)**: statue + marketplace early, second hero, crack the Dwarves ~day 11, loop the snow road (crystal, windmill, chests, graveyard), take Marketstead when the army matures. Reliable, road-assisted, low interference from Green — but slower, and the vale may fall to Green in the meantime.
- **B. Capital investment (vale rush)**: spend hard in week 1 (troops + Archer's House purchases), break Pass A, take Marketstead ~day 12 and the King's Mine ~day 16. Highest income curve and denies Green the centre — at the cost of an early treasury trough and real combat losses against 50 Nomads.
- **C. Aggression (against Green)**: as B, then push through the NE gate: Green's gold mine, then Ironvale ~day 34–38. Removes AI interference and adds 2,000/day — but the army bill is the biggest, and killing Green *does not win*; you must still bank 100,000.
- **The desert seasons all three**: a mid-game detour worth ~8–10k plus 500/day from the Purse — strongest for players who are behind schedule and need a second engine, and priced in movement across 200%-cost sand.

Chest gold-vs-XP is a live decision throughout: XP early reduces combat losses against the big Nomad stacks; gold is the objective itself. (Community rule: gold when poor and pressured — this map keeps you both.)

## 6. Guards — stepladder and theme

Guards scale with prize value and are themed to their region (Kuzmanov: no Orcs at the Faerie Ring):

| Guard | Stack | Protects |
|---|---|---|
| Rogues 8 | home chest, Pass B | pocket change |
| Dwarves 14 / 35 | crystal / western gold mine | snow country ("cold hands, honest scales") |
| Wolves 16 | Pass A | the ridge pass |
| Rogues 12–24 | trading post, NE gate, Green's mines | bandit country |
| Zombies 28 | north corridor | the dead country by the graveyard |
| Nomads 25–50 | gems, King's Mine, desert gate, Endless Purse | the Iron Company mercenaries |
| Medusas 30 | Pass C | the old desert pass |
| Rogues 35 | SE hoard | the Company's own coffers |

All counts are **authored** (deterministic, always hostile per engine rules for map-designer counts) — no random-stack RNG on strategically critical fights. Self-defending objects (graveyard, town) carry no redundant outer guards.

## 7. AI pressure model

Green (Normal AI, verified: no economic or vision cheats) is set up to function:
- Built castle + placed hero → it can recruit a second hero on turn 1 (2,500g from its 7,500 start).
- Free sawmill/ore + a 12-Rogue gold mine → its economy comes online in week 2 without help.
- Its lane into the vale (NE gate, 24 Rogues) is breakable by a week-2–3 AI army; the AI pathfinder treats over-strong guards as walls, so all Green-side guards are kept under the "week-2–3 AI army" bar.
- Expected behavior: expand at home in week 1–2, contest Marketstead / the gems mine / the trading post from week 3, and pressure Blue's vale holdings mid-game. Pass C gives it a second theatre. The map is not a rush-defense scenario — Green's shortest route to Highmarch runs through two guarded gates and the neutral town.

## 8. Aesthetics and readability

- Terrain boundaries are random-walk ragged (snow treeline, desert fringe) — no painted rectangles; transitions are computed by the engine's own editor code, so every border tile is a proper transition sprite.
- Density gradients per the Celestial Heavens checklist: object clusters thin out near roads, thicken near forests and landmarks; sprite variety is rotated (six tree families, four rock families, dirt mounds, dunes, lakes, flowers, cacti).
- Landmark composition: the water wheel on its stream, the snow graveyard, the volcanic-gray pass C scar (wasteland patch), the sphinx among dunes, the walled hollow behind the King's Mine.
- The minimap reads at a glance: white NW wedge, green mass, brown diagonal ridge, sand SE corner.
- Flavor: three daily events (day 1 brief, day 29 "one month remains", day 50 "final week") — **no recurring resource penalties** (the community's most-hated crutch); five rumors that hint at the four routes; two signs; a riddle whose answer is the scenario's own theme.

## 9. Why it should be fun

Every week asks a sharper version of the same question: *is this gold working for me or waiting for the Company?* Week 1: statue and marketplace, or troops? Week 2: west (safe) or vale (rich)? Week 3: the King's Mine bill comes due — pay it in Crusader-blood or walk away? Week 4+: raid Green, cash the desert, or sit on the pile and watch the counter? The deadline converts even the dead days between fights into decisions (liquidate rares now at 74g, or wait for a third marketplace at 100g?). And on day 56, the treasury is either a ransom or an epitaph.
