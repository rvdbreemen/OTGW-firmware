---
id: TASK-555
title: >-
  feat-2.0.0: port TASK-553 — threshold-hysteresis on drip mode transitions
  (ESP8266 + ESP32-S3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 08:51'
updated_date: '2026-05-07 09:08'
labels:
  - mqtt
  - heap
  - quality-of-life
  - telemetry
  - 2.0.0
dependencies: []
priority: medium
ordinal: 21000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev-branch TASK-553 to the 2.0.0 feature line. Adds threshold-hysteresis (deadband) plus K-ticks consecutive-healthy-reads requirement on top of the existing time-hysteresis (TASK-370) for drip mode normal<->slow transitions in loopMQTTDiscovery.

Why: field log on dev (1.5.0-beta.20+cbc21af) shows ~60-90s slow/restore thrash at the boundary of HEAP_LOW_THRESHOLD because exit and entry use the same threshold. TASK-553 fixes this on dev for ESP8266; this task ports the same pattern to 2.0.0 (which has both ESP8266 and ESP32-S3 paths via discoveryDripHasHeapPressure helper).

Recommended values (mirrors TASK-553):
- ESP8266: HEAP_LOW_RESTORE_THRESHOLD = HEAP_LOW_THRESHOLD + 1024 = 6144 (same as dev)
- ESP32-S3: restore at freeHeap >= 18432 (16384 + 2048) AND maxBlock >= 9216 (8192 + 1024) — ~12-15% deadband above entry thresholds, mirroring the ESP8266 ratio. Conservative starting values; ESP32-S3 has ~300KB DRAM so thrash is unlikely in field but pattern-symmetry keeps the port clean. Tunable post-validation.
- K_TICKS = 2 consecutive healthy reads on slow-mode tick cadence (10s) before restoring.

Coordinated with TASK-553 (dev). Both tasks should be implemented and pushed in lock-step where logical.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A new constant HEAP_LOW_RESTORE_THRESHOLD = HEAP_LOW_THRESHOLD + 1024 (=6144) is defined in helperStuff.ino with a comment explaining the deadband rationale
- [x] #2 A new helper discoveryDripIsHeapHealthyForRestore() is added in MQTTstuff.ino, mirroring the existing discoveryDripHasHeapPressure() shape; ESP32 branch uses freeHeap >= 18432 && maxBlock >= 9216, ESP8266 branch uses freeHeap >= HEAP_LOW_RESTORE_THRESHOLD
- [x] #3 loopMQTTDiscovery uses discoveryDripIsHeapHealthyForRestore() (not the inverted discoveryDripHasHeapPressure()) for the slow->normal restore decision; the normal->slow trigger remains discoveryDripHasHeapPressure()
- [x] #4 loopMQTTDiscovery requires K=2 consecutive healthy reads before restoring to normal mode; counter resets to 0 on any unhealthy read; counter is updated only when the timer is due (not every loop iteration)
- [x] #5 Existing time-hysteresis from TASK-370 (modeEnteredMs / canSwitch) is preserved unchanged
- [x] #6 Block-header comment in loopMQTTDiscovery is updated to document all three hysteresis layers (time TASK-370 + threshold TASK-553 + K-ticks TASK-553) and reference both task numbers
- [x] #7 Build for ESP8266 target exits 0 with no new warnings
- [x] #8 Build for ESP32-S3 target exits 0 with no new warnings
- [ ] #9 Field-log re-capture under steady healthy heap on ESP8266 shows zero spurious slowed/restored pairs over 5-minute window; ESP32-S3 baseline confirmed not regressed
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add HEAP_LOW_RESTORE_THRESHOLD constant in helperStuff.ino (= HEAP_LOW_THRESHOLD + 1024 = 6144) — same as TASK-553 dev
2. In MQTTstuff.ino, add discoveryDripIsHeapHealthyForRestore() helper mirroring discoveryDripHasHeapPressure():
   - ESP32: freeHeap >= 18432 AND maxBlock >= 9216 (12-15% deadband above 16384/8192 entry)
   - ESP8266: freeHeap >= HEAP_LOW_RESTORE_THRESHOLD
3. In loopMQTTDiscovery:
   - Keep entry trigger heapPressure = discoveryDripHasHeapPressure()
   - Add restore guard heapHealthyForRestore = discoveryDripIsHeapHealthyForRestore()
   - Add static uint8_t consecutiveHealthyTicks counter (updated on timer-due ticks only)
   - Restore branch: heapHealthyForRestore && consecutiveHealthyTicks >= 2
4. Update block-header comment to document three hysteresis layers
5. Build ESP8266 target → exit 0
6. Build ESP32-S3 target → exit 0
7. Commit on feature-dev-2.0.0 branch with feat(mqtt) prefix referencing both TASK-553 (origin) and TASK-555 (this port)
8. ACs #9 (field-log re-capture) requires hardware
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation landed in commit 4c731474 on feature-dev-2.0.0. Same design pattern as dev TASK-553 but with platform-aware companion helper.

ESP8266 path: same HEAP_LOW_RESTORE_THRESHOLD (6144) as dev. Helper returns ESP.getFreeHeap() >= HEAP_LOW_RESTORE_THRESHOLD.

ESP32-S3 path: helper returns platformFreeHeap() >= 18432 && platformMaxFreeBlock() >= 9216. Mirrors the existing entry thresholds (16384/8192) with ~12-15% deadband. Conservative starting values; ESP32-S3 has ~300KB DRAM so thrash unlikely in field, but pattern-symmetry with ESP8266 keeps the port clean. Tunable post-validation.

Same K-ticks counter pattern (DRIP_RESTORE_K_TICKS = 2, post-DUE update, reset on unhealthy or slow-mode entry).

Build: ./build.sh --firmware exit 0 (esp8266 default); ./build.sh --firmware --target esp32 exit 0.
Evaluator: 59/2/0, 97.1% (zero failures).

AC #9 (field-log re-capture) left unchecked: requires hardware deployment on both ESP8266 and ESP32-S3 to validate.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports dev TASK-553 (drip mode threshold-hysteresis + K-ticks) to the 2.0.0 feature line, with platform-aware deadband sizing for the ESP32-S3 path.

Why
2.0.0 inherits the same drip mode thrash issue from dev (TASK-370 only fixes same-second toggling, not borderline-recovery cycling). Keeping the two branches in sync prevents divergence and makes future field-log analysis comparable across platforms.

Changes
- src/OTGW-firmware/OTGW-firmware.h: declared HEAP_LOW_RESTORE_THRESHOLD = 6144 (same as dev). Note: ESP8266 path only; ESP32 uses different thresholds via platform-specific helper.
- src/OTGW-firmware/MQTTstuff.ino:
  - Added DRIP_RESTORE_K_TICKS = 2 constant.
  - Added companion helper discoveryDripIsHeapHealthyForRestore():
    - ESP32: platformFreeHeap() >= 18432 && platformMaxFreeBlock() >= 9216 (~12-15% deadband above 16384/8192 entry thresholds).
    - ESP8266: ESP.getFreeHeap() >= HEAP_LOW_RESTORE_THRESHOLD (1KB deadband above 5120 entry).
  - Mirror of the existing discoveryDripHasHeapPressure() pattern; non-overlapping with the entry predicate (Schmitt-trigger / deadband).
  - Added static uint8_t consecutiveHealthyTicks counter, updated once per timer tick (post-DUE).
  - Restore branch requires !heapPressure && consecutiveHealthyTicks >= 2.
  - Mode-switch decision moved inside post-DUE block (tick-aligned with counter update).
  - Block-header comment updated to document all three hysteresis layers.
- src/OTGW-firmware/version.h, data/version.hash: build artifact bumps.
- backlog/tasks/task-555: this task.

Trade-off (same as dev)
Slow-mode engagement under sustained pressure now bounded by current normal interval (up to 2s) instead of loop-iteration latency. canPublishMQTT() remains the authoritative rate-limit safety net.

Tests
- ./build.sh --firmware (esp8266) exit 0. Pre-existing SATweather.ino warning unrelated.
- ./build.sh --firmware --target esp32 exit 0 (ESP32-S3 SUCCESS, 31.9% RAM, 98.0% flash). Pre-existing SATble volatile-++ and SimpleTelnet flush warnings unrelated.
- ./build.sh --evaluate-quick: 59/2/0, 97.1% (zero failures, no regression).
- Field-log re-capture (AC #9): requires hardware deployment on both ESP8266 and ESP32-S3.

Coordinated with dev TASK-553 (commit 2b21fd6b on dev). Both ports use identical pattern, only the platform-specific deadband values differ.

Risks / Follow-ups
- Field validation needed on both targets.
- ESP32-S3 deadband values (18432/9216) are conservative starting estimates without field data; tunable post-validation.
<!-- SECTION:FINAL_SUMMARY:END -->
