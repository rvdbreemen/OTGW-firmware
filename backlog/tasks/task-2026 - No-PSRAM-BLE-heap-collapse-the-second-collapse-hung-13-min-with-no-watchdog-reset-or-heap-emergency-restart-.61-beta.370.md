---
id: TASK-2026
title: >-
  No-PSRAM BLE heap starvation: the unit hung 13 min with no watchdog reset or
  heap-emergency restart (.61, beta.370)
status: To Do
assignee: []
created_date: '2026-11-18T13:06:00Z'
updated_date: '2026-11-18T13:26:00Z'
labels:
  - heap
  - watchdog
  - bug
  - no-psram
dependencies: []
priority: high
ordinal: 1040000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2025, from otgw-bench-tester on .61 (no PSRAM, beta.370, 2026-11-18). With BLE on, the heap was starved from ~13:41 (free 7-13 KB, max block 2-5 KB, minFree 520 B) and the unit hung completely from ~13:50 until a USB reset at 14:03:10. The reboot log shows NO reboot between the 13:38:51 OTA and the USB reset: neither the heap-emergency restart (ADR-030 ladder / ADR-167 CRITICAL) nor the task watchdog (TWDT) reset the unit in about 22 minutes of starvation and 13 minutes of a full hang. In the field such a unit needs a power cycle. Find why neither recovery path fired; TASK-2025 removes the BLE trigger but not this gap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The hang is reproduced or explained from evidence (crash log, core dump, telnet trace up to the hang, emergency-recovery counters), not assumed
- [ ] #2 Named cause with file:line why neither the heap-emergency restart nor the TWDT reset the unit
- [ ] #3 Fix or recorded decision; the same reproduction shows a self-recovery (red/green)
<!-- AC:END -->
