---
id: TASK-361
title: 'feat(mqtt): distinguish verify heap-abort from clean pass via outcome enum'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:34'
updated_date: '2026-04-21 17:43'
labels:
  - code-review
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1A MEDIUM and Phase 2B MEDIUM-1: current heap-abort at MQTTstuff.ino:313-319 sets verifyReceivedCount equal to expected to suppress false-missing republish; this also suppresses true-missing detection under pressure and mis-reports the outcome as clean. Telemetry lies, and republish logic cannot tell a clean pass from an aborted run.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New VerifyOutcome enum class with UNKNOWN, CLEAN, MISSING, ABORTED_HEAP, ABORTED_DISCONNECT values in OTGW-firmware.h
- [x] #2 state.discovery.eLastOutcome published alongside iLastMissingCount/iLastOrphanCount in devinfoV2
- [x] #3 Heap-abort path sets ABORTED_HEAP outcome without mutating verifyReceivedCount
- [x] #4 Republish suppression only applies when outcome is ABORTED_*, not when CLEAN
- [x] #5 Web UI or /api/v2/discovery GET exposes outcome label
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add VerifyOutcome enum + eLastOutcome field to DiscoverySection in OTGW-firmware.h
2. Remove verifyReceivedCount=expected hack in heap-abort path
3. Wire outcome writes: heap-abort -> ABORTED_HEAP, disconnect -> ABORTED_DISCONNECT, normal close -> CLEAN/MISSING
4. Guard republish by outcome==MISSING only
5. Expose outcome string label in /api/v2/discovery GET under verification.last_outcome
6. Add disc_last_outcome field in devinfoV2 (sendDeviceInfoV2)
7. Build firmware and check all ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
VerifyOutcome enum added to OTGW-firmware.h (inside DiscoverySection scope context). Field eLastOutcome=UNKNOWN by default.

MQTTstuff.ino changes:
- startDiscoveryVerification resets eLastOutcome=UNKNOWN when a new pass begins (so stale ABORTED_* from prior pass cannot leak).
- tickDiscoveryVerification heap-abort path sets ABORTED_HEAP and no longer mutates verifyReceivedCount (hack removed).
- tickDiscoveryVerification disconnect fast-path sets ABORTED_DISCONNECT and iLastVerifyEpoch for telemetry honesty.
- endDiscoveryVerification only classifies CLEAN/MISSING when outcome is still UNKNOWN; abort paths already wrote theirs.
- Republish now gated on outcome==MISSING, so ABORTED_* naturally suppresses republish without faking counts.

REST:
- verifyOutcomeLabel() helper returns PROGMEM labels.
- /api/v2/discovery GET response: new verification.last_outcome key (string). Buffer raised 320->384 to fit.
- sendDeviceInfoV2: new disc_last_outcome entry alongside existing disc_* telemetry.

Build: green (firmware 0.70 MB).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TASK-361 replaces the verifyReceivedCount=expected hack with an honest VerifyOutcome enum, so telemetry can distinguish a clean pass from an aborted one while still suppressing republish under pressure.

Changes:
- OTGW-firmware.h: new enum class VerifyOutcome { UNKNOWN, CLEAN, MISSING, ABORTED_HEAP, ABORTED_DISCONNECT }, added DiscoverySection::eLastOutcome.
- MQTTstuff.ino: startDiscoveryVerification resets the outcome; tickDiscoveryVerification sets ABORTED_HEAP (removing the false-count hack) and ABORTED_DISCONNECT on the corresponding paths; endDiscoveryVerification classifies CLEAN/MISSING only when the outcome is still UNKNOWN; republish is gated on outcome==MISSING so ABORTED_* paths skip it naturally.
- restAPI.ino: verifyOutcomeLabel() helper; /api/v2/discovery GET exposes verification.last_outcome; sendDeviceInfoV2 adds disc_last_outcome. GET response buffer raised 320->384 to fit the new key.

Impact:
- MISSING count is now honest even under heap pressure (no more fake "all received" on abort).
- Republish no longer fires under heap/disconnect conditions (unchanged behavior, cleaner mechanism).
- New telemetry field gives operators clear visibility into why a verify run did not republish.

Tests:
- python build.py --firmware: green.
- UI/REST consumers pick up disc_last_outcome automatically via the existing JSON field-map.
<!-- SECTION:FINAL_SUMMARY:END -->
