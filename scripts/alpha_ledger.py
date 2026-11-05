#!/usr/bin/env python3
"""Rebuild the alpha/beta build ledger from git and the local post records (TASK-1632).

The coordinator keeps .claude/alpha-build-ledger.local.md by hand. This script rebuilds the same
facts so the ledger survives the loss of that file or of the machine:

  from git      every commit on origin/dev that changes _VERSION_PRERELEASE in
                src/OTGW-firmware/version.h: the new tag, the commit and the TASK ids in its message
  from disk     %LOCALAPPDATA%/OTGW-capture/published-tags.txt (written by discord-post-build.sh;
                one tag per posted zip) and published-posts.tsv next to it (written since
                TASK-1632: time, tag, version+hash, Discord message id, the -m text)

    python scripts/alpha_ledger.py [--last N] [--ref origin/dev]

A tag that was not posted itself is shown as covered when a later post's -m text names it
(single tags such as beta.58 and ranges such as beta.57-60 are understood).
"""
import argparse
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_H = "src/OTGW-firmware/version.h"
STORE = Path(os.environ.get("LOCALAPPDATA") or Path.home() / ".local" / "share") / "OTGW-capture"


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout


def bumps(ref, last):
    out = git("log", ref, f"-n{last}", "--format=%h%x09%ad%x09%s", "--date=short",
              "-G_VERSION_PRERELEASE", "--", VERSION_H)
    rows = []
    for line in out.splitlines():
        h, date, subject = line.split("\t", 2)
        text = git("show", f"{h}:{VERSION_H}")
        m = re.search(r"^#define _VERSION_PRERELEASE\s+(\S+)", text, re.M)
        core = re.search(r'^#define _SEMVER_CORE\s+"([^"]+)"', text, re.M)
        if not m:
            continue  # the production commit comments the prerelease out
        tag = m.group(1).strip('"')
        body = git("log", "-1", "--format=%B", h)
        tasks = sorted(set(re.findall(r"\bTASK-\d+(?:\.\d+)?\b", body)),
                       key=lambda t: [int(x) for x in re.findall(r"\d+", t)])
        rows.append({"tag": tag, "core": core.group(1) if core else "2.0.0", "commit": h,
                     "date": date, "tasks": tasks})
    return rows


def expand(text):
    """Tag tokens in a post text: 'beta.57-60' -> beta.57..beta.60, 'alpha.522' -> alpha.522."""
    found = set()
    for word, a, b in re.findall(r"\b(alpha|beta|rc)\.(\d+)(?:\s*-\s*(?:\1\.)?(\d+))?", text):
        for n in range(int(a), int(b or a) + 1):
            found.add(f"{word}.{n}")
    return found


def posted():
    tags = set()
    f = STORE / "published-tags.txt"
    if f.exists():
        for line in f.read_text(encoding="utf-8").split():
            tags.add(line if re.match(r"\d+\.\d+\.\d+-", line) else f"2.0.0-{line}")
    posts = []
    p = STORE / "published-posts.tsv"
    if p.exists():
        for line in p.read_text(encoding="utf-8").splitlines():
            cols = line.split("\t")
            if len(cols) >= 5:
                posts.append({"time": cols[0], "key": cols[1], "build": cols[2], "msg": cols[3], "text": cols[4]})
    return tags, posts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--last", type=int, default=40)
    ap.add_argument("--ref", default="origin/dev")
    a = ap.parse_args()
    tags, posts = posted()
    rows = bumps(a.ref, a.last)
    print("| Tag | Commit (bump) | Date | Tasks | Posted | Post / covered by |")
    print("|---|---|---|---|---|---|")
    keys = [f"{r['core']}-{r['tag']}" for r in rows]  # newest first
    for i, r in enumerate(rows):
        key = keys[i]
        post = next((p for p in posts if p["key"] == key), None)
        if key in tags:
            state = "yes"
            where = f"msg {post['msg']} ({post['build']})" if post else "posted (no post record; before TASK-1632)"
        else:
            state = "no"
            cover = next((p for p in posts if r["tag"] in expand(p["text"]) and p["key"] != key), None)
            # CLAUDE.md alpha-tag protocol: a tag that was not posted itself is covered by the next
            # posted zip of the same core. rows is newest first, so look at the newer rows.
            later = next((keys[j] for j in range(i - 1, -1, -1)
                          if keys[j] in tags and rows[j]["core"] == r["core"]), None)
            if cover:
                where = f"covered by {cover['key']} post (msg {cover['msg']}, named in its -m text)"
            elif later:
                where = f"covered by the next posted zip, {later}"
            else:
                where = "not posted yet"
        print(f"| {r['tag']} | {r['commit']} | {r['date']} | {', '.join(r['tasks']) or '-'} | {state} | {where} |")


if __name__ == "__main__":
    main()
