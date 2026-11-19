#!/usr/bin/env python3
"""TASK-1993 (TASK-1955 AC2, option A): log the app image size against its flash slot, warn above 95 %, fail above 98 %.

    python scripts/check_app_size.py <app.bin> <slot_bytes>      # exit 0 OK or WARN, 1 FAIL, 2 usage error

The esp32-combo app runs from the single app slot app0 (ota_0, 0x230000 B in partitions_otgw_esp32.csv, ADR-191);
the safeboot updater writes it from the factory partition, so the limit is that slot, not twice the image.
PlatformIO's upload.maximum_size only fails at 100 %; this check makes the headroom visible on every build and
stops the build while there is still room to plan a size reduction (candidates in TASK-1955 / TASK-1639).
"""
import sys
from pathlib import Path

WARN_PERCENT = 95.0
FAIL_PERCENT = 98.0


def main(argv):
    if len(argv) != 3:
        print("usage: check_app_size.py <app.bin> <slot_bytes>", file=sys.stderr)
        return 2
    img = Path(argv[1])
    try:
        slot = int(argv[2], 0)
        size = img.stat().st_size
    except (ValueError, OSError) as e:
        print("check_app_size: %s" % e, file=sys.stderr)
        return 2
    if slot <= 0:
        print("check_app_size: slot size must be > 0", file=sys.stderr)
        return 2
    pct = 100.0 * size / slot
    print("app size: %d B of %d B (%.1f %%), free %d B [%s]" % (size, slot, pct, slot - size, img.name))
    if pct > FAIL_PERCENT:
        print("FAIL: app image above %.0f %% of its slot; reduce the image first (TASK-1955 candidates)" % FAIL_PERCENT)
        return 1
    if pct > WARN_PERCENT:
        print("WARN: app image above %.0f %% of its slot; plan a size reduction (TASK-1955 candidates)" % WARN_PERCENT)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
