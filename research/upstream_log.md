# Upstream check log

One row per `check_updates` run, newest first; keep at most 30 rows (drop the oldest). Note header
stamps in `research/notes/*` name the commit at which each note was last verified; a row here records
that a later commit was checked and what it affected.

| date | pin -> candidate | commits | src buckets touched | release / installed | validation | action |
|---|---|---|---|---|---|---|
| 2026-09-05 | b086d1aa -> d778cb44 | +7 | irrelevant only (campaign x3); 30 non-src (CI, docs, translations) | 1.1.17 / 1.1.17 (v13, format diff 0) | build OK, 0 warnings; kings_ransom + ashen_succession SHA-256 identical; gameload OK | pin advanced; check_updates skill rewritten; homm2-map-maker skill harvested |
