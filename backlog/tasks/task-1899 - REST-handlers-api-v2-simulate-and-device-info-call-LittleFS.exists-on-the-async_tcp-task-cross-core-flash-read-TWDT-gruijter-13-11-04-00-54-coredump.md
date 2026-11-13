---
id: TASK-1899
title: >-
  REST handlers /api/v2/simulate and /device/info call LittleFS.exists on the
  async_tcp task: cross-core flash read, TWDT (gruijter 13-11 04:00:54,
  coredump)
status: To Do
assignee: []
created_date: '2026-11-13T15:45:00Z'
updated_date: '2026-11-13T16:52:00Z'
labels:
  - bug
  - stability
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 968000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's beta.218+2f5ecb4 capture (Discord #alpha-testing https://discord.com/channels/812969634638725140/1514720723980259460/1570818512295952525, analysis 1570826381443469447). Task-watchdog reset at 13 Nov 04:00:54 local (reboot 103). Core dump decoded against the matching .elf (app SHA ffcd6c3e, esp-coredump 1.18.1): the watchdog named loopTask (CPU 1); the running task was async_tcp, stuck in a cross-core flash read: sendSimulationStatus (restAPI.ino:376) -> LittleFS.exists("/otgw_simulation.log") -> lfs -> spi_flash_disable_interrupts_caches_and_other_cpu. The web UI called /api/v2/simulate 7,668 times in 18.5 h (about every 9 s, two browsers). Same pattern as the beta.182 health crash that TASK-1865 fixed for /api/v2/health only. device/info also calls LittleFS.exists in async_tcp context (restAPI.ino:3410, fsupdateincomplete). Both lines verified on origin/dev 3b1221c46 by the coordinator. Two more resets (08:38, 11:59) were 'Other watchdog' with no core dump: cause unknown, not attributed to this. Fix direction (coordinator): cache the existence on the loop task, like the TASK-1865 health fix, and add a grep gate or test that forbids LittleFS.* calls in REST handlers that run outside the loop task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No LittleFS call runs on the async_tcp task for /api/v2/simulate and /api/v2/device/info: existence is cached on the loop task (host test or source test on the real handlers, red on the old code)
- [ ] #2 A gate (evaluate.py check or test) fails on a LittleFS.* call inside a REST handler that runs on async_tcp, red on the old code, green on the fix
- [ ] #3 Bench: polling /api/v2/simulate and /device/info every 1-2 s for at least 2 h during filesystem activity gives no watchdog reset
- [ ] #4 Field: no TWDT with async_tcp in LittleFS in a long-run capture from a tester
- [ ] #5 Bench (sim): poll /api/v2/simulate every 9 s while SAT runs through PWM ON/OFF switches (simulation mode), at least 2 h, no watchdog reset
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 double correlation (gruijter beta.218 capture, transcript, checked by comms; Sergeant D asked 1570834690892497138, answered 1570836136136417321): both "Other watchdog" resets without a core dump fell 14-17 s after a regular SAT PWM ON switch (08:38:04 duty 0.79 -> reset 08:38:18; 11:58:50 duty 0.22 -> reset 11:59:07; ticks mode=2 pwm=1) AND 1.5-13 s after a web UI /api/v2/simulate call (08:38:05.563, 11:59:05.572; polled every 15 s). No log lines between the switch and the reset. Not a proven cause either way; the bench should exercise both.

2026-11-13 CORRECTION of the earlier "double correlation" note (comms misread the log): the line "SAT PWM: pwm_on=... ON" (SATcontrol.ino:1539 at 2f5ecb4) is printed on every 30 s control tick during a phase; it is NOT the ON switch. The real ON edges are the "PWM ON:" lines (SATcontrol.ino:1231, satPwmBeginOn): 08:36:04 (on=489 s) and 11:58:20 (on=275 s). The two "Other watchdog" resets came ~134 s and ~47 s INTO those ON phases, not 14-17 s after a switch. 19 ON edges that day, 17 without a reset. At an ON edge only CS, MM=0, CH=1 are queued plus a log line; no flash work (SATcontrol.ino:1231-1240, :1666-1690). Reset reason: platform_esp32.h:216 maps ESP_RST_WDT (raw 7) to "Other watchdog" = RTC/super/CPU-level watchdog, not the task watchdog (6) or interrupt watchdog (5); no new core dump (the 04:00:54 dump was still the stored one). Device logged live OT frames until 1.6 s / 2.7 s before reboot (reboot time from uptime). Ranked hypotheses (trace agent, read-only): H1 hardware-level watchdog, trigger unknown; H2 IPC stack overflow (this unit ipc0/ipc1 low points 80/88 B of 1024; beta.219 raised the stack) escalating to H1; H3 external 0x26 watchdog unlikely (loop task fed it every <=100 ms until 1-3 s before reboot; an EN-pin reset would read "Power on", wiring not verified); H4 SAT/PWM, LittleFS or /simulate: no evidence. The /simulate correlation (1.5-13 s before) remains a coincidence candidate only. Bench next: capture the ROM rst:0x.. line on the S3 USB console during such a reset; beta.218 vs beta.219 on a Classic+PIC with two web UIs polling and SAT in PWM. Corrected publicly: 1570843356219703308 (earlier message 1570836136136417321 marked as corrected).
<!-- SECTION:NOTES:END -->
