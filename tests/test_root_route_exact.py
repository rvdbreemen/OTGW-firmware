#!/usr/bin/env python3
"""TASK-1936: the "/" web route must use an exact URI matcher.

ESPAsyncWebServer 3.11 turns a plain string route into a BackwardCompatible matcher:
`path == uri || path.startsWith(uri + "/")` (WebServer.cpp, AsyncURIMatcher::matches). For "/" that
also catches every "//<file>" request, which then got the index page instead of the file.

This is a source check on the route registration, not a behaviour test: the matcher lives in the
library and is not compiled by the host harness. The behaviour is proven on the bench (GET //settings.ini
with credentials returns the file, not the index page).

Run: python tests/test_root_route_exact.py [--old-rev <rev>]
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REL = "src/OTGW-firmware/FSexplorer.ino"


def source(rev):
    if rev:
        return subprocess.check_output(["git", "show", f"{rev}:{REL}"], cwd=ROOT, text=True, encoding="utf-8")
    return (ROOT / REL).read_text(encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old-rev")
    text = source(ap.parse_args().old_rev)
    routes = re.findall(r"^\s*server\.on\((.+?),\s*HTTP_GET,\s*sendIndex\)", text, re.M)
    root = [r for r in routes if r.strip() in ('"/"', 'AsyncURIMatcher::exact("/")')]
    failures = []
    if len(root) != 1:
        failures.append(f"expected one root route to sendIndex, found {root}")
    elif root[0].strip() != 'AsyncURIMatcher::exact("/")':
        failures.append(f'root route is {root[0].strip()}: a BackwardCompatible "/" also matches every "//<file>" path')
    for f in failures:
        print("FAIL", f)
    print(("FAIL" if failures else "PASS") + f": root route matcher ({len(routes)} sendIndex routes seen)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
