#!/usr/bin/env python3
"""Quantitative economy model for 'The King's Ransom' (36x36, Normal, Blue human Knight).

FINAL TUNED VERSION - matches the guard values shipped in kings_ransom_map.cpp.

Engine facts used (verified at fheroes2 @ b086d1aa, see research_fh2m_and_homm2_design.md):
- Human Normal start: 7500 gold (+20w/5m/20o/5s/5c/5g)  [kingdom.cpp:865-903]
- Income granted at start of each day EXCEPT day 1      [kingdom.cpp:227-230]
- Castle 1000/day, town 250/day, gold mine 1000/day, statue +250/day  [profit.cpp]
- Victory: CURRENT treasury >= 100000 (day 56 playable, defeat day 57)
- Treasure chest E[gold|take-gold] ~ 1400 (31% 2000/32% 1500/32% 1000/5% art)
- Gold pile E=750; campfire E=500g+~5 res; water wheel 500 wk1 then 1000/wk;
  magic garden E=250g+2.5 gems weekly; windmill 2 rare-ish res weekly
- Hero 2500; town->castle 5000+20w+20o; statue 1250+5o; marketplace 500+5w
- Endless Purse of Gold: +500 gold/day while held
- Marketplace sell: rare 50/74/100 gold per unit at 1/2/3 marketplaces

Guard-implied acquisition days (my design estimates, cross-checked against Knight
week-by-week army growth; real games add friction from travel, RNG and Green's AI
contesting the vale - expect ~+3-5 days on the frictionless numbers below):
- western gold mine (35 dwarves):   day ~11-13 good play, ~17 mediocre
- King's Mine (50 nomads):          day ~15-18 committed play
- Marketstead (35p/12a/15pk/8sw/3cv): day ~17-19
- Green's castle (AI garrison):     day ~34-38 with a dedicated push
"""

DAYS = 56


def run(name, incomes, oneoffs):
    """incomes: (day_acquired, gold_per_day, label) - income starts the day AFTER acquisition.
    oneoffs: (day, delta_gold, label)."""
    gold = 7500.0
    win_day = None
    for day in range(1, DAYS + 1):
        if day > 1:
            for d0, per_day, _ in incomes:
                if day > d0:
                    gold += per_day
        for d, delta, _ in oneoffs:
            if d == day:
                gold += delta
        if win_day is None and gold >= 100000:
            win_day = day
    print(f"{name:55s} day56={gold:>9,.0f}  {'WIN day ' + str(win_day) if win_day else 'LOSS'}")
    return gold, win_day


print("Frictionless model (real play: add ~3-5 days of travel/AI/RNG friction)\n")

run("Passive (end turns, build nothing)", [(0, 1000, "Highmarch")], [])

run("Lazy (statue + water wheel, no expansion)",
    [(0, 1000, ""), (3, 250, ""), (0, 145, "weekly objects")],
    [(3, -1250, ""), (10, 2800, "free chests"), (12, 1500, "free piles")])

run("A. Conservative west (good play)",
    [(0, 1000, "Highmarch"), (3, 250, "statue"), (11, 1000, "western gold mine d11"),
     (18, 250, "Marketstead d18"), (24, 750, "castle upgrade d24"), (27, 250, "statue 2"),
     (0, 170, "weekly objects")],
    [(3, -1250, "statue"), (4, -500, "marketplace"), (5, -2500, "2nd hero"), (6, -2500, "wk1 troops"),
     (8, 2800, "home chests"), (10, 1500, "piles"), (13, 2800, "west chests+pile"),
     (14, -3000, "wk2 troops"), (19, 1400, "vale chest"), (22, -2500, "reinforce"),
     (24, -5000, "castle upgrade"), (27, -1250, "statue 2"), (32, -3000, "troops"),
     (45, 2500, "late liquidation")])

