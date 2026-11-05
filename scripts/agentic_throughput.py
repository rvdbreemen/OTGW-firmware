#!/usr/bin/env python3
"""Throughput figures of the agentic development cycle (TASK-1633), from repository data only.

    python scripts/agentic_throughput.py [--since YYYY-MM-DD] [--until YYYY-MM-DD]

Prints a Markdown report. Sources and their limits:
  tasks       backlog/tasks + backlog/archive/tasks frontmatter. Lead time = created_date to
              updated_date of tasks whose status is Done. updated_date is the last edit, which is
              usually the Done edit, so the figure is an upper-bound approximation.
  questions   docs/sat/open-questions.md: the first "asked YYYY-MM-DD[ ~]HH:MM" in a row's Status,
              and the date of the entry named by its checkmark, taken from
              docs/sat/sergeant-d-corrections.index.json (day resolution).
  labels      needs-maintainer-ok on tasks that are not Done.
  alpha tags  git log -G"_VERSION_PRERELEASE" -- src/OTGW-firmware/version.h on the current branch.
  waits       (TASK-1647) git history of the task files. A task waits for a maintainer decision
              while it carries a MAINTAINER_LABELS label, and for field evidence while its status
              is "In Review" or it carries a FIELD_LABELS label (convention:
              docs/guides/backlog-cli.md, "Marking a wait"). Start and stop are the commits that add
              and remove the marker, or the edit that sets the task to Done (a marker left on a
              Done task is stale, not a wait); the clock is the updated_date written in that same
              diff, else the commit time. Limits: auto_commit is off, so several edits can share one commit and
              only the last edit time is known; a marker set and cleared between two commits is
              invisible; a task that waits without a marker is not counted; history before
              2026-11-05 used these markers unevenly, so older waits are undercounted.
Needs a full clone; a shallow clone undercounts the alpha tags and the waits.
"""
import argparse
import datetime as dt
import json
import re
import statistics
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TASK_DIRS = [ROOT / "backlog" / "tasks", ROOT / "backlog" / "archive" / "tasks"]
QUESTIONS = ROOT / "docs" / "sat" / "open-questions.md"
INDEX = ROOT / "docs" / "sat" / "sergeant-d-corrections.index.json"

# Wait markers (TASK-1647). "field" and "field-report" mark where a report came from, not a wait.
MAINTAINER_LABELS = {"needs-maintainer-ok", "needs-decision"}
FIELD_LABELS = {"needs-field-data", "waiting-for-field", "field-verification"}
FIELD_STATUS = "In Review"
_LABEL_LINE = re.compile(r"^([+-])\s+-\s+['\"]?([\w.-]+)['\"]?\s*$")
_STATUS_LINE = re.compile(r"^([+-])status:\s*['\"]?(.*?)['\"]?\s*$")
_UPDATED_LINE = re.compile(r"^\+updated_date:\s*['\"]?(.*?)['\"]?\s*$")
_TASK_ID = re.compile(r"task-(\d+(?:\.\d+)*)\b", re.I)


def _kind(sign_line):
    """Wait kind a diff line adds or removes, or None."""
    m = _LABEL_LINE.match(sign_line)
    if m:
        if m.group(2) in MAINTAINER_LABELS:
            return m.group(1), "maintainer"
        if m.group(2) in FIELD_LABELS:
            return m.group(1), "field"
        return None
    m = _STATUS_LINE.match(sign_line)
    if m and m.group(2) == FIELD_STATUS:
        return m.group(1), "field"
    return None


