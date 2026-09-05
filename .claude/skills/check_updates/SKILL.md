---
name: check_updates
description: Keep this repo's fheroes2 knowledge current. Checks whether upstream fheroes2 master (github.com/ihhub/fheroes2) has moved past the pinned engine commit, triages what changed for map making, re-validates mapgen and every registered map at the new commit, fast-forwards the clone and re-pins, then syncs CLAUDE.md, the research ledger, research/notes and the homm2-map-maker skill; it also harvests lessons from recently finished maps into those docs. Use it whenever the user asks to check for updates, sync or refresh the research, bump or re-pin the engine commit, update CLAUDE.md or the notes, asks whether fheroes2 or the docs are up to date, mentions a new fheroes2 release or a newly installed game version, or wants what was learned on a finished map captured for future agents, even when they do not say check_updates.
---

# check_updates: keep the engine knowledge current

The docs in this repo (`CLAUDE.md`, `research_fh2m_and_homm2_design.md`, `research/notes/*`, the `homm2-map-maker` skill) exist so that an agent making a map never re-derives the engine. They rot in two ways: upstream fheroes2 moves (facts and `file:line` citations drift, mapgen may stop compiling or emit different bytes), and maps get built here whose hard-won lessons stay in session transcripts. This skill fixes both in one pass, without bloating anything.

## Principles

- **The engine source is the specification.** When a doc and the clone disagree, the clone at the pinned commit wins; fix the doc.
- **Determinism is the regression test.** Every registered map must regenerate byte-identically (its recorded SHA-256) at the new commit; a different hash means the engine changed something the maps depend on. This check always runs before the pin moves. No watch list replaces it.
- **Keep facts, cut prose.** Update the sentence that exists instead of appending a new one. A note's stamp advances only when something in that note was actually re-verified.
- **Delegate noise.** Builds and long command output go through the `verbose-runner` agent, long code reads through `Explore`, so the main context keeps conclusions, not logs. Without those agent types, redirect output to a file in the scratchpad and grep it.
- **Transactional clone.** The clone's `master` moves only after everything is green; on failure the checkout returns to the pin, so tool and docs never disagree.

## Inputs (read them, never hardcode them)

| Fact | Source of truth |
|---|---|
| Pinned commit, installed release and exe path, format version, recorded hashes | `CLAUDE.md` ("External dependency" and "Architecture") |
| Clone path | env `FHEROES2_ROOT`, else the default in `mapgen/gen_vcxproj.py` |
| Registered maps and seeds | `mapgen/src/map_registry.cpp` |
| Per-map recorded SHA-256 and validation commit | `<map>_validation.md`, first lines |
| Previous checks | `research/upstream_log.md` (newest first) |
| Path to bucket to notes/tools | `references/watch_list.md` |

`scripts/upstream_report.py` reads all of these and cross-checks them. If it reports an inconsistency (CLAUDE.md and a validation doc disagree on a hash, a repo or installed map copy differs from its recorded hash), fix that first.

## Phase A: detect

```bash
python .claude/skills/check_updates/scripts/upstream_report.py --fetch
```

`--fetch` is the only network action (`git fetch --tags origin` on the clone; remote-tracking refs and tags only). Without it the script reports against the previous fetch and says how old it is, fine for a glance, never for a verdict. Add `--json` to parse the result, `--candidate <sha>` to test a specific commit.

The report contains: clone HEAD vs pin (a mismatch or a dirty tree stops everything, someone moved the clone by hand, ask before continuing); candidate sha, ahead count, commit subjects; changed files by bucket, UNCATEGORIZED `src/` paths listed loudly; newest release tag vs the installed release and the installed exe's FileVersion; the `map_format_info.h/.cpp` diff between the installed release tag and the candidate, plus `currentSupportedVersion` at both ends; every registered map with its recorded hash and the actual hashes of the repo copy and the `%APPDATA%\fheroes2\maps\` copy.

