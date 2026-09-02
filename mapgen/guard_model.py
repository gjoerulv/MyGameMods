#!/usr/bin/env python3
"""Guard calibration model for The Ashen Succession.

Input: the output of `mapgen strength ashen_succession` (engine unit strengths, every placed
monster stack, every castle garrison). The model builds reference armies per race at a few
checkpoint days from the engine's own weekly growth numbers and compares each guard to them.

Usage:
    mapgen\\build\\x64\\Release\\mapgen.exe strength ashen_succession > strength.txt
    python mapgen/guard_model.py strength.txt

Reference army assumptions (deliberately simple, stated so they can be argued with):
  * The castle starts with DW1 + DW2 (as authored). DW3 is built day 3, DW4 day 6, DW5 day 12,
    DW6 day 24 - a fast but realistic build on a rich map.
  * A dwelling has one week of growth in stock when built and grows once per week from day 8
    on (the engine skips week 1). Well / "Week of" bonuses are ignored (conservative).
  * Everything grown is recruited (gold is not the binding constraint on a Rich map after
    week 1) and carried by one hero. Hero attack/defence, spells and shooters are ignored, so a
    real human army is worth roughly 1.2-1.5x the printed number; the AI needs 1.5x the guard's
    strength before it attacks (ai_planner_hero.cpp) and treats stronger guards as walls.
  * Wandering stacks grow by count/7 each week from week 2 (maps_tiles_helper.cpp), compounding.
  * Neutral town/castle garrisons grow too (castle.cpp Castle::ActionNewWeek / _joinRNDArmy):
    from week 2 one random join per week, plus a second with 40% for towns and always for
    castles. A join picks quality q = Rand(1,15) + day/10 and adds the race's BASE creature of
    level 5 (q>15, 1 unit), 4 (q>13, 1-3), 3 (q>10, 3-5), 2 (q>5, 5-7) or 1 (8-15), plus day/20,
    and only lands if that creature already has a stack or a slot is free (all five slots here
    are full, so only matching base creatures grow). Modelled as the expectation.

Bands (guard strength / reference army strength at the intended checkpoint, judged on the
WORST race, i.e. the highest ratio):
  * "week 2" guard : 0.35 - 0.70   (human breaks it week 2, AI in week 3)
  * "week 3" guard : 0.45 - 0.80
  * "month 2" guard: 0.55 - 0.95
"""

import sys
from collections import OrderedDict

RACES = OrderedDict(
    [
        ("Knight", [1, 2, 4, 6, 8, 10]),
        ("Barbarian", [12, 13, 15, 16, 18, 20]),
        ("Sorceress", [21, 22, 24, 26, 28, 29]),
        ("Warlock", [30, 31, 32, 33, 35, 36]),
        ("Wizard", [39, 40, 41, 43, 44, 46]),
        ("Necromancer", [48, 49, 51, 53, 55, 57]),
    ]
)
BUILD_DAY = [0, 0, 3, 6, 12, 24]  # day each dwelling exists
CHECKPOINTS = OrderedDict([("week 2", 10), ("week 3", 17), ("month 2", 31), ("month 2 late", 45)])

# Which checkpoint each guard label is designed for (first substring match wins).
GUARD_TIER = [
    ("seam artifact guard", "week 3"),
    ("home gold mine guard", "week 1"),  # days 4-7, checked against the week-1 reference
    ("gems mine guard", "week 1"),
    ("chest guard", "week 1"),
    ("artifact guard", "week 2"),
    ("pass A guard", "week 2"),
    ("pass B guard", "week 2"),
    ("sulfur mine guard", "week 2"),
    ("crystal mine guard", "week 2"),
    ("cache guard", "week 2"),  # the half chest + gold pile, a week-2 grab
    ("treasure guard", "week 3"),
    ("seam gold mine guard", "week 3"),
    ("alchemist lab guard", "week 3"),
    ("gate guard", "month 2"),
    ("cache gold mine guard", "month 2"),
    ("trading post guard", "pocket"),
]
CHECKPOINTS["week 1"] = 5
GARRISON = {
    # name: (tier, race base creature ids DW1..DW5, is castle)
    "Ravensgate": ("month 2", [12, 13, 15, 16, 18], False),
    "Ashford": ("month 2", [12, 13, 15, 16, 18], False),
    "Greyfen": ("month 2", [48, 49, 51, 53, 55], False),
    "Mirefall": ("month 2", [48, 49, 51, 53, 55], False),
    "Kingsfall": ("month 2 late", [1, 2, 4, 6, 8], True),
}
BANDS = {"week 1": (0.30, 0.90), "week 2": (0.35, 0.70), "week 3": (0.45, 0.80), "month 2": (0.55, 0.95), "month 2 late": (0.55, 1.30), "pocket": (0.0, 0.45)}