run("B. Capital investment - vale rush (excellent)",
    [(0, 1000, ""), (4, 250, "statue"), (12, 250, "Marketstead d12"), (16, 1000, "King's Mine d16"),
     (19, 750, "castle upgrade d19"), (20, 60, "gems via trading post"), (24, 1000, "western GM d24"),
     (26, 250, "statue 2"), (0, 170, "weekly objects")],
    [(2, -2500, "2nd hero"), (3, -3500, "heavy wk1 troops"), (4, -1250, "statue"), (5, -500, "marketplace"),
     (8, 2800, "home chests"), (9, 1500, "piles"), (11, -3500, "wk2 troops"), (14, 1400, "vale chest"),
     (15, 750, "vale gold pile"), (19, -5000, "castle upgrade"), (21, -3000, "losses"),
     (26, -1250, "statue 2"), (28, 1400, "north chest"), (30, -3500, "troops vs Green"),
     (40, 3500, "late liquidation")])

run("C. Aggression vs Green (excellent, risky)",
    [(0, 1000, ""), (4, 250, "statue"), (13, 250, "Marketstead"), (17, 1000, "King's Mine"),
     (20, 750, "castle upgrade"), (26, 1000, "Green's gold mine"), (36, 1000, "Ironvale d36"),
     (0, 170, "weekly objects")],
    [(2, -2500, "2nd hero"), (3, -3500, "heavy wk1 troops"), (4, -1250, "statue"), (5, -500, "marketplace"),
     (8, 2800, "home chests"), (10, 1500, "piles"), (13, 1400, "vale chest"), (15, -4500, "army"),
     (20, -5000, "castle upgrade"), (24, -4000, "army for push"), (30, -3500, "losses vs Green"),
     (34, -3000, "assault troops"), (37, 2000, "Ironvale plunder"), (45, 3000, "late liquidation")])

run("Mediocre (one mine d17, town d28, castle d34)",
    [(0, 1000, ""), (6, 250, "statue"), (17, 1000, "western GM"), (28, 250, "town"),
     (34, 750, "castle upgrade"), (0, 140, "weekly objects")],
    [(5, -2500, "2nd hero"), (6, -1250, "statue"), (8, -2500, "troops"), (12, 2800, "chests"),
     (16, 1500, "piles"), (18, -2500, "troops"), (28, 1400, "chest"), (33, -2000, "troops"),
     (34, -5000, "castle upgrade"), (48, 2000, "late liquidation")])

run("Mediocre + late desert push (Purse day 40)",
    [(0, 1000, ""), (6, 250, ""), (17, 1000, ""), (28, 250, ""), (40, 500, "Endless Purse"),
     (0, 140, "")],
    [(5, -2500, ""), (6, -1250, ""), (8, -2500, ""), (12, 2800, ""), (16, 1500, ""), (18, -2500, ""),
     (28, 1400, ""), (33, -2000, ""), (40, 4300, "desert hoard"), (42, 3000, "sphinx"), (48, 2000, "")])

run("Bad (near-passive + one late mine, heavy losses)",
    [(0, 1000, ""), (25, 1000, ""), (0, 100, "")],
    [(6, -2500, ""), (9, -3000, ""), (15, 1400, ""), (20, -3000, ""), (30, 1400, ""), (40, -2000, "")])

print("""
Desert route add-on (any strategy, ~week 4-6):
  sphinx riddle:                +3000
  hoard (2 chests + 2 piles):  ~+4300
  campfires x2:                ~+1000 (+resources)
  Endless Purse from day d:     +(56-d)*500  (day 36 -> +10000)
  cost: desert movement (200% penalty), 50 nomads + 35 rogues fights

Design targets vs model (frictionless -> expected real):
  excellent play:  win day 35-38 -> ~day 40-44   [on target]
  good play:       win day 41    -> ~day 45-48   [on target]
  mediocre play:   win day 44-48 -> ~day 50-56   [on target]
  passive/bad:     LOSS 62k-91k                  [on target]
""")