Exit codes: **0** up to date, **1** changes but none in a research bucket, **2** changes in a research bucket or UNCATEGORIZED, **3** format gate (serializer or format version differs from the installed release), **4** environment error (fetch failed, inputs missing): fix, never interpret.

## Decision gate

| Exit | Do |
|---|---|
| 0 | Append a log line (Phase D), then Phase E. |
| 1 | Phase C, then D and E. Advance the pin even for translation/CI-only changes: the rebuild is delegated and cheap, and small steps keep the next triage small. Hold only if the user says so. |
| 2 | Phase B, then C, D, E. |
| 3 | Stop and read `git -C <clone> diff <installed-tag> <candidate> -- src/fheroes2/maps/map_format_info.h src/fheroes2/maps/map_format_info.cpp` yourself. A refactor that leaves bytes and version alone is exit-2 work. A real serializer or version change means maps generated at the candidate could not load in the installed game. Find the last compatible commit (`git log --format=%H <installed-tag>..<candidate> -- <those two files>`, take the parent of the oldest listed) and ask the user whether to hold the pin there or update the installed game first. Do nothing else until they answer. |

## Phase B: triage and re-research (research buckets only)

Work bucket by bucket from the report; `references/watch_list.md` says which notes and tools each path feeds.

1. **Classify citations mechanically first.**
   ```bash
   python .claude/skills/check_updates/scripts/cite_drift.py --pin <pin> --candidate <sha>
   ```
   Every explicit `basename.cpp:N[-M]` citation in the notes and the ledger comes back as OK (file unchanged, or cited lines above every hunk), SHIFTED (a hunk above moved them; new numbers computed), NEEDS-REVIEW (a hunk touches the cited lines), REMOVED or AMBIGUOUS. Shorthand such as `cpp:293-303` or "lines 21-36" is counted as MANUAL per doc. Re-run with `--apply` to rewrite SHIFTED numbers only; it prints every change. NEEDS-REVIEW and MANUAL stay yours.
2. **Re-verify NEEDS-REVIEW claims** by reading the new code (delegate long reads to `Explore` with the exact question). Fix the fact and the numbers in place. Keep a fact that still holds, delete one the engine no longer supports, never leave both.
3. **Look for new facts, not only broken ones.** Scan the commit subjects for editor, map, object, victory/loss, AI, RMG or auto-playtest work. Anything a map author would want to know goes into the relevant note next to its topic (never appended at the end) with a `file:line` citation at the candidate; if it changes a recipe or a number every map uses, also into the ledger and `CLAUDE.md`.
4. **Mirrored engine logic in mapgen.** `grep -n "fheroes2 @" mapgen/src/*.cpp` lists where mapgen replicates an engine sequence (e.g. `gameload` mirrors `game_scenarioinfo.cpp:LoadNewMap` / `game_auto_playtest.cpp:prepareMap`). If the cited engine file changed, re-read both sides, fix mapgen if needed, and refresh that comment's stamp (comments are not serialized into maps; string literals are).
5. **Model constants.** When `monster_info`, `buildinginfo`, `profit`, `castle.cpp` (growth and joins), `resource_trading` or `difficulty` changed, re-check the numbers hardcoded in `mapgen/guard_model.py` and `mapgen/economy_model.py` and the tables in note 05 and ledger §7.
6. **Stamps.** Advance a note's header stamp (`@ <short-sha>, <date>`) only if you changed a citation or a fact in it. Untouched notes keep their stamp; the upstream log records that a check covered them.

**New release** (the report shows a newer numeric tag than the installed release): fetch its notes from `https://github.com/ihhub/fheroes2/releases/tag/<tag>`, add editor/map-relevant items to `research/notes/11_community_wisdom.md` §4 as a new subsection, and tell the user to update the game. The installed-version rows in `CLAUDE.md` and the ledger change only after the user has installed it (re-run the script; it reads the exe version). Notes 09-11 are otherwise never re-researched by this skill unless the user asks.

