#!/usr/bin/env python3
"""One-off cleanup of leftover headless-Chrome profile dirs from the web UI tests (TASK-1492).

The tests create %TEMP%/otgw-cdp-* profiles. Before TASK-1492 a profile could survive when Chrome still
held it at removal time. This removes such dirs older than --min-age-minutes (default 60), so a test that
is running right now keeps its profile.

Run: python scripts/cleanup_cdp_profiles.py [--dry-run] [--min-age-minutes N]
"""
import argparse
import glob
import os
import shutil
import tempfile
import time

ap = argparse.ArgumentParser()
ap.add_argument("--dry-run", action="store_true")
ap.add_argument("--min-age-minutes", type=float, default=60)
a = ap.parse_args()
cut = time.time() - a.min_age_minutes * 60
dirs = [d for d in glob.glob(os.path.join(tempfile.gettempdir(), "otgw-cdp-*")) if os.path.isdir(d) and os.path.getmtime(d) < cut]
removed = 0
for d in dirs:
    if a.dry_run:
        continue
    shutil.rmtree(d, ignore_errors=True)
    removed += 0 if os.path.exists(d) else 1
print(f"found {len(dirs)} leftover profile dir(s) older than {a.min_age_minutes:g} min, removed {removed}" + (" (dry run)" if a.dry_run else ""))
