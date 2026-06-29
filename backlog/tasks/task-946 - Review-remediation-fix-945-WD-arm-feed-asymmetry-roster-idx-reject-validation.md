---
id: TASK-946
title: >-
  Review remediation: fix 945 WD arm/feed asymmetry + roster idx/reject
  validation
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-29 09:06'
updated_date: '2026-06-29 09:06'
labels:
  - async-esp32s3
dependencies: []
ordinal: 159000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Aggressive code review (2026-06-29) found: (HIGH) TASK-945 gated the 0x26 feed on a runtime probe but left WatchDogEnabled(1) arm UNCONDITIONAL — a present 0x26 whose single-shot probe false-negatives gets armed-but-unfed -> ESP reset. Contradicts the documented 2026-06-14 always-feed directive (OTGW-Core.ino:1207-1218). Fix (option a): gate the ARM on s_extWdPresent too (disarm stays unconditional, safe via the boot disarm), add a 3x probe retry, fix the stale comment. (MEDIUM/LOW-MED, TASK-935 roster) restAPI.ino roster PUT returns 200 on silent updateSetting reject; non-numeric idx -> atoi=0 -> writes/clears slot 0. Fix: reject non-digit idx with 400, return 400 on malformed mac/bindkey.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 WatchDogEnabled arm (stateWatchdog!=0) gated on s_extWdPresent; disarm stays unconditional; on-device serial still 0 i2c errors on the no-0x26 classic-S3
- [ ] #2 initWatchDog 0x26 probe retries (>=3x) before declaring absent; comment OTGW-Core.ino:1207-1218 updated to the symmetric-gated contract
- [ ] #3 roster PUT/DELETE reject non-numeric idx with 400 (no silent slot-0 write); malformed mac/bindkey returns 400 not 200
- [ ] #4 esp32-classic + esp32 + esp32-combo build green; evaluate.py --quick 0 new failures
<!-- AC:END -->