def parse_wait_events(log_text):
    """Turn `git log --reverse -p -U0 --format='COMMIT %H %cI'` output into
    (time, task_id, kind, 'start'|'stop') events, oldest first. Per commit and task the
    added and removed markers are netted, so an archive move or rename is not a stop/start."""
    events = []

    def flush(commit_time, per_task):
        for tid, rec in per_task.items():
            when = rec["updated"] or commit_time
            for kind in sorted(rec["+"] - rec["-"]):
                events.append((when, tid, kind, "start"))
            stops = {"maintainer", "field"} if rec["done"] else rec["-"] - rec["+"]
            for kind in sorted(stops):
                events.append((when, tid, kind, "stop"))

    commit_time, per_task, cur = None, {}, None
    for line in log_text.splitlines():
        if line.startswith("COMMIT "):
            if commit_time is not None:
                flush(commit_time, per_task)
            stamp = dt.datetime.fromisoformat(line.split()[2])
            commit_time = stamp.astimezone(dt.timezone.utc).replace(tzinfo=None)
            per_task, cur = {}, None
        elif line.startswith("diff --git "):
            m = _TASK_ID.search(line.rsplit(" b/", 1)[-1])
            cur = per_task.setdefault(f"TASK-{m.group(1)}",
                                      {"+": set(), "-": set(), "updated": None, "done": False}) if m else None
        elif cur is not None and line[:1] in "+-" and not line.startswith(("+++", "---")):
            u = _UPDATED_LINE.match(line)
            if u:
                cur["updated"] = parse_time(u.group(1)) or cur["updated"]
                continue
            s = _STATUS_LINE.match(line)
            if s and s.group(1) == "+" and s.group(2) == "Done":
                cur["done"] = True  # Done ends every wait, also when a stale marker stays behind
            k = _kind(line)
            if k:
                cur[k[0]].add(k[1])
    if commit_time is not None:
        flush(commit_time, per_task)
    return events


def wait_intervals(events, now):
    """(task_id, kind, start, stop_or_None) per wait; a second start while open is ignored."""
    open_, out = {}, []
    for when, tid, kind, what in events:
        key = (tid, kind)
        if what == "start" and key not in open_:
            open_[key] = when
        elif what == "stop" and key in open_:
            out.append((tid, kind, open_.pop(key), when))
    out += [(tid, kind, start, None) for (tid, kind), start in open_.items()]
    return out


def git_wait_log():
    pattern = r"^(status: .?(" + FIELD_STATUS + r"|Done)|\s+- .?(" + "|".join(sorted(MAINTAINER_LABELS | FIELD_LABELS)) + r").?\s*$)"
    return subprocess.run(["git", "-C", str(ROOT), "-c", "core.quotePath=false", "log", "--reverse", "-p", "-U0",
                           "--format=COMMIT %H %cI", "-G" + pattern, "--",
                           "backlog/tasks", "backlog/archive/tasks"],
                          capture_output=True, text=True, encoding="utf-8", errors="replace").stdout


def parse_time(s):
    s = s.strip().strip("'\"")
    for fmt in ("%Y-%m-%dT%H:%M:%SZ", "%Y-%m-%d %H:%M", "%Y-%m-%d"):
        try:
            return dt.datetime.strptime(s, fmt)
        except ValueError:
            pass
    return None


def tasks():
    for d in TASK_DIRS:
        for p in d.glob("task-*.md"):
            text = p.read_text(encoding="utf-8", errors="replace")
            fm = text.split("---", 2)[1] if text.startswith("---") else ""
            get = lambda k: (re.search(rf"^{k}:\s*(.*)$", fm, re.M) or [None, ""])[1]
            labels = re.findall(r"^\s+-\s+(.*)$", (re.search(r"^labels:\s*\n((?:\s+-.*\n)*)", fm, re.M) or [None, ""])[1], re.M)
            yield {"id": get("id").strip("'\""), "status": get("status").strip("'\""),
                   "created": parse_time(get("created_date")), "updated": parse_time(get("updated_date")),
                   "labels": [l.strip().strip("'\"") for l in labels]}


def pct(values, q):
    if not values:
        return None
    s = sorted(values)
    return s[min(len(s) - 1, int(round(q * (len(s) - 1))))]


