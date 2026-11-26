#!/usr/bin/env python3
"""Fail when a staged backlog task file shares its id with another backlog task file (TASK-3042).

Two files with one id make the backlog CLI refuse that id as ambiguous. The check reads the
`id:` line of every task file under backlog/tasks, backlog/archive/tasks, backlog/completed and
backlog/drafts, and fails only when a duplicate group contains a file that is part of the commit,
so a duplicate that already exists elsewhere does not block unrelated commits.

Usage: check_backlog_task_ids.py [--root DIR] [--staged FILE ...]   (default: staged files from git)
       check_backlog_task_ids.py --pre-push [--ref origin/dev] [--root DIR]   (ref lines on stdin, as git gives a pre-push hook)
Exit 0 = fine, 1 = a staged file has a duplicate id (or, with --pre-push, a pushed task id is already
taken in origin/dev by a file with another name).

Pre-push mode (TASK-3096): two sessions can create the same id in two worktrees that do not see each other's
file, and a rebase does not run the pre-commit hook. So before a push, every task file ADDED in the pushed
range is compared with `git ls-tree -r <ref> backlog/`: the same id under another file name (another title) is a
collision and blocks the push. The same file name is the same task. A file the range itself deletes or renames
away does not count. A ref that is not fetched only warns and skips.
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


FILE_ID_RE = re.compile(r"^task-(\d+(?:\.\d+)*)(?=[ -])", re.I)
ZERO = "0" * 40


def _git(root, *args):
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=root)


def _file_id(path):
    m = FILE_ID_RE.match(path.rsplit("/", 1)[-1])
    return "TASK-" + m.group(1) if m else None


def pushed_collisions(root, base, local, ref="origin/dev"):
    """Return (collisions, warning). A collision is (id, added path, [paths already at ref]); warning is a text or None."""
    if _git(root, "rev-parse", "--verify", "--quiet", ref + "^{commit}").returncode != 0:
        return [], f"{ref} is not fetched; the task id check is skipped (run git fetch)"
    ls = _git(root, "ls-tree", "-r", "--name-only", ref, "backlog/").stdout.splitlines()
    theirs = collections.defaultdict(set)
    for f in ls:
        if f.endswith(".md") and _file_id(f):
            theirs[_file_id(f)].add(f)
    diff = _git(root, "diff", "--name-status", "--no-renames", f"{base}..{local}", "--", "backlog/").stdout.splitlines()
    added = [l.split("\t", 1)[1] for l in diff if l.startswith("A\t") and l.endswith(".md")]
    gone = {l.split("\t", 1)[1] for l in diff if l.startswith("D\t")}
    out = []
    for f in added:
        tid = _file_id(f)
        if not tid:
            continue
        others = sorted(x for x in theirs.get(tid, ()) if x != f and x not in gone)
        if others:
            out.append((tid, f, others))
    return out, None


def pre_push(root, ref):
    if _git(root, "rev-parse", "--verify", "--quiet", ref + "^{commit}").returncode != 0:
        print(f"[task-id] warning: {ref} is not fetched; the task id check is skipped (run git fetch)", file=sys.stderr)
        return 0
    bad = []
    for line in sys.stdin.read().splitlines():
        parts = line.split()
        if len(parts) < 4 or parts[1] == ZERO:
            continue   # a delete push
        local, remote_sha = parts[1], parts[3]
        base = remote_sha
        if remote_sha == ZERO or _git(root, "cat-file", "-e", remote_sha + "^{commit}").returncode != 0:
            mb = _git(root, "merge-base", local, ref)
            if mb.returncode != 0:
                continue
            base = mb.stdout.strip()
        found, warn = pushed_collisions(root, base, local, ref)
        if warn:
            print(f"[task-id] warning: {warn}", file=sys.stderr)
            return 0
        bad += found
    for tid, f, others in bad:
        print(f"[task-id] {tid} is added by this push as '{f}' but {ref} already has it as:", file=sys.stderr)
        for o in others:
            print(f"[task-id]   {o}", file=sys.stderr)
    if bad:
        print("[task-id] Renumber your new task: git fetch, rebase onto origin/dev, give the file the next free id "
              "(backlog CLI picks max+1). Bypass: OTGW_TASKID_HOOK_DISABLE=1 git push ...", file=sys.stderr)
        return 1
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--staged", nargs="*")
    ap.add_argument("--pre-push", action="store_true")
    ap.add_argument("--ref", default="origin/dev")
    a = ap.parse_args()
    if a.pre_push:
        return pre_push(a.root, a.ref)
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
