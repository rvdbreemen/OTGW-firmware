---
id: TASK-1763
title: Boot panic in ipc1 during GPIO ISR install (beta.162, .61, 1 of 10 reboots)
status: To Do
assignee: []
created_date: '2026-11-09T19:21:00Z'
labels:
  - bug
  - crash
  - boot
dependencies: []
priority: high
ordinal: 862000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench OTGW32 .61, 2.0.0-beta.162+5786928, 2026-11-09, TASK-1528 step b run (10 reboots via GET /ReBoot, run-20261109). 1 of 10 reboots ended in "Exception/panic" (crashlog 12:02:44, bootcount 189 -> 190); the other 9 were "Software reset". Robert waived it for the beta.162 public beta (2026-11-09).

Timeline (host log and telnet, local time): boot6 (two-tab trace boot, trace ended 11:53:16; uptime ~11 min, hd_min_free_heap 3592, idle except device/info every 5 s and a read-only telnet). 12:02:20 GET /ReBoot; deferred reboot "Reboot after upgrade" (37 ms); pre-doRestart heap=63152 minHeap=3592 exccause=0; doRestart: flushSettings 1 ms, mqtt disconnect 1 ms, ws close 0 ms, "stopping telnet+otgwstream" 12:02:20.463; one more device/info served at 12:02:20.644 (also in the 9 good reboots); telnet at 12:02:22 still got the old banner; telnet timeouts 12:02:28 and 12:02:34, connection refused 12:02:39 and 12:02:43; back at 12:02:45.8 with "Boot : Exception/panic", boot line exccause=1. That reboot took 25.5 s; the other 9 took 15.2-16.0 s.

Decode (by otgw-firmware-4c, ELF 88650453 from OTGW-build-archive): panic in ipc1, backtrace heap_caps_malloc <- esp_intr_alloc <- gpio_isr_register_on_core_static <- ipc_task, i.e. during GPIO ISR install.

Candidates (not proven): attachInterrupt / attachInterruptArg in src/libraries/OpenTherm/src/OpenTherm.cpp:95 and :100; attachInterrupt in src/OTGW-firmware/s0PulseCount.ino:50. Cause unknown; whether the panic happened in the restart path of the old boot or early in the new boot's setup is not settled by the logs (no serial capture).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Determine from the decode and code where the GPIO ISR install ran (restart path of the old boot vs setup of the new one) and why heap_caps_malloc in esp_intr_alloc panicked, with evidence
- [ ] #2 Reproduce or bound it on the bench: repeated /ReBoot cycles (including after a long uptime and a low-heap boot) with serial capture; count panics per N reboots
- [ ] #3 Fix, or a decision with Robert, with before/after panic counts
<!-- AC:END -->
