#!/usr/bin/env python3
"""Fail when a staged backlog task file shares its id with another backlog task file (TASK-3042).

Two files with one id make the backlog CLI refuse that id as ambiguous. The check reads the
`id:` line of every task file under backlog/tasks, backlog/archive/tasks, backlog/completed and
backlog/drafts, and fails only when a duplicate group contains a file that is part of the commit,
so a duplicate that already exists elsewhere does not block unrelated commits.

Usage: check_backlog_task_ids.py [--root DIR] [--staged FILE ...]   (default: staged files from git)
Exit 0 = fine, 1 = a staged file has a duplicate id.
"""
import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

DIRS = ("backlog/tasks", "backlog/archive/tasks", "backlog/completed", "backlog/drafts")
ID_RE = re.compile(r"^id:\s*['\"]?(TASK-[\d.]+)['\"]?\s*$", re.M | re.I)


def task_ids(root):
    groups = collections.defaultdict(list)
    for d in DIRS:
        base = Path(root) / d
        if not base.is_dir():
            continue
        for f in sorted(base.glob("*.md")):
            m = ID_RE.search(f.read_text(encoding="utf-8", errors="replace")[:3000])
            if m:
                groups[m.group(1).upper()].append(f"{d}/{f.name}")
    return groups


def check(root, staged):
    staged = {s.replace("\\", "/") for s in staged}
    bad = {i: fs for i, fs in task_ids(root).items() if len(fs) > 1 and staged.intersection(fs)}
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--staged", nargs="*")
    a = ap.parse_args()
    staged = a.staged
    if staged is None:
        out = subprocess.run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
                             capture_output=True, text=True, cwd=a.root).stdout
        staged = [x for x in out.splitlines() if x.startswith("backlog/") and x.endswith(".md")]
    bad = check(a.root, staged)
    for i, fs in sorted(bad.items()):
        print(f"[task-id] {i} is used by {len(fs)} files:", file=sys.stderr)
        for f in fs:
            print(f"[task-id]   {f}", file=sys.stderr)
    if bad:
        print("[task-id] Give the new file the next free id (backlog CLI picks max+1). "
              "Bypass: OTGW_TASKID_HOOK_DISABLE=1 git commit ...", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
