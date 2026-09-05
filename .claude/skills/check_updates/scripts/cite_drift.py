#!/usr/bin/env python3
"""cite_drift.py - classify the `file.cpp:N[-M]` citations in the research docs against an
engine diff, so that a pin move re-verifies only what actually changed.

For every explicit citation (`basename.cpp:123`, `dir/basename.h:10-20`, `serialize.h:194-203, 344-348`) in the given docs
(default: research/notes/*.md and the ledger) the script resolves the basename to a path in
the candidate tree, reads the -U0 diff of that file between pin and candidate, and reports:

  OK            file unchanged, or the cited lines lie before every hunk
  SHIFTED       a hunk above the cited lines changed their numbers; new numbers computed
  NEEDS-REVIEW  a hunk touches the cited lines: read the new code, fix fact and numbers
  REMOVED       the file no longer exists at the candidate
  AMBIGUOUS     several files share the basename; cite with a directory to disambiguate

Shorthand references the script cannot resolve (`cpp:293-303`, `h:59-60`, "lines 21-36")
are counted per doc as MANUAL so you know how much hand-checking each doc needs.

--apply rewrites SHIFTED citations in place (exact string replacement, CRLF preserved) and
prints every change. NEEDS-REVIEW is never edited by the script. Dry run by default.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parents[1]
REPO = SKILL_DIR.parents[2]

CITATION_RE = re.compile(r"(?<![A-Za-z0-9_./-])([A-Za-z0-9_][A-Za-z0-9_./-]*?\.(?:cpp|h)):(\d+(?:-\d+)?(?:,\s*\d+(?:-\d+)?)*)(?![0-9])")
RANGE_RE = re.compile(r"(\d+)(?:-(\d+))?")
SHORTHAND_RE = re.compile(r"(?<![A-Za-z0-9_.])(?:cpp|h):\d+(?:-\d+)?")
LINES_RE = re.compile(r"\blines? \d+(?:-\d+)?", re.IGNORECASE)
HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


def git(clone: Path, *args: str, check: bool = True) -> str:
    proc = subprocess.run(["git", "-C", str(clone), *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace")
    if check and proc.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} failed: {proc.stderr.strip()}")
    return proc.stdout


def clone_path() -> Path:
    env = os.environ.get("FHEROES2_ROOT")
    if env:
        return Path(env)
    sys.path.insert(0, str(REPO / "mapgen"))
    import gen_vcxproj

    return Path(gen_vcxproj.FHEROES2_ROOT)


def pin_from_claude_md() -> str | None:
    m = re.search(r"validated at commit `([0-9a-f]{40})`", (REPO / "CLAUDE.md").read_text(encoding="utf-8", errors="replace"))
    return m.group(1) if m else None


def tree_index(clone: Path, rev: str) -> dict[str, list[str]]:
    idx: dict[str, list[str]] = defaultdict(list)
    for path in git(clone, "ls-tree", "-r", "--name-only", rev).splitlines():
        if path.endswith((".cpp", ".h")):
            idx[path.rsplit("/", 1)[-1]].append(path)
    return idx


def resolve(cited: str, idx: dict[str, list[str]]) -> tuple[str | None, str]:
    """Return (path, status) where status is 'ok', 'missing' or 'ambiguous'."""
    base = cited.rsplit("/", 1)[-1]
    paths = idx.get(base, [])
    if "/" in cited:
        paths = [p for p in paths if p.endswith("/" + cited) or p == cited]
    if len(paths) == 1:
        return paths[0], "ok"
    if not paths:
        return None, "missing"
    return None, "ambiguous"


def hunks(clone: Path, pin: str, cand: str, path: str) -> list[tuple[int, int, int, int]]:
    out = []
    for line in git(clone, "diff", "-U0", pin, cand, "--", path).splitlines():
        m = HUNK_RE.match(line)
        if m:
            a, b, c, d = (int(m.group(1)), int(m.group(2) or 1), int(m.group(3)), int(m.group(4) or 1))
            out.append((a, b, c, d))
    return sorted(out)


def classify(start: int, end: int, hs: list[tuple[int, int, int, int]]) -> tuple[str, int]:
    delta = 0
    for a, b, c, d in hs:
        if b == 0:  # pure insertion of d lines after old line a
            if a < start:
                delta += d
            elif a < end:
                return "NEEDS-REVIEW", 0
        else:
            old_end = a + b - 1
            if old_end < start:
                delta += d - b
            elif a > end:
                continue
            else:
                return "NEEDS-REVIEW", 0
    return ("SHIFTED" if delta else "OK"), delta


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pin", help="baseline commit (default: the pin recorded in CLAUDE.md)")
    ap.add_argument("--candidate", default="origin/master", help="new commit (default origin/master)")
    ap.add_argument("--apply", action="store_true", help="rewrite SHIFTED citations in place")
    ap.add_argument("--verbose", action="store_true", help="also list OK citations")
    ap.add_argument("files", nargs="*", help="docs to scan (default: research/notes/*.md and the ledger)")
    args = ap.parse_args()

    clone = clone_path()
    pin = git(clone, "rev-parse", "--verify", (args.pin or pin_from_claude_md() or "") + "^{commit}").strip()
    cand = git(clone, "rev-parse", "--verify", args.candidate + "^{commit}").strip()
    files = [Path(f) for f in args.files] or sorted((REPO / "research" / "notes").glob("*.md")) + [REPO / "research_fh2m_and_homm2_design.md"]

    idx_cand = tree_index(clone, cand)
    changed = set(git(clone, "diff", "--name-only", pin, cand).splitlines())
    hunk_cache: dict[str, list[tuple[int, int, int, int]]] = {}

    totals: Counter = Counter()
    per_doc: dict[str, Counter] = {}
    rows: list[tuple[str, int, str, str, str]] = []
    edits = 0

    for doc in files:
        raw = doc.read_text(encoding="utf-8", errors="replace")
        lines = raw.splitlines(keepends=True)
        counts: Counter = Counter()
        new_lines = []
        for lineno, line in enumerate(lines, 1):
            counts["MANUAL"] += len(SHORTHAND_RE.findall(line)) + len(LINES_RE.findall(line))
            replaced = line
            for m in reversed(list(CITATION_RE.finditer(line))):
                cited, ranges = m.group(1), m.group(2)  # ranges may be "12", "12-20" or "12-20, 44-48"
                path, res = resolve(cited, idx_cand)
                new_text = ""
                if res == "missing":
                    status = "REMOVED"
                elif res == "ambiguous":
                    status = "AMBIGUOUS"
                elif path not in changed:
                    status = "OK"
                else:
                    if path not in hunk_cache:
                        hunk_cache[path] = hunks(clone, pin, cand, path)
                    hs = hunk_cache[path]
                    statuses: list[str] = []

                    def shift(rm: re.Match) -> str:
                        s, e = int(rm.group(1)), int(rm.group(2) or rm.group(1))
                        st, delta = classify(s, e, hs)
                        statuses.append(st)
                        if st != "SHIFTED":
                            return rm.group(0)
                        return f"{s + delta}-{e + delta}" if rm.group(2) else f"{s + delta}"

                    new_ranges = RANGE_RE.sub(shift, ranges)  # separators are kept as written
                    if "NEEDS-REVIEW" in statuses:
                        status = "NEEDS-REVIEW"
                    elif "SHIFTED" in statuses:
                        status = "SHIFTED"
                        new_text = f"{cited}:{new_ranges}"
                    else:
                        status = "OK"
                counts[status] += 1
                if status != "OK" or args.verbose:
                    rows.append((doc.name, lineno, m.group(0), status, new_text))
                if status == "SHIFTED" and args.apply:
                    replaced = replaced[: m.start()] + new_text + replaced[m.end():]
                    edits += 1
            new_lines.append(replaced)
        per_doc[doc.name] = counts
        totals.update(counts)
        if args.apply and "".join(new_lines) != raw:
            doc.write_text("".join(new_lines), encoding="utf-8", newline="")

    print(f"pin {pin[:9]} -> candidate {cand[:9]}; {len(changed)} files changed in the clone\n")
    print("| doc | OK | SHIFTED | NEEDS-REVIEW | REMOVED | AMBIGUOUS | MANUAL |")
    print("|---|---|---|---|---|---|---|")
    for name, c in per_doc.items():
        print(f"| {name} | {c['OK']} | {c['SHIFTED']} | {c['NEEDS-REVIEW']} | {c['REMOVED']} | {c['AMBIGUOUS']} | {c['MANUAL']} |")
    print(f"| **total** | {totals['OK']} | {totals['SHIFTED']} | {totals['NEEDS-REVIEW']} | {totals['REMOVED']} | {totals['AMBIGUOUS']} | {totals['MANUAL']} |\n")
    if rows:
        print("| doc | line | citation | status | new |")
        print("|---|---|---|---|---|")
        for name, lineno, cit, status, new in rows:
            print(f"| {name} | {lineno} | `{cit}` | {status} | {('`' + new + '`') if new else ''} |")
        print()
    if args.apply:
        print(f"applied {edits} SHIFTED rewrites" + (" (files written)" if edits else ""))
    else:
        print("dry run; pass --apply to rewrite SHIFTED citations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
