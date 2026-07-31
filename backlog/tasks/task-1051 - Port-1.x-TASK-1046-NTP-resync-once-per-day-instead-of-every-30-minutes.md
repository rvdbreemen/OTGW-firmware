---
id: TASK-1051
title: 'Port 1.x TASK-1046: NTP resync once per day instead of every 30 minutes'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-31 19:50'
updated_date: '2026-07-31 20:33'
labels: []
dependencies: []
ordinal: 246000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x commit 6eda7ce28 (v1.7.2). dev still has NTP_RESYNC_TIME 1800 in OTGW-firmware.h:129, consumed at networkStuff.ino:762. The 1.x line raised it to 86400 to cut a recurring SDK-SNTP allocation cycle that was implicated in the ~80-minute heap death (TASK-1037/TASK-1050). NTP still syncs at boot; 24h drift is far below the 1s display resolution. One-line change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 NTP_RESYNC_TIME is 86400 in src/OTGW-firmware/OTGW-firmware.h with a comment citing the 1.x origin
- [x] #2 networkStuff.ino resync path unchanged apart from the constant
- [x] #3 build.bat green for esp32 target
- [x] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
NTP_RESYNC_TIME 1800 -> 86400 in OTGW-firmware.h, comment cites the 1.x origin (TASK-1046). networkStuff.ino:762 resync path untouched. build.bat --target esp32 SUCCESS (firmware + filesystem); evaluate.py 94 checks / 0 failures.
<!-- SECTION:NOTES:END -->
