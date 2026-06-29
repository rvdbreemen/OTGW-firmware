---
id: TASK-946
title: >-
  Review remediation: fix 945 WD arm/feed asymmetry + roster idx/reject
  validation
status: Done
assignee:
  - '@claude'
created_date: '2026-06-29 09:06'
updated_date: '2026-06-29 20:11'
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
- [x] #1 WatchDogEnabled arm (stateWatchdog!=0) gated on s_extWdPresent; disarm stays unconditional; on-device serial still 0 i2c errors on the no-0x26 classic-S3
- [x] #2 initWatchDog 0x26 probe retries (>=3x) before declaring absent; comment OTGW-Core.ino:1207-1218 updated to the symmetric-gated contract
- [x] #3 roster PUT/DELETE reject non-numeric idx with 400 (no silent slot-0 write); malformed mac/bindkey returns 400 not 200
- [x] #4 esp32-classic + esp32 + esp32-combo build green; evaluate.py --quick 0 new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
FIXED + ON-DEVICE VERIFIED 2026-06-29 (classic-S3 @COM8, alpha.289). WD: arm gated on s_extWdPresent (disarm unconditional), 3x probe retry, comment corrected. Serial: 0 i2c errors/11s, clean boot, no reset loop. Roster: strict strtol idx parse + mac/bindkey 400 validation (satBleParseRosterIdx/satBleMacValid/satBleBindkeyValid, #if HAS_SAT_BLE). 3-target build green, evaluate 0-fail. AC#3 roster 400 paths are build+logic verified; on-device HTTP 400-test deferred (classic-S3 not WiFi-provisioned; OTGW32 swapped out).

AC#3 ON-DEVICE VERIFIED 2026-06-29 (classic-S3 @192.168.88.64, alpha.292). curl over WiFi: PUT idx=abc->400, PUT idx=9 (range)->400, PUT idx=2 no-fields->400, PUT idx=2 mac=ZZZZ->400, DELETE idx=xyz->400, DELETE idx=99->400. GET roster->200. All malformed-input paths reject 400, no silent slot-0 write. Earlier deferral (device not provisioned) resolved: device was WiFi-provisioned during 949 testing.
<!-- SECTION:NOTES:END -->
