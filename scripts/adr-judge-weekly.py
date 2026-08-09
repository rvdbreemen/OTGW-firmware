#!/usr/bin/env python3
"""Run the ADR LLM judge over every ADR, one isolated call each, with progress.

The pre-commit hook cannot do this. The judge makes one model call per ADR whose
scope the diff touches, sequentially, and 64 of this repo's 65 opted-in ADRs
declare no `path_glob`, so every one of them is in scope on every commit. At a
measured 17.7 s per call that is roughly 20 minutes of blocking, which is why
`judge.llm_enabled` is false in `.adr-kit.json` (ADR-089) and the hook runs the
declarative pass only.

Twenty minutes is fine once a week. This runner is that weekly pass: it judges
each ADR in isolation against the week's diff, prints a verdict per ADR as it
goes rather than at the end, and writes a report. `--llm` overrides the config's
llm_enabled=false for this invocation only; the hook keeps reading the config and
stays fast.

It is self-throttling, so CI can call it on every build. A stamp file records
when the pass last actually ran and what it concluded. Inside the interval the
runner exits in milliseconds without touching a model.

The remembered verdict matters as much as the timestamp: stamping a run that
found violations and then skipping for a week would turn CI green while the
violation stands. So a skip replays the previous violations and keeps failing
until a run comes back clean.

    python scripts/adr-judge-weekly.py                 # runs, or skips if fresh
    python scripts/adr-judge-weekly.py --status        # what the stamp says
    python scripts/adr-judge-weekly.py --force         # ignore the interval
    python scripts/adr-judge-weekly.py --only ADR-085  # re-check one after a fix

Exit 0 when no ADR reports a violation (or the stamp is fresh and was clean),
1 when any does, 2 on a setup error.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
ADR_DIR = REPO / "docs" / "adr"
# Tracked on purpose: the interval is shared state. A CI runner starts from a
# clean checkout every time, so a gitignored stamp would make every build a full
# pass, which is the opposite of the point. Override with $ADR_JUDGE_STAMP when
# the runner prefers a cache directory over a commit.
STAMP = Path(os.environ.get("ADR_JUDGE_STAMP") or (ADR_DIR / ".adr-judge-last-run.json"))
ENFORCEMENT_RX = re.compile(r"## Enforcement.*?```json\n(.*?)```", re.S)
ADR_ID_RX = re.compile(r"^(ADR-\d+)")


def find_judge() -> Path:
    """Resolve bin/adr-judge from the newest installed plugin version.

    Same resolution the pre-commit wrapper uses, so the weekly pass and the
    commit-time pass are always the same binary.
    """
    base = Path.home() / ".claude" / "plugins" / "cache" / "rvdbreemen-adr-kit" / "adr-kit"
    candidates = sorted(base.glob("*/bin/adr-judge"))
    if not candidates:
        raise SystemExit("adr-judge not found in the plugin cache; "
                         "run /adr-kit:install-hooks in a Claude Code session")
    return candidates[-1]


def judged_adrs(only: str | None) -> list[tuple[str, bool]]:
    """Return (adr_id, has_path_glob) for every ADR the LLM pass would evaluate.

    `llm_judge` defaults to TRUE as of adr-kit 0.47.0, so an Enforcement block
    without the key is opted IN. That is not a typo in the ADRs: the 0.47.0
    migration removed the explicit `"llm_judge": false` rather than writing
    `true`, which is why grepping for the key finds almost nothing while 65 ADRs
    are in fact enabled.
    """
    out: list[tuple[str, bool]] = []
    for path in sorted(ADR_DIR.glob("ADR-*.md")):
        m = ADR_ID_RX.match(path.name)
        if not m:
            continue
        adr = m.group(1)
        if only and adr != only:
            continue
        block = ENFORCEMENT_RX.search(path.read_text(encoding="utf-8", errors="replace"))
        if not block:
            continue                       # no Enforcement section: judge skips it
        try:
            rules = json.loads(block.group(1))
        except json.JSONDecodeError:
            print(f"WARNING: {path.name} has an unparseable Enforcement block; skipped",
                  file=sys.stderr)
            continue
        if not rules.get("llm_judge", True):
            continue
        globs = [r.get("path_glob")
                 for key in ("forbid_pattern", "forbid_import", "require_pattern")
                 for r in (rules.get(key) or []) if isinstance(r, dict)]
        out.append((adr, any(globs)))
    return out


def read_stamp() -> dict | None:
    try:
        return json.loads(STAMP.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None                        # absent or corrupt: treat as never run


def stamp_age_days(stamp: dict) -> float | None:
    try:
        last = dt.datetime.fromisoformat(stamp["last_run"])
    except (KeyError, ValueError, TypeError):
        return None
    if last.tzinfo is None:
        last = last.replace(tzinfo=dt.timezone.utc)
    return (dt.datetime.now(dt.timezone.utc) - last).total_seconds() / 86400


def write_stamp(judged: int, violations: list[str], duration_s: float, since: str) -> None:
    STAMP.parent.mkdir(parents=True, exist_ok=True)
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO,
                          capture_output=True, text=True).stdout.strip()
    STAMP.write_text(json.dumps({
        "last_run": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "commit": head,
        "since": since,
        "judged": judged,
        "violations": violations,
        "duration_s": round(duration_s, 1),
    }, indent=2) + "\n", encoding="utf-8")


def write_diff(since: str, out_path: Path) -> int:
    rev = subprocess.run(["git", "rev-list", "-1", f"--before={since} ago", "HEAD"],
                         cwd=REPO, capture_output=True, text=True).stdout.strip()
    if not rev:
        rev = subprocess.run(["git", "rev-list", "--max-parents=0", "HEAD"],
                             cwd=REPO, capture_output=True, text=True).stdout.strip()
    diff = subprocess.run(["git", "diff", f"{rev}..HEAD"],
                          cwd=REPO, capture_output=True, text=True).stdout
    out_path.write_text(diff, encoding="utf-8")
    return len(diff)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--since", default="7.days",
                    help="how far back the judged diff reaches (default 7.days)")
    ap.add_argument("--only", help="judge a single ADR, e.g. ADR-085")
    ap.add_argument("--diff", help="judge this diff file instead of deriving one")
    ap.add_argument("--timeout", type=int, default=120,
                    help="per-ADR timeout in seconds (default 120)")
    ap.add_argument("--report", default=str(REPO / "logs" / "adr-judge-weekly.md"))
    ap.add_argument("--max-age-days", type=float, default=7.0,
                    help="re-judge only when the last pass is older than this "
                         "(default 7); CI can call the runner every build")
    ap.add_argument("--force", action="store_true",
                    help="judge now regardless of how recent the last pass was")
    ap.add_argument("--status", action="store_true",
                    help="print what the stamp records and exit")
    args = ap.parse_args()

    stamp = read_stamp()
    age = stamp_age_days(stamp) if stamp else None

    if args.status:
        if not stamp:
            print(f"no stamp at {STAMP}; the pass has never completed here")
            return 0
        print(json.dumps(stamp, indent=2))
        print(f"age: {age:.2f} days" if age is not None else "age: unparseable")
        return 0

    # Deterministic fast path. Costs a file read, so CI can call this on every
    # build and only pay for the real pass once per interval.
    if stamp and age is not None and age < args.max_age_days and not args.force and not args.only:
        remembered = stamp.get("violations") or []
        if remembered:
            # Do NOT report clean just because the stamp is fresh: the last pass
            # found something and skipping would hide it for the rest of the week.
            print(f"last pass {age:.1f}d ago found {len(remembered)} violation(s), "
                  f"still unresolved: {', '.join(remembered)}")
            print(f"stamp: {STAMP}")
            print("Fix them and re-run with --force, or re-check one with --only ADR-NNN.")
            return 1
        print(f"skip: last pass {age:.1f}d ago was clean over {stamp.get('judged', '?')} "
              f"ADR(s); next due in {args.max_age_days - age:.1f}d")
        return 0

    judge = find_judge()
    adrs = judged_adrs(args.only)
    if not adrs:
        print("no ADRs to judge", file=sys.stderr)
        return 2

    if args.diff:
        diff_path = Path(args.diff)
        diff_bytes = diff_path.stat().st_size
    else:
        diff_path = Path(args.report).parent / "adr-judge-weekly.diff"
        diff_path.parent.mkdir(parents=True, exist_ok=True)
        diff_bytes = write_diff(args.since, diff_path)

    unbounded = sum(1 for _, bounded in adrs if not bounded)
    print(f"judge   : {judge}")
    print(f"diff    : {diff_path} ({diff_bytes} bytes, since {args.since})")
    print(f"adrs    : {len(adrs)} to judge, {unbounded} of them without a path_glob")
    print(f"estimate: ~{len(adrs) * 18 // 60} min at the measured 18 s per call\n")

    env = dict(os.environ)
    env.pop("ADR_KIT_NO_LLM", None)        # env force-off outranks --llm; clear it
    results: list[tuple[str, str, float, str]] = []
    started = time.time()

    for i, (adr, bounded) in enumerate(adrs, 1):
        t0 = time.time()
        try:
            proc = subprocess.run(
                [sys.executable, str(judge), "--llm", "--adr-dir", str(ADR_DIR),
                 "--diff", str(diff_path), "--dry-run-enforcement", adr,
                 "--llm-timeout", str(args.timeout)],
                cwd=REPO, capture_output=True, text=True, env=env,
                timeout=args.timeout + 60)
            code, out = proc.returncode, (proc.stdout or "") + (proc.stderr or "")
        except subprocess.TimeoutExpired:
            code, out = 124, "timed out"
        elapsed = time.time() - t0

        verdict = {0: "OK", 1: "VIOLATION", 2: "NOT FOUND", 124: "TIMEOUT"}.get(code, f"exit {code}")
        scope = "" if bounded else "  (unscoped)"
        # Printed per ADR, not collected for the end: a 20-minute run that only
        # speaks when it finishes is indistinguishable from a hung one.
        print(f"[{i:>3}/{len(adrs)}] {adr:<8} {verdict:<10} {elapsed:5.1f}s{scope}",
              flush=True)
        detail = ""
        if code == 1:
            detail = "\n".join(l for l in out.splitlines()
                               if "VIOLATION" in l or "reason" in l.lower())[:2000]
            for line in detail.splitlines():
                print(f"          {line}", flush=True)
        results.append((adr, verdict, elapsed, detail))

    total = time.time() - started
    violations = [r for r in results if r[1] == "VIOLATION"]
    problems = [r for r in results if r[1] not in ("OK",)]

    report = Path(args.report)
    report.parent.mkdir(parents=True, exist_ok=True)
    lines = [f"# ADR weekly judge", "",
             f"- diff: `{diff_path}` ({diff_bytes} bytes, since {args.since})",
             f"- judged: {len(results)} ADRs in {total/60:.1f} min",
             f"- violations: {len(violations)}", ""]
    for adr, verdict, elapsed, detail in results:
        lines.append(f"- **{adr}** {verdict} ({elapsed:.1f}s)")
        if detail:
            lines += ["", "  ```", *(f"  {d}" for d in detail.splitlines()), "  ```", ""]
    report.write_text("\n".join(lines) + "\n", encoding="utf-8")

    # Stamp only a pass that actually judged everything. Two ways it can fail to:
    # a --only run covered one ADR, and a run with timeouts or lookup errors
    # reached no verdict for those. Either would reset the clock on the strength
    # of work that did not happen, and the next six days of CI would report a
    # coverage the repository never got.
    unjudged = [r[0] for r in results if r[1] not in ("OK", "VIOLATION")]
    if args.only:
        print(f"stamp : not written (--only judged 1 of the ADR set)")
    elif unjudged:
        print(f"stamp : NOT written; {len(unjudged)} ADR(s) reached no verdict "
              f"({', '.join(unjudged[:5])}{'...' if len(unjudged) > 5 else ''}). "
              f"The next run will judge the full set again.")
    else:
        write_stamp(len(results), [r[0] for r in violations], total, args.since)
        print(f"stamp : {STAMP}")

    print(f"\njudged {len(results)} ADRs in {total/60:.1f} min; "
          f"{len(violations)} violation(s), {len(problems) - len(violations)} other issue(s)")
    print(f"report: {report}")
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
