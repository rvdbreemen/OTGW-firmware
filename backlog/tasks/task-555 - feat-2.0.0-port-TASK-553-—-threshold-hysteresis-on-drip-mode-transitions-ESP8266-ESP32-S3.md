---
id: TASK-555
title: >-
  feat-2.0.0: port TASK-553 — threshold-hysteresis on drip mode transitions
  (ESP8266 + ESP32-S3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 08:51'
updated_date: '2026-05-07 08:51'
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
- [ ] #1 A new constant HEAP_LOW_RESTORE_THRESHOLD = HEAP_LOW_THRESHOLD + 1024 (=6144) is defined in helperStuff.ino with a comment explaining the deadband rationale
- [ ] #2 A new helper discoveryDripIsHeapHealthyForRestore() is added in MQTTstuff.ino, mirroring the existing discoveryDripHasHeapPressure() shape; ESP32 branch uses freeHeap >= 18432 && maxBlock >= 9216, ESP8266 branch uses freeHeap >= HEAP_LOW_RESTORE_THRESHOLD
- [ ] #3 loopMQTTDiscovery uses discoveryDripIsHeapHealthyForRestore() (not the inverted discoveryDripHasHeapPressure()) for the slow->normal restore decision; the normal->slow trigger remains discoveryDripHasHeapPressure()
- [ ] #4 loopMQTTDiscovery requires K=2 consecutive healthy reads before restoring to normal mode; counter resets to 0 on any unhealthy read; counter is updated only when the timer is due (not every loop iteration)
- [ ] #5 Existing time-hysteresis from TASK-370 (modeEnteredMs / canSwitch) is preserved unchanged
- [ ] #6 Block-header comment in loopMQTTDiscovery is updated to document all three hysteresis layers (time TASK-370 + threshold TASK-553 + K-ticks TASK-553) and reference both task numbers
- [ ] #7 Build for ESP8266 target exits 0 with no new warnings
- [ ] #8 Build for ESP32-S3 target exits 0 with no new warnings
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
