#!/usr/bin/env python3
"""Fail a docs(adr)/docs(skill) subject that names Robert or Accepted and carries a time later than the commit's
author date (TASK-3098, retro proposal 2c).

A subject such as "docs(adr): ADR-265 Accepted (Robert, 2026-11-26 ~01:10)" written at 00:44 states a time that has
not happened yet. The author date is the clock to compare with (with its UTC offset): a rebase rewrites the committer
date, not the author date. Only 'YYYY-MM-DD HH:MM' and 'YYYY-MM-DD ~HH:MM' are read, so a planned slot such as
'08:00 beta' does not trigger. A past exact time passes.

Usage: check_commit_subject_time.py <commit message file> [--author-date ISO | --author-ident "Name <e> EPOCH +ZZZZ"]
Without either, the author identity comes from `git var GIT_AUTHOR_IDENT`. Exit 0 = fine, 1 = a future time.
"""
import argparse
import re
import subprocess
import sys
from datetime import datetime, timedelta, timezone

PREFIX_RE = re.compile(r"^docs\((adr|skill)\)", re.I)
NAME_RE = re.compile(r"Robert|Accepted")
TIME_RE = re.compile(r"(\d{4})-(\d{2})-(\d{2})\s+~?(\d{1,2}):(\d{2})")


def author_datetime(iso=None, ident=None):
    if iso:
        return datetime.fromisoformat(iso)
    if ident is None:
        ident = subprocess.run(["git", "var", "GIT_AUTHOR_IDENT"], capture_output=True, text=True).stdout.strip()
    m = re.search(r"(\d+) ([+-])(\d{2})(\d{2})\s*$", ident)
    if not m:
        return None
    off = timedelta(hours=int(m.group(3)), minutes=int(m.group(4)))
    tz = timezone(off if m.group(2) == "+" else -off)
    return datetime.fromtimestamp(int(m.group(1)), tz)


def future_times(subject, now):
    """Times in the subject (read in the author's UTC offset) that are later than now."""
    if not PREFIX_RE.match(subject) or not NAME_RE.search(subject):
        return []
    out = []
    for y, mo, d, h, mi in TIME_RE.findall(subject):
        try:
            when = datetime(int(y), int(mo), int(d), int(h), int(mi), tzinfo=now.tzinfo)
        except ValueError:
            continue
        if when > now:
            out.append(f"{y}-{mo}-{d} {int(h):02d}:{mi}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("msgfile")
    ap.add_argument("--author-date")
    ap.add_argument("--author-ident")
    a = ap.parse_args()
    with open(a.msgfile, encoding="utf-8", errors="replace") as f:
        subject = f.readline().strip()
    now = author_datetime(a.author_date, a.author_ident)
    if now is None:
        return 0
    bad = future_times(subject, now)
    if bad:
        print(f"[commit-time] the subject says {', '.join(bad)}, but this commit is authored {now.isoformat(timespec='minutes')}.", file=sys.stderr)
        print("[commit-time] A time in the past is fine; write the real time of the event (check the DM or the clock), "
              "not a guess ahead.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