def parse(path):
    monsters = {}
    guards = []
    garrisons = []
    section = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("=== "):
                section = line.strip("= ").strip()
                continue
            if not line.strip():
                continue
            parts = line.split("\t")
            if section == "monsters":
                mid, name, strength, grown, gold = parts
                monsters[int(mid)] = {"name": name, "strength": float(strength), "grown": int(grown), "gold": int(gold)}
            elif section == "guards":
                label, pos, mid, name, count, strength = parts
                guards.append({"label": label, "pos": pos, "id": int(mid), "name": name, "count": int(count), "strength": float(strength)})
            elif section == "garrisons":
                name, strength, composition = parts
                comp = {}
                for item in composition.strip(",").split(","):
                    if item:
                        mid, cnt = item.split(":")
                        comp[int(mid)] = int(cnt)
                garrisons.append({"name": name, "strength": float(strength), "composition": comp})
    return monsters, guards, garrisons


def weeks_grown(build_day, day):
    """Number of weekly growths a dwelling built on build_day has received by `day` (week starts: 8, 15, 22...)."""
    return sum(1 for w in range(8, day + 1, 7) if w > build_day)


def reference_army(monsters, race_ids, day):
    total = 0.0
    for k, mid in enumerate(race_ids):
        if BUILD_DAY[k] > day:
            continue
        count = monsters[mid]["grown"] * (1 + weeks_grown(BUILD_DAY[k], day))
        total += count * monsters[mid]["strength"]
    return total


def guard_growth_factor(day):
    weeks = max(0, (day - 1) // 7)  # growth happens at the start of weeks 2, 3, ...
    return (1 + 1 / 7) ** weeks


def garrison_growth(monsters, composition, base_ids, is_castle, day):
    """Expected strength added by neutral-garrison joins up to `day`."""
    added = 0.0
    for week_start in range(8, day + 1, 7):
        time_mod = week_start // 10
        joins = 2.0 if is_castle else 1.4
        expected_join = 0.0
        for q0 in range(1, 16):
            q = q0 + time_mod
            if q > 15:
                level, cnt = 4, 1
            elif q > 13:
                level, cnt = 3, 2
            elif q > 10:
                level, cnt = 2, 4
            elif q > 5:
                level, cnt = 1, 6
            else:
                level, cnt = 0, 11.5
            cnt += time_mod / 2
            mid = base_ids[level]
            if mid in composition:  # a matching stack exists, so the join lands
                expected_join += cnt * monsters[mid]["strength"] / 15.0
        added += joins * expected_join
    return added


def tier_for(label):
    for key, tier in GUARD_TIER:
        if key in label:
            return tier
    return None


def strip_prefix(label):
    for prefix in ("center NW ", "center NE ", "center SW ", "center SE ", "N-west ", "N-east ", "S-west ", "S-east ", "W-north ", "W-south ", "E-north ",
                   "E-south ", "NW ", "NE ", "SW ", "SE ", "N ", "S ", "W ", "E "):
        if label.startswith(prefix):
            return label[len(prefix):]
    return label


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    monsters, guards, garrisons = parse(sys.argv[1])

    print("Reference armies (engine strength units):")
    ref = {}
    for race, ids in RACES.items():
        ref[race] = {cp: reference_army(monsters, ids, day) for cp, day in CHECKPOINTS.items()}
        print(f"  {race:12s} " + "  ".join(f"{cp}: {ref[race][cp]:7.0f}" for cp in CHECKPOINTS))
    print()

    def report(name, strength, tier, grown):
        if tier is None:
            print(f"  ??? {name:44s} {strength:8.0f}   (no tier)")
            return
        lo, hi = BANDS[tier]
        ratios = {race: grown / ref[race][tier] for race in RACES}
        worst = max(ratios.values())
        best = min(ratios.values())
        flag = "HIGH" if worst > hi else ("LOW " if best < lo else "OK  ")
        print(f"  {flag} {name:44s} {strength:7.0f} ->{grown:7.0f} @{tier:12s} " + " ".join(f"{race[:4]}={r:.2f}" for race, r in ratios.items()))

    print("Guards (base strength -> with neutral growth at the checkpoint; ratio guard/reference per race; flag on the worst race):")
    seen = set()
    for g in guards:
        key = (strip_prefix(g["label"]), g["id"], g["count"])
        if key in seen:
            continue
        seen.add(key)
        tier = tier_for(g["label"])
        grown = g["strength"] * (guard_growth_factor(CHECKPOINTS[tier]) if tier else 1.0)
        report(f"{key[0]} ({g['count']} {g['name']})", g["strength"], tier, grown)
    print()

    print("Garrisons (with expected neutral-town joins up to the checkpoint):")
    for ga in garrisons:
        info = GARRISON.get(ga["name"])
        if info is None:
            report(ga["name"], ga["strength"], None, ga["strength"])
            continue
        tier, base_ids, is_castle = info
        grown = ga["strength"] + garrison_growth(monsters, ga["composition"], base_ids, is_castle, CHECKPOINTS[tier])
        report(ga["name"], ga["strength"], tier, grown)
    print()
    print("Bands: " + ", ".join(f"{k} {v[0]:.2f}-{v[1]:.2f}" for k, v in BANDS.items()))
    print("AI attacks a neutral only above 1.5x its strength (castles: garrison x1.25 tower/melee factor x1.5).")


if __name__ == "__main__":
    main()
