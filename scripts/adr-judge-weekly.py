#!/usr/bin/env python3
"""Run the ADR LLM judge over every ADR, one isolated call each, with progress.

The pre-commit hook cannot do this. The judge makes one model call per Accepted
ADR whose scope the diff touches, sequentially, and 58 of this repo's 68 judged
ADRs declare no `path_glob`, so every one of them is in scope on every commit.
At a measured 23 s per call that is roughly 26 minutes of blocking, which is why
`judge.llm_enabled` is false in `.adr-kit.json` (ADR-089) and the hook runs the
declarative pass only.

Twenty minutes is fine once a week. This runner is that weekly pass: it judges
each ADR in isolation against the week's diff, prints a verdict per ADR as it
goes rather than at the end, and writes a report. `--llm` overrides the config's
llm_enabled=false for this invocation only; the hook keeps reading the config and
stays fast.

It is self-throttling per ADR, so CI can call it on every build. Each ADR carries
its own last-judged timestamp and verdict in a tracked stamp file, and only ADRs
older than the interval are re-judged. Inside the interval the runner exits in
under half a second without touching a model.

Per ADR rather than per run, because that is what survives partial work: an ADR
that times out stays due while its neighbours stay fresh, an interrupted pass
keeps every verdict it reached, and a newly added ADR is judged next run instead
of inheriting someone else's interval.

The remembered verdict matters as much as the timestamp: recording a run that
found violations and then skipping would turn CI green while the violation
stands. So a skip replays them and keeps failing until a re-judge comes back
clean.

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
# Empirical, from a 9-ADR sample spanning a 21x range of Decision length (415 to
# 8722 chars) against the same 436 KB week diff: 20.2 s min, 21.7 s median,
# 27.8 s max, stdev 2.5 s. The spread is only 1.38x because most of a call is CLI
# startup rather than inference, so ADR size barely moves it.
PER_CALL_S = 23                    # mean, used only for the up-front estimate
DEFAULT_TIMEOUT_S = 180            # 6.5x the observed max: bounded, not strict
ENFORCEMENT_RX = re.compile(r"## Enforcement.*?```json\n(.*?)```", re.S)
ADR_ID_RX = re.compile(r"^(ADR-\d+)")
# Status lives in one of two places. Records written by adr-kit 0.47 carry YAML
# frontmatter; the older ones in this repo state it in prose under "## Status"
# ("Superseded by ADR-078, 2026-05-21."). Reading only the frontmatter classifies
# 73 of 74 as unknown, which is how a superseded ADR ended up in a judging run.
FRONTMATTER_STATUS_RX = re.compile(r'^status:\s*"?([A-Za-z]+)', re.M)
PROSE_STATUS_RX = re.compile(r"^##+ *Status *$\s*\n+\s*([A-Za-z]+)", re.M)


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


def adr_status(text: str) -> str:
    """Lowercased status word, from frontmatter or from the '## Status' prose."""
    m = FRONTMATTER_STATUS_RX.search(text) or PROSE_STATUS_RX.search(text)
    return m.group(1).lower() if m else "unknown"


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
        text = path.read_text(encoding="utf-8", errors="replace")
        if adr_status(text) != "accepted":
            # The judge evaluates Accepted ADRs; a Superseded or Rejected record
            # returns in half a second having done nothing. Filtering here keeps
            # the estimate honest and saves a subprocess per skipped ADR.
            continue
        block = ENFORCEMENT_RX.search(text)
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


def read_stamp() -> dict:
    """Per-ADR judging record: {"adrs": {"ADR-001": {last_run, verdict, ...}}}.

    Per ADR rather than one global timestamp, for three reasons. A newly added
    ADR is judged on the next run instead of waiting out someone else's interval.
    An ADR that timed out re-judges on its own while its neighbours stay fresh.
    And a run that is interrupted halfway keeps every verdict it did reach.

    The file is tracked, which is what makes this CI-independent: the record
    travels with the checkout, so it needs no runner cache, no build artifact and
    no scheduled workflow. A CI job that does not commit the file back simply
    leaves those ADRs due again, which is the safe direction.
    """
    try:
        data = json.loads(STAMP.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"schema": 1, "adrs": {}}
    if not isinstance(data.get("adrs"), dict):
        return {"schema": 1, "adrs": {}}
    return data


def age_days(iso: str | None) -> float | None:
    if not iso:
        return None
    try:
        last = dt.datetime.fromisoformat(iso)
    except (ValueError, TypeError):
        return None
    if last.tzinfo is None:
        last = last.replace(tzinfo=dt.timezone.utc)
    return (dt.datetime.now(dt.timezone.utc) - last).total_seconds() / 86400


def stamp_one(stamp: dict, adr: str, verdict: str, elapsed: float, head: str) -> None:
    """Record one ADR's verdict and flush immediately.

    Written per ADR, not once at the end: a 25-minute pass that is interrupted
    should keep what it already established, and an ADR that never reached a
    verdict must stay due rather than inherit the run's overall outcome.
    """
    stamp.setdefault("adrs", {})[adr] = {
        "last_run": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "verdict": verdict,
        "seconds": round(elapsed, 1),
        "commit": head,
    }
    STAMP.parent.mkdir(parents=True, exist_ok=True)
    STAMP.write_text(json.dumps({"schema": 1, "adrs": stamp["adrs"]},
                                indent=2, sort_keys=True) + "\n", encoding="utf-8")


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
    ap.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT_S,
                    help=f"per-ADR timeout in seconds (default {DEFAULT_TIMEOUT_S}, "
                         f"about 6.5x the measured 27.8 s worst case)")
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
    recorded = stamp.get("adrs", {})
    all_adrs = judged_adrs(args.only)
    if not all_adrs:
        print("no ADRs to judge", file=sys.stderr)
        return 2

    if args.status:
        fresh = due = never = 0
        for adr, _ in all_adrs:
            a = age_days((recorded.get(adr) or {}).get("last_run"))
            if a is None:
                never += 1
            elif a < args.max_age_days:
                fresh += 1
            else:
                due += 1
        print(f"stamp : {STAMP}")
        print(f"adrs  : {len(all_adrs)} judged by the LLM pass")
        print(f"        {fresh} fresh (< {args.max_age_days:g}d), {due} due, {never} never judged")
        stale_v = [a for a, _ in all_adrs
                   if (recorded.get(a) or {}).get("verdict") == "VIOLATION"]
        if stale_v:
            print(f"        {len(stale_v)} with a recorded VIOLATION: {', '.join(stale_v)}")
        return 0

    # A recorded violation keeps failing regardless of freshness. Its ADR is also
    # re-judged below when due, so a fix clears it on the next pass.
    outstanding = [a for a, _ in all_adrs
                   if (recorded.get(a) or {}).get("verdict") == "VIOLATION"]

    # Per-ADR selection: judge only what is actually due. A new ADR has no record
    # and is therefore always due, which is what makes this safe to add ADRs to.
    if args.force or args.only:
        adrs = all_adrs
    else:
        adrs = [(a, b) for a, b in all_adrs
                if (age_days((recorded.get(a) or {}).get("last_run")) or 1e9) >= args.max_age_days]

    if not adrs:
        nxt = min((args.max_age_days - (age_days((recorded.get(a) or {}).get("last_run")) or 0)
                   for a, _ in all_adrs), default=0)
        print(f"skip: all {len(all_adrs)} ADRs judged within {args.max_age_days:g}d; "
              f"next due in {nxt:.1f}d")
        if outstanding:
            print(f"but {len(outstanding)} carry an unresolved VIOLATION: {', '.join(outstanding)}")
            return 1
        return 0

    judge = find_judge()

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
    print(f"estimate: ~{len(adrs) * PER_CALL_S // 60} min at the measured "
          f"{PER_CALL_S} s per call\n")

    env = dict(os.environ)
    env.pop("ADR_KIT_NO_LLM", None)        # env force-off outranks --llm; clear it
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO,
                          capture_output=True, text=True).stdout.strip()
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
        if verdict in ("OK", "VIOLATION"):
            # Only a real verdict stamps, and it stamps only this ADR. A timeout
            # leaves that ADR due while the rest of the run keeps its results.
            stamp_one(stamp, adr, verdict, elapsed, head)
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

    # Each ADR stamped itself the moment it reached a verdict, so there is no
    # end-of-run stamp to write. What is left is to say which ones did not, and
    # are therefore still due.
    unjudged = [r[0] for r in results if r[1] not in ("OK", "VIOLATION")]
    if unjudged:
        print(f"due again: {len(unjudged)} ADR(s) reached no verdict, so they were "
              f"not stamped ({', '.join(unjudged[:5])}"
              f"{'...' if len(unjudged) > 5 else ''})")
    print(f"stamp : {STAMP}")

    print(f"\njudged {len(results)} ADRs in {total/60:.1f} min; "
          f"{len(violations)} violation(s), {len(problems) - len(violations)} other issue(s)")
    print(f"report: {report}")
    return 1 if violations or outstanding else 0


if __name__ == "__main__":
    raise SystemExit(main())
