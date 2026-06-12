---
id: TASK-861
title: 'fix(pic): restore PIC banner recovery on esp32-classic'
status: Done
assignee:
  - '@codex'
created_date: '2026-06-11 21:10'
updated_date: '2026-06-11 21:23'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The esp32-classic build can miss the boot-time PIC ETX probe and then never recover because handlePICSerial returns before draining OTGWSerial when state.pic.bAvailable is false. Compare against dev branch ground truth and restore serial read-side recovery while keeping PIC command/control writes guarded until detection succeeds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A missed boot ETX on a PIC-capable build does not permanently block serial draining or PR=A banner recovery.
- [x] #2 PIC command writes and ser2net writes remain guarded while the PIC is unavailable.
- [x] #3 The esp32-classic target builds successfully after the change.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Compare current esp32-classic PIC detection and serial drain behavior against dev.
2. Restore the dev-compatible recovery path: keep reading OTGWSerial after a missed ETX so PR=A/banner recovery can set state.pic.bAvailable.
3. Keep writes to PIC/port 25238 guarded while the PIC is unavailable.
4. Build esp32-classic and run focused static checks.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Compared current 2.0.0 against dev and ADR-060. Dev continues draining the PIC UART even when state.pic.bAvailable is false; current 2.0.0 returned before reading, preventing the documented PR=A/banner recovery after a missed ETX.

Implemented a surgical fix in handlePICSerial(): PIC-capable builds now continue draining OTGWSerial while unavailable so the direct PR=A retry can be read; the port-25238 network-to-PIC write loop now requires isPICEnabled(), preserving the unavailable-PIC command guard.

Verification: direct PlatformIO compile passed for esp32-classic (1 succeeded in 00:02:25.947). evaluate.py --quick --no-color passed with 70 checks, 0 failures, 2 existing warnings. tests/test_evaluate.py passed 47 tests. tests/test_build.py passed 9 tests. Hardware Telnet smoke was not run from this environment.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed an esp32-classic PIC recovery regression by restoring the dev-branch serial-drain behavior for PIC-capable builds. handlePICSerial() no longer returns before reading OTGWSerial when state.pic.bAvailable is false, so the ADR-060 direct PR=A retry can receive the PIC banner and re-enable PIC functions after a missed boot ETX. The port-25238 network-to-PIC write loop remains guarded by isPICEnabled(), so commands are still blocked while the PIC is unavailable. Verified with esp32-classic PlatformIO build, evaluate.py --quick, tests/test_evaluate.py, and tests/test_build.py; live hardware Telnet smoke remains the only unrun checklist item.
<!-- SECTION:FINAL_SUMMARY:END -->