def fmt_days(v):
    return "n/a" if v is None else f"{v:.1f} d"


def fmt_hours(days):
    return "n/a" if days is None else f"{days * 24:.1f} h"


def main():
    ap = argparse.ArgumentParser()
    today = dt.date.today()
    ap.add_argument("--since", default=(today - dt.timedelta(days=30)).isoformat())
    ap.add_argument("--until", default=today.isoformat())
    a = ap.parse_args()
    since = dt.datetime.fromisoformat(a.since)
    until = dt.datetime.fromisoformat(a.until) + dt.timedelta(days=1)
    out = [f"# Agentic cycle throughput, {a.since} to {a.until}", "",
           "Generated by `scripts/agentic_throughput.py` (TASK-1633). Repository data only; see the script header for the limits of each figure.", ""]

    all_tasks = list(tasks())
    done = [t for t in all_tasks if t["status"] == "Done" and t["created"] and t["updated"]
            and since <= t["updated"] < until]
    lead = [(t["updated"] - t["created"]).total_seconds() / 86400 for t in done]
    created = [t for t in all_tasks if t["created"] and since <= t["created"] < until]
    out += ["## Tasks", "",
            "| Figure | Value |", "|---|---|",
            f"| Tasks created in the window | {len(created)} |",
            f"| Tasks Done in the window (by last update) | {len(done)} |",
            f"| Lead time created to Done, median | {fmt_days(statistics.median(lead) if lead else None)} |",
            f"| Lead time, 80th percentile | {fmt_days(pct(lead, 0.8))} |",
            f"| Tasks open now (To Do / In Progress) | {sum(1 for t in all_tasks if t['status'] == 'To Do')} / {sum(1 for t in all_tasks if t['status'] == 'In Progress')} |",
            f"| Open tasks labelled needs-maintainer-ok | {sum(1 for t in all_tasks if t['status'] != 'Done' and 'needs-maintainer-ok' in t['labels'])} |",
            ""]

    # Answer time: the "YYYY-MM-DD HH:MM UTC" on the entry's Source line, else the index date.
    import sys
    sys.path.insert(0, str(ROOT / "scripts"))
    import sat_corrections_index as sci
    entry_date = {}
    if INDEX.exists():
        for r in json.loads(INDEX.read_text(encoding="utf-8"))["entries"]:
            if r.get("date"):
                entry_date[r["id"]] = dt.datetime.fromisoformat(r["date"])
    register = sci.REGISTER.read_text(encoding="utf-8").replace("\r\n", "\n")
    for n, _title, body in sci.entries(register):
        t = re.search(r"(20\d\d-\d\d-\d\d)[ T,]+~?(\d\d):(\d\d) UTC", sci.source_line(body))
        if t:
            entry_date[n] = dt.datetime.fromisoformat(f"{t.group(1)} {t.group(2)}:{t.group(3)}")
    timed = set()
    for n, _title, body in sci.entries(register):
        if re.search(r"(20\d\d-\d\d-\d\d)[ T,]+~?(\d\d):(\d\d) UTC", sci.source_line(body)):
            timed.add(n)
    waits, open_ages, excluded = [], [], 0
    for line in QUESTIONS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^\| *((?:Q|Conf|QD)[\w.-]*)[^|]*\|([^|]*)\|([^|]*)\|", line)
        if not m:
            continue
        ans, status = m.group(2).strip(), m.group(3).strip()
        asked = re.search(r"asked (20\d\d-\d\d-\d\d)(?:[ ,]+~?(\d\d):(\d\d) UTC)?", status)
        asked_at = None
        if asked:
            asked_at = dt.datetime.fromisoformat(asked.group(1) + (f" {asked.group(2)}:{asked.group(3)}" if asked.group(2) else ""))
        em = re.match(r"✓\s*(\d+)", ans)
        if asked_at and em and int(em.group(1)) in entry_date and since <= asked_at < until:
            wait = (entry_date[int(em.group(1))] - asked_at).total_seconds() / 86400
            # Both ends need a UTC clock time; a negative wait means the row and entry do not pair up.
            if asked.group(2) and int(em.group(1)) in timed and wait >= 0:
                waits.append(wait)
            else:
                excluded += 1
        if re.match(r"(asked|queued)\b", status, re.I) and not ans.startswith("✓"):
            d = asked_at or (dt.datetime.fromisoformat(x.group(1)) if (x := re.search(r"(20\d\d-\d\d-\d\d)", status)) else None)
            if d:
                open_ages.append((m.group(1), (dt.datetime.combine(today, dt.time()) - d).days))
    out += ["## SAT questions to Sergeant D", "",
            "| Figure | Value |", "|---|---|",
            f"| Answered questions with UTC times on both ends | {len(waits)} (excluded: {excluded} without both times or not pairing up) |",
            f"| Asked to answer, median | {fmt_hours(statistics.median(waits) if waits else None)} |",
            f"| Asked to answer, 80th percentile | {fmt_hours(pct(waits, 0.8))} |",
            f"| Asked to answer, shortest / longest | {fmt_hours(min(waits) if waits else None)} / {fmt_hours(max(waits) if waits else None)} |",
            f"| Open now (asked or queued) | {len(open_ages)}: " + (", ".join(f"{q} {d} d" for q, d in open_ages) or "none") + " |",
            ""]

    now = dt.datetime.now(dt.timezone.utc).replace(tzinfo=None)
    intervals = wait_intervals(parse_wait_events(git_wait_log()), now)
    out += ["## Task waits for a maintainer decision or field evidence", "",
            "A wait runs from the edit that sets the marker to the edit that clears it (TASK-1647; markers and limits in the script header).", "",
            "| Figure | Maintainer decision | Field evidence |", "|---|---|---|"]
    rows = {}
    for kind in ("maintainer", "field"):
        closed = [((stop - start).total_seconds() / 86400, tid) for tid, k, start, stop in intervals
                  if k == kind and stop is not None and since <= stop < until]
        opened = [((now - start).total_seconds() / 86400, tid) for tid, k, start, stop in intervals
                  if k == kind and stop is None]
        days = [d for d, _ in closed]
        longest = max(closed) if closed else None
        oldest = max(opened) if opened else None
        rows[kind] = [str(len(closed)),
                      fmt_days(statistics.median(days) if days else None),
                      fmt_days(pct(days, 0.8)),
                      f"{longest[0]:.1f} d ({longest[1]})" if longest else "n/a",
                      str(len(opened)),
                      f"{oldest[0]:.1f} d ({oldest[1]})" if oldest else "n/a"]
    for i, label in enumerate(["Waits ended in the window", "Wait, median", "Wait, 80th percentile",
                               "Longest ended wait", "Waiting now", "Oldest open wait"]):
        out.append(f"| {label} | {rows['maintainer'][i]} | {rows['field'][i]} |")
    out.append("")

    log = subprocess.run(["git", "-C", str(ROOT), "log", f"--since={a.since}", f"--until={until.date().isoformat()}",
                          "--format=%ad", "--date=short", "-G_VERSION_PRERELEASE", "--",
                          "src/OTGW-firmware/version.h"], capture_output=True, text=True).stdout.split()
    per_day = Counter(log)
    days = max(1, (until - since).days)
    busiest = per_day.most_common(1)
    out += ["## Alpha and beta tags on this branch", "",
            "| Figure | Value |", "|---|---|",
            f"| Prerelease tags in the window | {len(log)} |",
            f"| Per calendar day, mean | {len(log) / days:.1f} |",
            f"| Busiest day | {busiest[0][0] + ' (' + str(busiest[0][1]) + ')' if busiest else 'n/a'} |",
            ""]
    print("\n".join(out))


if __name__ == "__main__":
    main()
