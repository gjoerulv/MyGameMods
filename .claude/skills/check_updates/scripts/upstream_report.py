#!/usr/bin/env python3
"""upstream_report.py - Phase A detector for the check_updates skill.

Read-only apart from --fetch, which runs `git fetch --tags origin` on the fheroes2 clone
(remote-tracking refs and tags only). The script never checks out, merges, builds or edits
a document. Everything it knows is read from the repo, never hardcoded:

  CLAUDE.md                                   pinned commit, installed release/exe, format version, hashes
  mapgen/gen_vcxproj.py                       clone path (env FHEROES2_ROOT overrides)
  mapgen/src/map_registry.cpp                 registered maps
  <map>_validation.md                         per-map recorded SHA-256
  .claude/skills/check_updates/references/watch_list.md   path prefix -> bucket -> notes/tools

Exit codes: 0 up to date | 1 changes, none in a research bucket | 2 changes in a research
bucket or an UNCATEGORIZED src/ path | 3 format gate (serializer or format version differs
from the installed release) | 4 environment error (fetch failed, inputs missing).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parents[1]
REPO = SKILL_DIR.parents[2]
RESEARCH_BUCKETS = {"format", "objects", "terrain", "editor", "scenario", "economy", "ai"}
FORMAT_FILES = ["src/fheroes2/maps/map_format_info.h", "src/fheroes2/maps/map_format_info.cpp"]


def run(cmd: list[str], cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess:
    proc = subprocess.run(cmd, cwd=str(cwd) if cwd else None, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")
    if check and proc.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)}\n{proc.stderr.strip()}")
    return proc


def git(clone: Path, *args: str, check: bool = True) -> str:
    return run(["git", "-C", str(clone), *args], check=check).stdout.strip()


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    return hashlib.sha256(path.read_bytes()).hexdigest()


def version_tuple(tag: str) -> tuple[int, ...]:
    return tuple(int(x) for x in tag.split("."))


# ----------------------------------------------------------------------------- inputs

def clone_path() -> Path:
    env = os.environ.get("FHEROES2_ROOT")
    if env:
        return Path(env)
    sys.path.insert(0, str(REPO / "mapgen"))
    import gen_vcxproj  # only defines constants at import time; main() is guarded

    return Path(gen_vcxproj.FHEROES2_ROOT)


def parse_claude_md(text: str) -> dict:
    pin = re.search(r"validated at commit `([0-9a-f]{40})`", text)
    release = re.search(r"release (\d+\.\d+\.\d+)", text)
    exe = re.search(r"`([A-Za-z]:\\[^`\n]*?fheroes2\.exe)`", text)
    fmt = re.search(r"format v(\d+)", text)
    hashes = dict(re.findall(r"`([A-Za-z0-9_]+)\.fh2m`: sha256 `([0-9a-f]{64})`", text))
    return {
        "pin": pin.group(1) if pin else None,
        "installed_release": release.group(1) if release else None,
        "installed_exe": exe.group(1) if exe else None,
        "format_version": int(fmt.group(1)) if fmt else None,
        "hashes": hashes,
    }


def parse_registry(text: str) -> list[dict]:
    rows = re.findall(r'\{\s*"([A-Za-z0-9_]+)"\s*,\s*"([^"]*)"\s*,\s*(\d+)\s*,\s*(\d+)U?', text)
    return [{"name": n, "title": t, "width": int(w), "seed": int(s)} for n, t, w, s in rows]


def parse_watch_list(text: str) -> list[tuple[str, str, str, str]]:
    rows = []
    for line in text.splitlines():
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 4 or cells[0].lower() == "prefix" or set(cells[0]) <= {"-", ":"}:
            continue
        prefix = cells[0].strip("`").replace("\\", "/")
        rows.append((prefix, cells[1], cells[2], cells[3]))
    return rows


def bucket_for(path: str, rows: list[tuple[str, str, str, str]]) -> tuple[str, str, str, str]:
    best = None
    for prefix, bucket, notes, tools in rows:
        if path.startswith(prefix) and (best is None or len(prefix) > len(best[0])):
            best = (prefix, bucket, notes, tools)
    if best:
        return best
    if path.startswith("src/"):
        return ("", "UNCATEGORIZED", "classify it and add a row to watch_list.md", "")
    return ("", "other", "", "")


def exe_file_version(path: str | None) -> str | None:
    if not path or not Path(path).is_file():
        return None
    ps = f"(Get-Item -LiteralPath '{path}').VersionInfo.FileVersion"
    proc = run(["powershell", "-NoProfile", "-NonInteractive", "-Command", ps], check=False)
    raw = proc.stdout.strip().replace(",", ".").replace(" ", "")
    if not raw:
        return None
    parts = raw.split(".")
    while len(parts) > 3 and parts[-1] == "0":
        parts.pop()
    return ".".join(parts)


# ----------------------------------------------------------------------------- main

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fetch", action="store_true", help="git fetch --tags origin on the clone first (the only network action)")
    ap.add_argument("--json", action="store_true", help="print a JSON object instead of markdown")
    ap.add_argument("--candidate", help="commit to compare against instead of origin/master")
    ap.add_argument("--max-commits", type=int, default=60, help="commit subjects shown in markdown (JSON always has all)")
    args = ap.parse_args()

    out: dict = {"warnings": [], "preconditions_ok": True}
    try:
        claude_md = read(REPO / "CLAUDE.md")
        cfg = parse_claude_md(claude_md)
        clone = clone_path()
        watch_rows = parse_watch_list(read(SKILL_DIR / "references" / "watch_list.md"))
        maps = parse_registry(read(REPO / "mapgen" / "src" / "map_registry.cpp"))
    except Exception as exc:  # noqa: BLE001
        print(f"ENVIRONMENT ERROR: {exc}", file=sys.stderr)
        return 4
    if not cfg["pin"]:
        print("ENVIRONMENT ERROR: no 'validated at commit `<sha>`' line in CLAUDE.md", file=sys.stderr)
        return 4
    if not (clone / ".git").exists():
        print(f"ENVIRONMENT ERROR: clone not found at {clone}", file=sys.stderr)
        return 4
    out.update({"repo": str(REPO), "clone": str(clone), **{k: v for k, v in cfg.items() if k != "hashes"}})

    # --- fetch / freshness
    if args.fetch:
        proc = run(["git", "-C", str(clone), "fetch", "--tags", "origin"], check=False)
        if proc.returncode != 0:
            print("FETCH FAILED - do not trust any verdict from this run:\n" + proc.stderr, file=sys.stderr)
            return 4
        out["fetched"] = True
    else:
        out["fetched"] = False
    git_dir = Path(git(clone, "rev-parse", "--git-dir"))
    if not git_dir.is_absolute():
        git_dir = clone / git_dir
    fetch_head = git_dir / "FETCH_HEAD"
    out["last_fetch_age_hours"] = round((time.time() - fetch_head.stat().st_mtime) / 3600, 1) if fetch_head.exists() else None

    # --- clone state vs pin
    pin = cfg["pin"]
    head = git(clone, "rev-parse", "HEAD")
    dirty = git(clone, "status", "--porcelain")
    branch = git(clone, "branch", "--show-current")
    out.update({"clone_head": head, "clone_branch": branch, "clone_dirty": bool(dirty)})
    if head != pin:
        out["preconditions_ok"] = False
        out["warnings"].append(f"clone HEAD {head[:9]} != pinned {pin[:9]} (someone moved the clone; stop and ask)")
    if dirty:
        out["preconditions_ok"] = False
        out["warnings"].append("clone working tree is dirty (stop and ask)")

    # --- candidate
    candidate = git(clone, "rev-parse", "--verify", (args.candidate or "origin/master") + "^{commit}")
    ahead = int(git(clone, "rev-list", "--count", f"{pin}..{candidate}"))
    behind = int(git(clone, "rev-list", "--count", f"{candidate}..{pin}"))
    if behind:
        out["warnings"].append(f"pin is not an ancestor of the candidate ({behind} commits only in the pin): history rewrite or wrong branch")
    out.update({
        "candidate": candidate,
        "candidate_date": git(clone, "log", "-1", "--format=%cd", "--date=short", candidate),
        "pin_date": git(clone, "log", "-1", "--format=%cd", "--date=short", pin),
        "ahead": ahead,
        "behind": behind,
    })
    commits = []
    if ahead:
        for line in git(clone, "log", "--format=%h%x09%ad%x09%s", "--date=short", f"{pin}..{candidate}").splitlines():
            sha, date, subj = line.split("\t", 2)
            commits.append({"sha": sha, "date": date, "subject": subj})
    out["commits"] = commits

    # --- changed files -> buckets
    changed = []
    if ahead:
        for line in git(clone, "diff", "--name-status", pin, candidate).splitlines():
            parts = line.split("\t")
            status, path = parts[0], parts[-1]
            prefix, bucket, notes, tools = bucket_for(path, watch_rows)
            changed.append({"status": status, "path": path, "bucket": bucket, "notes": notes, "tools": tools})
    out["changed_files"] = changed
    buckets: dict[str, list[str]] = {}
    for c in changed:
        buckets.setdefault(c["bucket"], []).append(c["path"])
    out["buckets"] = buckets
    research_hit = any(b in RESEARCH_BUCKETS or b == "UNCATEGORIZED" for b in buckets)

    # --- releases
    numeric_tags = [t for t in git(clone, "tag", "-l").splitlines() if re.fullmatch(r"\d+\.\d+\.\d+", t)]
    newest_tag = max(numeric_tags, key=version_tuple) if numeric_tags else None
    out["newest_release_tag"] = newest_tag
    out["newest_release_date"] = git(clone, "log", "-1", "--format=%cd", "--date=short", newest_tag) if newest_tag else None
    out["installed_exe_version"] = exe_file_version(cfg["installed_exe"])
    installed_tag = cfg["installed_release"]
    if newest_tag and installed_tag and version_tuple(newest_tag) > version_tuple(installed_tag):
        out["warnings"].append(f"new release {newest_tag} available (installed {installed_tag}): read its release notes (Phase B) and tell the user")
    if out["installed_exe_version"] and installed_tag and out["installed_exe_version"] != installed_tag:
        out["warnings"].append(f"installed exe reports {out['installed_exe_version']} but CLAUDE.md says {installed_tag}: update CLAUDE.md after re-checking compatibility")
    if not out["installed_exe_version"]:
        out["warnings"].append("installed exe version unknown: add the exe path to the installed-game line in CLAUDE.md")

    # --- format gate (vs installed release)
    gate = False
    fmt_diff_lines = None
    if installed_tag and git(clone, "rev-parse", "--verify", "--quiet", installed_tag + "^{commit}", check=False):
        stat = git(clone, "diff", "--numstat", installed_tag, candidate, "--", *FORMAT_FILES)
        fmt_diff_lines = sum(int(a) + int(b) for a, b, _ in (l.split("\t") for l in stat.splitlines()) if a != "-")
        if fmt_diff_lines:
            gate = True
            out["warnings"].append(f"map_format_info.* differs from the installed release by {fmt_diff_lines} lines: read the diff before anything else")
    else:
        out["warnings"].append(f"tag {installed_tag} not found locally (run with --fetch); compatibility diff skipped")
    out["format_diff_lines_vs_installed"] = fmt_diff_lines

    def supported_version(rev: str) -> int | None:
        src = git(clone, "show", f"{rev}:src/fheroes2/maps/map_format_info.cpp", check=False)
        m = re.search(r"currentSupportedVersion\s*[{=(]\s*(\d+)", src)
        return int(m.group(1)) if m else None

    out["format_version_at_pin"] = supported_version(pin)
    out["format_version_at_candidate"] = supported_version(candidate)
    if cfg["format_version"] and out["format_version_at_candidate"] and out["format_version_at_candidate"] != cfg["format_version"]:
        gate = True
        out["warnings"].append(f"currentSupportedVersion is {out['format_version_at_candidate']} at the candidate, CLAUDE.md records v{cfg['format_version']}")
    out["format_gate"] = gate and ahead > 0

    # --- maps: recorded vs actual hashes
    appdata = Path(os.environ.get("APPDATA", "")) / "fheroes2" / "maps"
    map_rows = []
    for m in maps:
        vdoc = REPO / f"{m['name']}_validation.md"
        vhash = None
        vcommit = None
        if vdoc.is_file():
            t = read(vdoc)
            mh = re.search(r"SHA-256 `([0-9a-f]{64})`", t)
            mc = re.search(r"`([0-9a-f]{40})`", t)
            vhash = mh.group(1) if mh else None
            vcommit = mc.group(1) if mc else None
        row = {
            **m,
            "claude_md_hash": cfg["hashes"].get(m["name"]),
            "validation_doc_hash": vhash,
            "validation_doc_commit": vcommit,
            "repo_copy_hash": sha256(REPO / f"{m['name']}.fh2m"),
            "installed_copy_hash": sha256(appdata / f"{m['name']}.fh2m"),
        }
        recorded = row["claude_md_hash"] or row["validation_doc_hash"]
        problems = []
        if not recorded:
            problems.append("no recorded hash")
        if row["claude_md_hash"] and row["validation_doc_hash"] and row["claude_md_hash"] != row["validation_doc_hash"]:
            problems.append("CLAUDE.md and validation doc disagree")
        if recorded and row["repo_copy_hash"] and row["repo_copy_hash"] != recorded:
            problems.append("repo copy differs from recorded hash")
        if recorded and row["installed_copy_hash"] and row["installed_copy_hash"] != recorded:
            problems.append("installed copy differs from recorded hash")
        if row["installed_copy_hash"] is None:
            problems.append("not installed in %APPDATA%\\fheroes2\\maps")
        row["problems"] = problems
        for p in problems:
            out["warnings"].append(f"map {m['name']}: {p}")
        map_rows.append(row)
    out["maps"] = map_rows

    # --- verdict
    if ahead == 0 and head == pin:
        code, verdict = 0, "up to date: clone, pin and origin/master agree"
    elif out["format_gate"]:
        code, verdict = 3, "FORMAT GATE: serializer/format version differs from the installed release; read the diff and ask the user"
    elif research_hit:
        code, verdict = 2, "map-relevant upstream changes: Phase B (triage/re-research), then Phase C"
    elif ahead:
        code, verdict = 1, "upstream moved, nothing in a research bucket: Phase C (rebuild, hashes, gameload), then D and E"
    else:
        code, verdict = 1, "clone HEAD differs from pin: stop and ask"
    out.update({"exit_code": code, "verdict": verdict})

    if args.json:
        print(json.dumps(out, indent=2))
        return code

    # --- markdown report
    w = print
    w("# Upstream report")
    w("")
    if not out["preconditions_ok"]:
        w("**PRECONDITION FAILED** - " + "; ".join(x for x in out["warnings"] if "stop and ask" in x))
        w("")
    w(f"- Pin: `{pin[:9]}` ({out['pin_date']})  |  candidate `{candidate[:9]}` ({out['candidate_date']})  |  ahead {ahead}, behind {behind}")
    w(f"- Clone: `{clone}` on `{branch or 'detached'}` at `{head[:9]}`{' (dirty)' if dirty else ''}; "
      f"{'fetched now' if out['fetched'] else 'NOT fetched (age of last fetch: ' + str(out['last_fetch_age_hours']) + ' h; pass --fetch for a verdict)'}")
    w(f"- Release: newest tag `{newest_tag}` ({out['newest_release_date']}); CLAUDE.md installed `{installed_tag}`; exe reports `{out['installed_exe_version']}`")
    w(f"- Format: map_format_info.* diff vs `{installed_tag}` = {fmt_diff_lines} lines; currentSupportedVersion pin {out['format_version_at_pin']} -> candidate {out['format_version_at_candidate']} (CLAUDE.md v{cfg['format_version']})")
    w("")
    if commits:
        w(f"## Commits ({ahead})")
        w("")
        for c in commits[: args.max_commits]:
            w(f"- `{c['sha']}` {c['date']} {c['subject']}")
        if len(commits) > args.max_commits:
            w(f"- ... and {len(commits) - args.max_commits} more (use --json)")
        w("")
    if changed:
        w("## Changed files by bucket")
        w("")
        order = ["UNCATEGORIZED"] + sorted(RESEARCH_BUCKETS) + ["build", "irrelevant", "other"]
        for b in order:
            if b not in buckets:
                continue
            paths = buckets[b]
            w(f"### {b} ({len(paths)})")
            notes = {c["notes"] for c in changed if c["bucket"] == b and c["notes"] and c["notes"] != "—"}
            tools = {c["tools"] for c in changed if c["bucket"] == b and c["tools"] and c["tools"] != "—"}
            if notes:
                w(f"re-check in: {'; '.join(sorted(notes))}")
            if tools:
                w(f"tools: {'; '.join(sorted(tools))}")
            limit = len(paths) if b not in {"irrelevant", "other"} else 15
            for p in paths[:limit]:
                w(f"- {p}")
            if len(paths) > limit:
                w(f"- ... and {len(paths) - limit} more")
            w("")
    w("## Registered maps")
    w("")
    w("| map | seed | recorded (CLAUDE.md) | validation doc | repo copy | installed copy | problems |")
    w("|---|---|---|---|---|---|---|")
    for r in map_rows:
        short = lambda h: (h[:12] if h else "-")  # noqa: E731
        w(f"| {r['name']} | {r['seed']} | {short(r['claude_md_hash'])} | {short(r['validation_doc_hash'])} | {short(r['repo_copy_hash'])} | {short(r['installed_copy_hash'])} | {'; '.join(r['problems']) or 'none'} |")
    w("")
    if out["warnings"]:
        w("## Warnings")
        w("")
        for x in out["warnings"]:
            w(f"- {x}")
        w("")
    w(f"## Verdict: exit {code}")
    w("")
    w(verdict)
    return code


if __name__ == "__main__":
    sys.exit(main())
