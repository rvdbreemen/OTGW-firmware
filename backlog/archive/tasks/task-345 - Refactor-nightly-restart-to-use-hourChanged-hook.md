---
id: TASK-345
title: Refactor nightly restart to use hourChanged hook
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 07:43'
updated_date: '2026-04-20 07:49'
labels:
  - 1.4.1
  - cleanup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the inline wall-clock compare in OTGW-firmware.ino:264-276 with the existing hourChanged() helper, eliminating the minute=0 window and reviving hourChanged() from dead-code status.

**Current problem**
The nightly restart block runs on every iteration of its containing per-minute task and does a ZonedDateTime conversion every time, then checks hour==iRestartHour && minute==0. Relying on minute==0 creates a 60-second firing window that depends on timer tick alignment.

**Proposed change**
Gate the entire check with hourChanged(). First main-loop iteration after the hour flips triggers exactly one evaluation; subsequent iterations within the same hour return early via short-circuit.

```cpp
if (settings.bNightlyRestart && settings.ntp.bEnable 
    && state.uptime.iSeconds > 3600 && hourChanged()) {
  // NTP-synced hour check, restart if hour matches iRestartHour
}
```

**Why not dayChanged()**
sendtimecommand() in networkStuff.ino:494 already consumes the dayChanged() event. A second caller would create a race: whoever calls first consumes, the other misses.

hourChanged() currently has zero callers, so making this block its sole consumer is safe. Document the coupling for future callers.

**Guards preserved**
- bNightlyRestart opt-in
- ntp.bEnable sanity
- uptime > 3600s (restart-loop protection)
- time() > 2000-01-01 (NTP-synced)

**Out of scope**
Reboot-robust dayChanged() via LittleFS file. Documented in earlier discussion as TASK-candidate for when a persistent daily caller appears.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Inline minute==0 check removed from OTGW-firmware.ino
- [x] #2 hourChanged() used as gate on the nightly restart block
- [x] #3 hourChanged() no longer dead code (has at least one caller)
- [x] #4 Comment above the block updated to explain the hourChanged-gated design
- [x] #5 Existing guards preserved: bNightlyRestart, ntp.bEnable, uptime > 3600, time > 2000
- [x] #6 Build passes for esp8266
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit OTGW-firmware.ino:261-276 — replace inline wall-clock block with hourChanged()-gated version
2. Update comment to explain the new design
3. Build esp8266 via build.py
4. Commit + push to origin/1.4.1
5. Final summary with before/after
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Refactored the nightly restart block in OTGW-firmware.ino (doTaskEvery60s) to use hourChanged() as gate:

- Removed: minute==0 wall-clock window check
- Added: hourChanged() in the outer condition, leveraging short-circuit evaluation so AceTime conversion only happens once per hour boundary
- Preserved: all existing guards (bNightlyRestart, ntp.bEnable, uptime>3600, time()>2000-01-01)

Side effect: hourChanged() is no longer dead code. The refactored block is its sole caller in 1.4.x. Inline comment warns that adding a second caller would create an event-consumption race.

Build verified: incremental esp8266 firmware compile clean. Binary 723,680 bytes (~0.1% larger than pre-refactor due to extra comment block and one additional AceTime path at hour boundary, negligible).

Commit 22daada8 on origin/1.4.1.
<!-- SECTION:FINAL_SUMMARY:END -->
