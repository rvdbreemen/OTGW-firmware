---
id: TASK-950
title: 'Combo: gate IMU probe on persisted boardmode WITHOUT the boot-hang reorder'
status: To Do
assignee: []
created_date: '2026-06-29 18:36'
updated_date: '2026-07-09 21:26'
labels:
  - async-esp32s3
dependencies: []
ordinal: 163000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Deferred from TASK-949. Goal (maintainer design): on a cached/forced boot, SKIP the early probeProImu() so the regular S3 Mini's GPIO 12 (PIC_RST) is not driven every boot. Needs iBoardMode known BEFORE the probe (line ~258, before readSettings at ~292). TASK-949's first attempt moved LittleFS.begin+readSettings to the top of setup() to achieve this and HUNG boot on real hardware (app entered, no banner, no AP; reverted in d7a34f4a). Find a hang-free way: e.g. a minimal dependency-free early read of just the boardmode key from /settings.ini (or an RTC/NVS cache of the resolved mode), used only to gate the probe — without relocating the full readSettings/concurrency/LED init. MUST be on-device boot-verified before push (the reorder compiled clean but hung at runtime).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a cached boot (boardmode 1/2/3) the IMU probe does NOT run (verify via serial: no i2cWriteReadNonStop on a cached boot)
- [ ] #2 First/auto boot still probes + detects + persists as today
- [ ] #3 On-device: boots clean (banner+AP), no hang, no reset loop, on a real S3 Mini
- [ ] #4 esp32 + esp32-classic + esp32-combo build green; evaluate.py --quick 0 new fail
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 drain review: kept open — high boot-hang risk. TASK-949's attempt to get iBoardMode before the IMU probe (by moving LittleFS.begin+readSettings to setup() top) HUNG boot on real hardware (reverted d7a34f4a). A safe fix needs a minimal dependency-free early read of just the boardmode key (or an RTC/NVS cache) without relocating readSettings — delicate, and a wrong move hangs the bench boards (USB-reflash recovery only). Related to today's TASK-1031 work but distinct. This one warrants a dedicated careful session with on-device boot verification, not an aggressive-drain pass, per the boot-hang landmine.
<!-- SECTION:NOTES:END -->
