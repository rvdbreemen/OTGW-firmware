---
id: TASK-359
title: 'fix(mqtt): enforce NTP and uptime preconditions in startDiscoveryVerification'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:33'
updated_date: '2026-04-21 16:55'
labels:
  - code-review
  - mqtt
  - bug
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1A HIGH#2: comment in doTaskMinuteChanged:316-319 claims NTP sync + uptime greater than 3600s guards are enforced inside startDiscoveryVerification, but they are not. Comment misleads future maintainers; first-boot verify may fire before per-source discovery topics have been published.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 startDiscoveryVerification returns false when isNTPtimeSet() is false
- [x] #2 startDiscoveryVerification returns false when state.uptime.iSeconds less than 3600
- [x] #3 Inline comment lists ALL preconditions in one block
- [x] #4 Comment in OTGW-firmware.ino:316-319 stays accurate after the change
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Confirm isNTPtimeSet() and state.uptime.iSeconds symbols (done)
2. Add two preconditions after existing checks in startDiscoveryVerification
3. Update inline comment to enumerate all preconditions in one block
4. Verify build
5. Check ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edit applied to MQTTstuff.ino (startDiscoveryVerification).
- Added two preconditions after isFlashing():
    if (!isNTPtimeSet()) return false;
    if (state.uptime.iSeconds < 3600) return false;
- Replaced the one-line header comment with a numbered 1..8 enumeration of every precondition currently enforced, including the new two.
- Confirmed isNTPtimeSet() declared in networkStuff.h:104 and defined in networkStuff.ino:468.
- Confirmed state.uptime.iSeconds is uint32_t in OTGW-firmware.h:256 and is the field used by OTGW-firmware.ino:272 for the same 3600s guard (restart-hour logic).
- The caller comment in OTGW-firmware.ino:316-319 is now truthful: every precondition it lists (NTP sync, uptime>3600, heap>=6000, no pending drip, MQTT connected) is enforced inside startDiscoveryVerification().
Build: python build.py --firmware passed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Enforced the two missing preconditions in `startDiscoveryVerification()` (MQTTstuff.ino) so the caller comment in `doTaskMinuteChanged()` (OTGW-firmware.ino:316-319) is no longer a lie.

Why:
- Phase-1A HIGH#2 flagged that the caller comment promised NTP-sync and uptime>=3600s guards, but neither was actually checked inside `startDiscoveryVerification()`. A first-boot run could fire before per-source discovery topics had been published, triggering a spurious "missing -> republish" cascade right at the point the heap is most fragile. It could also run before NTP sync, making `iLastVerifyEpoch` meaningless.

Changes:
- Added two early-returns immediately after the `isFlashing()` check:
    `if (!isNTPtimeSet()) return false;`
    `if (state.uptime.iSeconds < 3600) return false;`
- Replaced the single-line header comment with a numbered 1..8 enumeration of every precondition now enforced (already-verifying, MQTT connected, not flashing, NTP set, uptime>=3600s, no pending drip, heap >= VERIFICATION_MIN_HEAP_START, max-block >= VERIFICATION_BUFFER_BYTES+256). Now a maintainer can sanity-check the caller comment against the function body at a glance.
- Verified both symbols exist as claimed: `isNTPtimeSet()` declared in networkStuff.h:104 / defined in networkStuff.ino:468, and `state.uptime.iSeconds` is uint32_t in OTGW-firmware.h:256 (same field the restart-hour guard at OTGW-firmware.ino:272 already uses with an identical 3600s threshold, so the behaviour is internally consistent).

Tests:
- python build.py --firmware passed.
- No library/API surface change; strictly defensive guards that previously relied on the caller gating (which does not gate, because `doTaskMinuteChanged` fires every minute).

Risk: during the first hour after boot, auto-verify will be skipped. That was already the documented intent — this change just makes the documentation match reality.
<!-- SECTION:FINAL_SUMMARY:END -->
