#!/usr/bin/env python3
"""TASK-1725: name a missing git submodule and the command that fixes it.

A fresh `git worktree add` (before .githooks/post-checkout existed, or with hooks off) leaves the
submodule directories empty; the firmware build and the C++ host harnesses then fail on a missing
OpenTherm.h instead of saying what is wrong. build.py and scripts/run_tests.py call check() first.

Each submodule is recognised by one file it must contain, so an empty directory counts as missing.
Standard library only.
"""

import sys
from pathlib import Path

# name: (path, a file that exists once the submodule is checked out)
SUBMODULES = {
    "OpenTherm": ("src/libraries/OpenTherm", "src/OpenTherm.h"),
    "SimpleTelnet": ("src/libraries/SimpleTelnet", "library.properties"),
    # Read-only reference material (thermo-nova, the PIC source): tests that need it skip without it,
    # and CI has no access to it (.github/workflows/build.yml), so it is never required.
    "other-projects": ("other-projects", "CLAUDE.md"),
}
BUILD = {k: SUBMODULES[k] for k in ("OpenTherm", "SimpleTelnet")}


def missing(root, wanted):
    """The names in `wanted` whose marker file is absent under `root`."""
    root = Path(root)
    return [name for name, (path, marker) in wanted.items() if not (root / path / marker).is_file()]


def message(names):
    paths = [SUBMODULES[n][0] for n in names]
    lines = ["Missing git submodule(s): " + ", ".join(f"{n} ({p})" for n, p in zip(names, paths)) + "."]
    lines.append("The directory is empty, for example in a worktree made before TASK-1725. Fix:")
    lines.append("  git submodule update --init -- " + " ".join(paths))
    return "\n".join(lines)


def check(root, required=BUILD, optional=None):
    """Exit 2 with a clear message when a required submodule is missing; print a note for an
    optional one. Returns the list of missing optional names."""
    gone = missing(root, required)
    if gone:
        print(message(gone), file=sys.stderr)
        sys.exit(2)
    soft = missing(root, optional) if optional else []
    if soft:
        print("note: " + message(soft).replace("\n", "\n      ") + "\n      Tests that need it are skipped.",
              file=sys.stderr)
    return soft