## Phase C: re-validate the toolchain at the candidate

Preconditions: `git -C <clone> status --porcelain` empty and HEAD equal to the pin. Then:

1. `git -C <clone> checkout --detach <sha>` (master stays at the pin until the end).
2. `python mapgen/gen_vcxproj.py`. It reads the clone's `sources.props`, so upstream file adds and removes are picked up automatically; a non-zero exit is its own canary for a renamed `main()` file. A missing-header build error usually means the hand-maintained `INCLUDE_DIRS` list in that script needs a new directory, not that mapgen is broken.
3. Build via `verbose-runner` with the literal command from `CLAUDE.md`:
   ```
   "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" mapgen/mapgen.vcxproj -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo
   ```
4. For every registered map: `mapgen/build/x64/Release/mapgen.exe generate <name> <scratchpad>/<name>.fh2m`; compare its SHA-256 with the recorded one; check the output for `round-trip byte comparison: IDENTICAL`, `all action objects reachable` and an unchanged guard-sealed list. Then `mapgen.exe gameload <file>` and compare the printed condition bits and per-kingdom counts with the map's validation doc. Run these through `verbose-runner` as well (the transcripts are long); ask it for the hash and the verdict lines.
5. **Green** (clean build, every hash identical, every gameload OK): `git -C <clone> checkout master` then `git -C <clone> merge --ff-only <sha>`. Continue with Phase D.
6. **Red**, in order of likelihood:
   - Compile or link error naming a `Maps::`, `World` or `Settings` symbol: API drift. Make the smallest mapgen change that restores the same behaviour, note it for the report, rebuild.
   - Link error for SDL/zlib or a missing `packages\` file: upstream bumped its prebuilt deps (`script/windows/install_packages.bat` changed). Re-run that script inside the clone (it only writes the ignored `VisualStudio/packages/`), rebuild.
   - Hash differs while round-trip is IDENTICAL and gameload OK: engine behaviour changed (RNG consumption order, terrain transitions, object tables, road/stream indices, default metadata). Diff `inspect` output and the ASCII grids old vs new to name the cause. Accepting a new hash is the user's call: explain what changed in the map; only with consent regenerate to the repo root and `%APPDATA%\fheroes2\maps\` and update the hash in `CLAUDE.md` and `<map>_validation.md` (state the new commit next to it).
   - Round-trip DIFFERENT or gameload failure: serializer or loader change; treat it as the format gate.
   - Anything unresolved: `git -C <clone> checkout master`, rebuild at the pin so the exe matches the documented commit, and report exactly what blocked the update.

## Phase D: sync the documents

Live pins, all updated to the new commit and date:

- `CLAUDE.md`: the "validated at commit" line; the installed-game line (release, exe path, format version). Stay within the budget below.
- `.claude/skills/homm2-map-maker/SKILL.md`: the engine-clone row of its toolchain table.
- `research_fh2m_and_homm2_design.md` §1: commit, date, installed release with `git rev-list --count <installed-tag>..<sha>` as "commits behind HEAD", the compatibility sentence (re-run the `map_format_info` diff and quote its size), and one sentence saying note header stamps are last-verified commits, later checks being listed in `research/upstream_log.md`.
- `research/upstream_log.md` (create if missing; newest entry first; at most 30 entries, drop the oldest): one table row `| date | pin -> candidate | +n commits | src buckets touched | release / installed | validation verdict | action |`.
- Each `<map>_validation.md`: one appended line, `Re-validated at <sha> (<date>): SHA-256 identical.` (or the accepted-new-hash wording). Nothing else in those files changes.
- Notes touched in Phase B: header stamp (B.6).
- Auto-memory (the directory named in the system prompt, if any): refresh the pinned commit and hash pointers inside the existing memory files and keep `MEMORY.md` at one line per file. Do not create memory files for facts the repo now records.

**Never touch:** the `creatorNotes` and every other string literal in `mapgen/src/*_map.cpp` (they are serialized into the map, so the recorded hash would change); `<map>_design.md`; a recorded hash without a consented regeneration; the stamps of notes you did not verify; any file inside the clone.

## Phase E: harvest lessons for future agents (every run)

Upstream is not the only source of drift: each map built here teaches engine rules and adds builder features the guiding docs may not mention yet. Sweep, then route.

Sweep, mechanical first:
1. `git log --since=<date of the last log entry> --stat` in this repo: new `*_map.cpp`, changes to `mapgen.h` / `mapgen_main.cpp`, new docs.
2. Public members of `MapBuilder` in `mapgen/src/mapgen.h` versus the map-maker skill's toolchain table and its "Key MapBuilder calls" paragraph. Every flag or method a map author must know (e.g. `strictPlacement`, `tryPlace`, `routeRoad`, `computeReachability`) gets one line there.
3. The `CLAUDE.md` commands block versus the map-maker skill's command block: same modes (`generate`, `gameload`, `inspect`, `strength`), same helper scripts.
4. Every `<map>_validation.md` and `<map>_design.md` newer than the last check: their "defects caught", "known limitations", lesson and gotcha passages.
5. Auto-memory files, as read-only input: any engine rule or workflow lesson there that no repo doc states.

Route each lesson (grep a distinctive phrase first; if it is already recorded, improve that sentence instead of adding one):
- `CLAUDE.md`: only rules and commands that shape every map (a passability or terrain rule, a validation gate), one sentence each.
- `.claude/skills/homm2-map-maker/SKILL.md`: workflow, gotchas, toolchain rows, validation steps. An agent reads this while building a map, so the one-line version of a rule goes here and the proof goes elsewhere.
- `research/notes/NN_*.md`: the engine fact with `file:line` at the current pin, inserted next to its topic.
- Ledger: condensed facts and numbers; grow net-neutral (compress something when you add).
- Never the upstream log (it records checks, not knowledge).

## Budgets (check before finishing)

| File | Cap | Rule |
|---|---|---|
| `CLAUDE.md` | 70 lines | universal rules and commands only |
| `research_fh2m_and_homm2_design.md` | 700 lines | net-neutral growth |
| `.claude/skills/homm2-map-maker/SKILL.md` | 200 lines | one line per gotcha or tool |
| `research/notes/*.md` | no cap | insert near the topic, never append at the end |
| `references/*.md` in this skill | 100 lines | prune rows nobody consults |
| `research/upstream_log.md` | 30 entries | drop the oldest |

Report old to new line counts for every file you edited; that is the bloat meter.

## Final report to the user

```
Pin: <old> (<date>) -> <new> (<date>)            [or: held at <old>, reason]
Upstream: +<n> commits; buckets: <list or none map-relevant>; uncategorized: <paths or none>
Compatibility: newest release <tag>, installed <version>; map_format_info diff <n> lines; format v<N>
Validation: <map>: hash identical | accepted new <hash>; gameload OK       (one line per map)
Docs changed (old->new lines): CLAUDE.md 52->54, ...
Research changes: <note>: <what>, ...                                       [or none]
Lessons harvested: <lesson> -> <doc>, ...                                   [or none]
Open questions: <what needs the user>                                       [or none]
```

## Never do

- Move `master` in the clone before build, hashes and gameload are all green; edit any file inside the clone; run `install_packages.bat` unless a deps change was diagnosed.
- Edit string literals in `mapgen/src/*_map.cpp`, any `<map>_design.md`, or a recorded hash without a consented regeneration.
- Bulk re-stamp notes, or re-cite by hand what `cite_drift.py --apply` does mechanically.
- Re-crawl community or manual sources (notes 09-11) unless asked or a new release appeared.
- Call anything "up to date" from a stale fetch, or file an UNCATEGORIZED path as irrelevant without adding it to the watch list.
- Commit. Leave the changes for the user to review and commit.
