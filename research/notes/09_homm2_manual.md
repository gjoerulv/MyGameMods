# HoMM2 Original Gameplay Documentation — Economy Scenario Research

Compiled 2026-09-01. Question-driven extraction for an economy-focused scenario design.

## Sources used

1. **[MANUAL]** *Heroes of Might and Magic II* (Gold, incl. Price of Loyalty expansion manual) — official scanned manual, OCR full text.
   Internet Archive item `HEROES_OF_MIGHT_MAGIC_II`: https://archive.org/details/HEROES_OF_MIGHT_MAGIC_II
   (full text: https://archive.org/download/HEROES_OF_MIGHT_MAGIC_II/HEROES_OF_MIGHT__MAGIC_II_djvu.txt — print artifact "Heroes II Manual 4/12/01 ... Page N" preserves original page numbers; page cites below use these.)
2. **[GUIDE]** Joe Grant Bell, *Heroes of Might and Magic II: The Succession Wars — The Official Strategy Guide* (Prima, 1996) — OCR full text.
   Internet Archive item `homm-2-guide`: https://archive.org/details/homm-2-guide
3. **[FH2]** fheroes2 engine source (faithful open-source re-implementation of the original HoMM2 engine; used only where the two print sources give no number, always labeled): https://github.com/ihhub/fheroes2 — files cited per item.

Note: ManualShelf ("HEROES OF MIGHT & MAGIC II User Guide") and ManualMachine ("Heroes of Might and Magic II Gold User Manual") both return HTTP 403 to automated fetching and could not be used; the archive.org scan above is the same Gold-edition manual.

OCR caveat: the manual's cost boxes print resource-type **icons** which OCR loses; gold values and quantities are legible, resource *types* are taken from [GUIDE] tables (which spell them out) where marked.

---

## 1. Economy: income sources

| Source | Income | Citation |
|---|---|---|
| Castle | 1000 gold/day | [MANUAL] p.13 (tutorial) and p.34: "Towns provide you with 250 gold per day, and castles provide you with 1000 gold per day." |
| Town (unupgraded) | 250 gold/day | [MANUAL] pp.13, 34 |
| Gold mine | 1000 gold/day | [MANUAL] p.13: "1000 for each gold mine" |
| Sawmill (lumber mill) | 2 wood/day | [MANUAL] p.14 (tutorial): "The sawmill produces two units of wood each turn as long as you own it." |
| Ore mine | 2 ore/day | NOT in manual or guide; [FH2] `src/fheroes2/kingdom/profit.cpp` `ProfitConditions::FromMine` |
| Mercury / Sulfur / Crystal / Gems mine (Alchemist's Lab etc.) | 1 unit/day each | NOT in manual or guide; [FH2] `profit.cpp` |
| Statue | +250 gold/day to that castle | [MANUAL] p.38 ("increases the income of a castle by 250 gold") and p.94 (structure box: "Increases income of town by 250"); [GUIDE] p.36 "Statues provide an extra 250 Gold per turn" |
| Warlock Dungeon (special building) | +500 gold income | [MANUAL] p.38 ("generates 500 gold more income in the castle" — period not stated). [GUIDE] p.~34 says "adds 500 Gold per week" but [FH2] `profit.cpp` implements it as **+500 gold/day**; the guide wording appears to be an error. |
| Gold-producing artifacts (e.g. Endless Purse/Bag/Sack of Gold) | listed in [GUIDE] Table 5-5 "Gold Producing Artifacts" | not extracted here |

The manual explicitly lists mines/sawmill/lighthouse as "Continual Sites" (flagged, keep paying while owned) — [MANUAL] p.33.

## 2. Marketplace and exchange rates

- Manual text: "Allows you to convert resources you have into other resources. The more Marketplaces you own, the better the exchange rate ... the exchange is never one for one, and the rate plateaus when you control nine Marketplaces." — [MANUAL] p.38; same statement in structure box p.94.
- [GUIDE] p.35 confirms: "the exchange rate increases with each Marketplace, maxing out at nine marketplaces."
- **Neither the manual nor the strategy guide prints a numeric exchange-rate table.** The exact table below is from [FH2] `src/fheroes2/kingdom/resource_trading.cpp` (labeled engine data, not manual data). "Common" = wood/ore; "rare" = mercury/sulfur/crystal/gems. Read as cost of 1 unit of the target.

| # Marketplaces | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|
| resource→same-class resource (give N, get 1) | 10 | 7 | 5 | 4 | 4 | 3 | 3 | 3 | 2 |
| common→rare (give N common, get 1 rare) | 20 | 14 | 10 | 8 | 7 | 6 | 5 | 5 | 4 |
| rare→common (give N rare, get 1 common) | 5 | 4 | 3 | 2 | 2 | 2 | 2 | 2 | 1 |
| sell 1 common → gold received | 25 | 37 | 50 | 62 | 74 | 87 | 100 | 112 | 124 |
| sell 1 rare → gold received | 50 | 74 | 100 | 124 | 149 | 175 | 200 | 224 | 249 |
| buy 1 common ← gold paid | 2500 | 1667 | 1250 | 1000 | 834 | 715 | 625 | 556 | 500 |
| buy 1 rare ← gold paid | 5000 | 3334 | 2500 | 2000 | 1667 | 1429 | 1250 | 1112 | 1000 |

- Adventure-map **Trading Post** trades at the 3-marketplace rate ([FH2] `dialog_marketplace.cpp`: `tradingPost ? 3 : count`); [GUIDE] Table 5-18 describes it as "Exchange resources at a low ratio, once per week" (the "once per week" claim appears only in the guide).

## 3. Knight castle buildings: costs, dependencies, boosters

Town→Castle upgrade: **5000 gold + 20 wood + 20 ore** — [MANUAL] p.34; [GUIDE] p.~33.
Only one structure/upgrade may be built per castle per turn — [MANUAL] p.36.

Common structures ([MANUAL] pp.93–95 boxes; resource types from [GUIDE] where noted):

| Building | Cost | Effect / notes |
|---|---|---|
| Mage Guild L1 | 2000 gold + 5 + 5 (wood+ore) | teaches spells; upgradable to L5, "additional levels increasingly more expensive" ([MANUAL] p.93). [GUIDE] Table 3-1: L2–L5 = 1000 gold each; OCR shows rare-resource requirement rising 4/6/8/10 per level (columns partly garbled — treat wood/ore split as unverified). |
| Tavern | 500 gold + 5 (wood) | defender morale bonus + weekly rumor; not in Necromancer towns ([MANUAL] pp.37, 93) |
| Thieves' Guild | 750 gold + 5 (wood) | player intel; more guilds = more info, 5 guilds = complete ([MANUAL] pp.37–38, 93) |
| Shipyard | 2000 gold + 20 (wood) | allows ships; each ship costs **1000 gold + 10 wood** ([MANUAL] pp.38, 93–94) |
| Statue | 1250 gold + 5 (ore) | +250 gold/day ([MANUAL] p.94) |
| Marketplace | 500 gold + 5 (wood) | resource trading ([MANUAL] p.94) |
| Well | 500 gold | +2/week to every dwelling's growth ([MANUAL] pp.38, 94) |
| Horde Building — Knight's is the **Farm** | 1000 gold | +8/week to lowest dwelling (Peasants) ([MANUAL] pp.38, 95; Farm name from [GUIDE] p.~35) |
| Left Turret / Right Turret | 1500 gold + 5 (ore) each | extra ballista shot each ([MANUAL] pp.39, 95) |
| Moat | 750 gold | stops ground movement; −3 Defense while in moat ([MANUAL] p.95) |
| Captain's Quarters | 500 gold | hires Captain of the Guard for town defense ([MANUAL] p.39 describes; cost only in [GUIDE] Table 3-1) |
| Fortifications (Knight special) | 1500 gold + 5 + 15 (wood+ore per [GUIDE] garbled; manual icons lost) | toughens castle walls vs catapult ([MANUAL] pp.38, 101) |

Knight dwellings ([MANUAL] pp.98–101 boxes; resource types confirmed by [GUIDE] Ch.3 pp.~39–41 and Table 3-3):

| Building | Cost | Requires | Produces |
|---|---|---|---|
| Thatched Hut | 200 gold | — | 12 Peasants/week |
| Archery Range | 1000 gold | Thatched Hut | 8 Archers/week |
| Archery Range Upgrade (→Rangers) | 1500 gold + 5 wood | Armory, Blacksmith, Archery Range | — |
| Blacksmith | 1000 gold + 5 ore | Well, Thatched Hut | 5 Pikemen/week |
| Blacksmith Upgrade (→Veteran Pikemen) | 1500 gold + 5 ore | Blacksmith, Armory | — |
| Armory | 2000 gold + 10 wood + 10 ore | Tavern, Thatched Hut | 4 Swordsmen/week |
| Armory Upgrade (→Master Swordsmen) | 2000 gold + 5 wood + 5 ore | Armory, Blacksmith | — |
| Jousting Arena | 3000 gold + 20 wood | Blacksmith, Armory | 3 Cavalry/week |
| Jousting Arena Upgrade (→Champions) | 3000 gold + 10 wood | Jousting Arena | — |
| Cathedral | 5000 gold + 20 wood + 20 crystal | Blacksmith, Armory | 2 Paladins/week |
| Cathedral Upgrade (→Crusaders) | 5000 gold + 10 wood + 10 crystal | Cathedral | — |

[GUIDE] p.~41 "Knight's Bottom Line" (dwellings only, excl. Mage Guild/common): **25,200 gold, 80 wood, 25 ore, 30 crystals**. Manual note ([MANUAL] p.99): "The Knight castle has the most structures, and requires vast amounts of wood. A small amount of ore ... crystals only at the highest level."

Growth boosters: Well +2/dwelling/week; Farm (horde) +8 Peasants/week; dwellings replenish on Day 1 of each week ([MANUAL] pp.36–38). Upgrading a dwelling auto-upgrades unpurchased creatures; upgrading owned units costs double the price difference per creature ([MANUAL] p.37).

## 4. Creature stats, costs, growth

Knight line ([MANUAL] pp.75–77 stat boxes; weekly growth from dwelling boxes pp.98–101, before Well/Farm):

| Creature | Att | Def | Dmg | HP | Speed | Shots | Cost (gold) | Growth/wk | Special |
|---|---|---|---|---|---|---|---|---|---|
| Peasant | 1 | 1 | 1-1 | 1 | Very Slow | 0 | 20 | 12 | — |
| Archer | 5 | 3 | 2-3 | 10 | Very Slow | 12 | 150 | 8 | ranged |
| Ranger | 5 | 3 | 2-3 | 10 | Average | 24 | 200 | (upg) | fires 2 shots/turn |
| Pikeman | 5 | 9 | 3-4 | 15 | Average | 0 | 200 | 5 | — |
| Veteran Pikeman | 5 | 9 | 3-4 | 20 | Fast | 0 | 250 | (upg) | — |
| Swordsman | 7 | 9 | 4-6 | 25 | Average | 0 | 250 | 4 | — |
| Master Swordsman | 7 | 9 | 4-6 | 30 | Fast | 0 | 300 | (upg) | — |
| Cavalry | 10 | 9 | 5-10 | 30 | Very Fast | 0 | 300 | 3 | — |
| Champion | 10 | 9 | 5-10 | 40 | Ultra Fast | 0 | 375 | (upg) | — |
| Paladin | 11 | 12 | 10-20 | 50 | Fast | 0 | 600 | 2 | 2 attacks |
| Crusader | 11 | 12 | 10-20 | 65 | Very Fast | 0 | 1000 | (upg) | 2 attacks; immune to Curse; x2 dmg vs undead |

Neutral guards ([MANUAL] pp.90–91):

| Creature | Att | Def | Dmg | HP | Speed | Shots | Cost | Special |
|---|---|---|---|---|---|---|---|---|
| Rogue | 6 | 1 | 1-2 | 4 | Fast | 0 | 50 | no enemy retaliation |
| Nomad | 7 | 6 | 2-5 | 20 | Very Fast | 0 | 200 | — |
| Ghost | 8 | 7 | 4-6 | 20 | Fast | 0 | (no cost printed in OCR; ghosts cannot join armies per [MANUAL] p.90) | flies; undead; creatures killed by ghosts become ghosts |
| Medusa | 8 | 9 | 6-10 | 35 | Average | 0 | 500 | 20% chance to petrify for the combat |
| (Genie, same page) | 10 | 9 | 20-30 | 50 | Very Fast | 0 | 650 + 1 gem | flies; 10% halve enemy unit |

## 5. Adventure pickups and weekly income sites

- **Treasure Chest**: "Gold or a minor artifact can be found in treasure chests. You can keep the gold you get, or convert the gold to experience points." — [MANUAL] p.32. [GUIDE] Table 5-18: "Find 1000 Gold, 1500 Gold, 2000 Gold or a Treasure artifact." Neither print source gives the experience amounts or odds; [FH2] `maps_tiles_helper.cpp` + `heroes_action.cpp`: land chest rolls 32% 1000g, 32% 1500g, 31% 2000g, 5% Treasure artifact; experience option = **gold − 500** (i.e. 500/1000/1500 xp).
- **Campfire**: [GUIDE] Table 5-18: "Find gold and a random resource" (no numbers). [FH2]: 4–6 of one random resource + 100 gold per resource unit (i.e. 400–600 gold).
- **Water Wheel**: [GUIDE] Table 5-18: "Collect 500 or 1000 Gold once per week." [FH2]: 500 gold if taken in week 1, 1000 gold thereafter; refills weekly. NOT described in the manual.
- **Windmill** ("Mill"): [GUIDE] Table 5-18: "Find 2 units of a random resource, weekly." [FH2]: 2 units of random non-wood resource. Manual only cites the windmill as an example of a once-per-week site ([MANUAL] p.33).
- **Magic Garden**: [GUIDE] Table 5-18: "Receive 500 Gold or 5 gems" (weekly refill per [FH2]). NOT in the manual.
- Others from [GUIDE] Table 5-18: Flotsam 25% 5 wood / 25% 5 wood+200g / 25% 10 wood+500g / 25% nothing; Lean-to 1–4 of a resource (60%) or nothing; loose resource piles = "small, random amount" ([MANUAL] p.32).

## 6. Difficulty levels: starting resources

[MANUAL] p.20 says only: "Game difficulty affects the resources you begin with, the resources the computer players begin with, and the intelligence of the computer opponents." **No numbers in the manual.**

Numbers from [GUIDE] Ch.6 "Computer's Starting Values" (pp.~137–139); cross-checked against [FH2] `kingdom.cpp` `_getKingdomStartingResources`:

| Difficulty | Human start | AI start | AI economy cheats ([GUIDE]) |
|---|---|---|---|
| Easy | 10,000 gold, 30 wood, 30 ore, 10 each rare | 7,500 gold, 20 wood/ore, 5 each rare | none (crippled AI) |
| Normal | 7,500 gold, 20 wood, 20 ore, 5 each rare | 10,000 gold, 30 wood/ore, 10 each rare ([GUIDE]); [FH2] uses 7,500/20/5 for AI at Normal — sources disagree | none |
| Hard | 5,000 gold, 10 wood, 10 ore, 2 each rare | 10,000 / 30 / 10 | +10% gold income; +1 free wood and ore per turn |
| Expert | 2,500 gold, 5 wood, 5 ore, 0 rare | 10,000 / 30 / 10 | +25% gold income; +1 of every resource per turn |
| Impossible | 0 gold, 0 resources | 10,000 / 30 / 10 | +100% gold income (double); +2 of every resource per turn |

(Rare = mercury, sulfur, crystal, gems. AI attack thresholds per [GUIDE]: Hard 60-40, Expert 75-25, Impossible 90-10 hit-point advantage.)

## 7. Movement

- Heroes move at the speed of the slowest unit in their army (unless in a ship) — [MANUAL] pp.33, 45.
- **Base movement points are NOT quantified in manual or guide.** [FH2] `heroes.cpp` `GetMaxMovePoints`: land base by slowest troop speed — Very Slow 1000, Slow 1100, Average 1200, Fast 1300, Very Fast 1400, Ultra Fast 1500; sea base 1500 (+500 per owned lighthouse).
- **Road: 25% bonus** — [MANUAL] p.45: "you get a 25% bonus to movement while traveling on a road" (road row = 75% cost in the table).
- Terrain movement-cost table ([MANUAL] p.45, "% of normal movement", 200% = half speed):

| Terrain | No Pathfinding | Basic | Advanced | Expert |
|---|---|---|---|---|
| Desert | 200% | 175% | 150% | 100% |
| Swamp | 175% | 150% | 125% | 100% |
| Snow | 175% | 150% | 100% | 100% |
| Cracked (wasteland) | 125% | 100% | 100% | 100% |
| Beach | 125% | 100% | 100% | 100% |
| Lava | 100% | 100% | 100% | 100% |
| Water | 100% | 100% | 100% | 100% |
| Dirt | 100% | 100% | 100% | 100% |
| Grass | 100% | 100% | 100% | 100% |
| Road | 75% | 75% | 75% | 75% |

- Logistics: +10/20/30% land movement (Basic/Advanced/Expert); Navigation: +50/100/150% sea — [MANUAL] p.47.

## 8. Heroes: hiring and cap

- "All heroes cost **2500 gold** to hire." — [MANUAL] p.39 (Recruit Hero; two candidates offered per castle).
- "you have a **maximum of eight heroes**" — [MANUAL] p.44 (hero trading/dismiss section). Max eight secondary skills per hero, three levels each ([MANUAL] p.46).
- Knight/Barbarian heroes buy a spellbook for 500 gold at a Mage Guild ([MANUAL] p.37).
- Losing all towns/castles gives 7 days to capture one or you lose ([MANUAL] p.34).

## 9. Week/month structure and growth events

- Time is tracked as month / week / day; "New recruits become available at the start of each week" (Day 1) — [MANUAL] pp.10, 36. Dwellings replenish at the beginning of each week ([MANUAL] p.36).
- **The manual never documents week length, weeks-per-month, "Week of the (monster)", "Month of the (monster)", or the Month of the Plague.** The guide doesn't either. (7-day weeks, 4-week months are observable in game; the 7-day figure appears indirectly via the 7-day grace rule, [MANUAL] p.34.)
- Event mechanics from [FH2] `kingdom/week.cpp`, `castle/castle.cpp`, `game_static.cpp` (engine data, not manual):
  - Each week: 75% plain named week, **25% "Week of the (monster)"** → that creature's dwelling gets **+5 extra recruits** in castles that have the dwelling built.
  - Each month (first week of months 2+): 50% plain month, **40% "Month of the (monster)"** → that creature's castle dwelling population **doubles** (+100% of current stock), **10% "Month of the Plague"** → **all castle dwelling populations halved and no growth that week**.
  - Well bonus +2, horde-building bonus +8 (same constants as manual); neutral (unowned) towns grow at 50% rate.

## Explicitly NOT FOUND in the original manual

- Numeric marketplace exchange-rate table (only "never one for one", plateau at 9).
- Ore/mercury/sulfur/crystal/gem mine daily outputs (only gold mine and sawmill are quantified).
- Treasure chest gold/experience amounts; campfire/water wheel/windmill/magic garden amounts.
- Starting resources per difficulty (qualitative statement only).
- Base hero movement points in numbers.
- Week/month event system (Week/Month of a monster, Month of the Plague).
- Captain's Quarters cost (guide only).
Where these gaps are filled above, the substitute source ([GUIDE] or [FH2]) is labeled inline.
