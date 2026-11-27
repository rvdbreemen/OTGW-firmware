#!/usr/bin/env python3
"""List what changed since a tag, for the beta notes of a release slot (docs helper, host only).

    python scripts/beta_changes_since.py [<since-tag>] [<until-ref>]     # default: newest v2.0.0-beta.* tag .. origin/dev

One line per non-merge commit with its TASK ids, in three groups: firmware (src/OTGW-firmware, src/libraries),
web UI (src/OTGW-firmware/data, src/OTGW-firmware/webui), and the rest (docs, backlog, tests, scripts: usually
internal). It is a checklist, not the notes: the user-visible lines are still written by hand, checked against the
code, and the SAT lines need the register entry numbers. The count of internal changes for "Behind the scenes" is the
size of the third group.
"""
import re
import subprocess
import sys


def git(*a):
    return subprocess.run(["git", *a], capture_output=True, text=True, check=True).stdout


def main(argv):
    since = argv[0] if argv else git("describe", "--tags", "--abbrev=0", "--match", "v2.0.0-beta.*", "origin/dev").strip()
    until = argv[1] if len(argv) > 1 else "origin/dev"
    log = git("log", "--no-merges", "--format=%h\x1f%s", f"{since}..{until}").strip().splitlines()
    groups = {"firmware": [], "web UI": [], "rest": []}
    for line in log:
        sha, subj = line.split("\x1f", 1)
        files = git("show", "--name-only", "--format=", sha).split()
        fw = any(f.startswith(("src/OTGW-firmware/", "src/libraries/")) and not f.startswith(("src/OTGW-firmware/data/", "src/OTGW-firmware/webui/", "src/OTGW-firmware/version")) for f in files)
        ui = any(f.startswith(("src/OTGW-firmware/data/", "src/OTGW-firmware/webui/")) for f in files)
        tasks = ",".join(sorted(set(re.findall(r"TASK-[0-9.]+", subj)), key=lambda t: t)) or "-"
        item = f"{sha} [{tasks}] {subj[:100]}"
        (groups["firmware"] if fw else groups["web UI"] if ui else groups["rest"]).append(item)
    print(f"since {since} until {until}: {len(log)} non-merge commits")
    for name, items in groups.items():
        print(f"\n== {name} ({len(items)})")
        for it in items:
            print("  " + it)
    print(f"\nBehind the scenes count (the 'rest' group): {len(groups['rest'])}; firmware and web UI commits need a line each, or an 'internal' decision.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
