---
id: TASK-2026
title: >-
  No-PSRAM BLE heap starvation: the unit hung 13 min with no watchdog reset or
  heap-emergency restart (.61, beta.370)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T13:06:00Z'
updated_date: '2026-11-18T21:37:00Z'
labels:
  - heap
  - watchdog
  - bug
  - no-psram
milestone: 2.0.0
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 otgw-dev: cause and fix (option A, Robert DM 1572611211378565161; ADR-258 accepted on his 'Ja', DM 1572613449282756649). Why nothing restarted the unit: the WiFi dead-link supervisor (networkStuff.ino) skipped every probe it could not run for lack of memory ('heap not healthy', 'no pcb', 'no echo buffer') as 'skipped', never as 'dead', so a starved heap never counted toward the restart; the heap-emergency ladder and the TWDT did not fire because the loop kept running. Fix: wifiProbeStarvedDead() (networkStuff.ino:816-830) counts such a skip as a dead probe when the check is armed and the largest free block is under WIFI_STARVED_MAXBLOCK 4096 (:649), used at :867, :890, :896; the restart reason stays 'WiFi: connected but no traffic'. Red/green: tests/test_wifi_deadlink.py::WifiDeadLinkStateMachine::test_a_starved_heap_counts_as_dead_and_recovers FAILS on origin networkStuff.ino (OTGW_NETWORK_INO), the file passes 35/35 with the heaphold test on the fix (heaphold in its own commit). evaluate --quick 88/0. adr-lint --strict: ADR-258 PASS strictly, repo FAIL count 63 unchanged. Not built, not benched yet (bench: 'H' heap-hold on .61, prerelease only).
<!-- SECTION:NOTES:END -->
