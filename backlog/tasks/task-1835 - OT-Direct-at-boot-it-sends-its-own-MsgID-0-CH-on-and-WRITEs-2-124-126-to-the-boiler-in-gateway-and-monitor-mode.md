---
id: TASK-1835
title: >-
  OT-Direct: at boot it sends its own MsgID 0 (CH on) and WRITEs 2, 124, 126 to
  the boiler in gateway and monitor mode
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-11T23:45:00Z'
updated_date: '2026-11-12T04:06:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 918000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N5), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: own frames only without a thermostat (StandAloneMsg G:1797-1820, HandleRequest G:2388-2407), never in monitor mode; never sends MsgID 2, 124 or 126 (MessageTable G:1822-1842, AdminMessages G:3438-3462).
- OT-Direct: initOTDirect sends buildStatusRequest() (flags 0x01 CH on, DHW off), then WRITE 2 = 0x0100, READ 3, WRITE 124 = 0x0233, WRITE 126 = 0x0001, in every mode (O:1076-1149, :949-962). 2026-11-11 audit listed this as info (CD report C12), nothing filed.
- Consequence: after every reboot/OTA the boiler gets one status frame CH on/DHW off plus a foreign master config before the thermostat's first frame.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 in gateway and monitor mode no gateway-made frame leaves before the first thermostat frame or before the no-thermostat decision
- [x] #2 host test on the real initOTDirect slice with mock master: zero requests in those modes; red on current code
- [ ] #3 bench capture of a boot with a thermostat: first frame to the boiler is a T
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PIC 6.8: own frames only without a thermostat (StandAloneMsg G:1797-1820, HandleRequest G:2388-2407), never in monitor mode. Move initOTDirect() steps 7-8 (boot status request + WRITE 2 / READ 3 / WRITE 124 / WRITE 126) into otdBootHandshake() and call it after the auto-detect (step 9) only in master mode. Gateway, monitor and bypass mode: nothing of our own at boot; gateway without a thermostat: the auto-detect switches to master and the handshake follows that decision; later own frames come from the scheduler under otdOwnFramesAllowed (TASK-1701). bOnline in gateway mode is set by handleMasterResponse() at the first relayed boiler answer.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): whether SAT or the boiler-presence flag (otBoilerCacheValid[3]) depends on the boot READ 3.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Implemented (lane-1762b, local): OTDirect.ino otdBootHandshake() holds the former steps 7-8; initOTDirect() calls it after step 9 only when IS_MASTER_MODE(). New tests/test_otd_boot_frames.py compiles the real initOTDirect() (and otdBootHandshake()) with a counting mock master: fix 6 OK (gateway, gateway with a thermostat via auto-detect, monitor, bypass: 0 frames; no thermostat: 0 before the decision, 5 after it once master; master: 5 as before). On the pre-1835 OTDirect.ino 5 failures, each (5, 0) != (0, 0). tests/test_otd_*.py and the OT-Direct board/ISR/W5500 tests: 22 files pass; test/host/test_override_reply OK; evaluate --quick 86/0. AC3 (bench capture of a boot with a thermostat) is not possible on the bench (no thermostat in OT-Direct gateway mode on any unit): field check to be asked from tjfs via comms 92 with the release note.

2026-11-12 shipped: origin/dev 6be2fa59b..f1bb2a000, zip 2.0.0-beta.200+f1bb2a0 posted #alpha-testing msg 1570288402668982272. Before the build: full run_tests.py 212/213 (test_stream_real_cbuf under -j 4, exit 0 alone, TASK-1854), evaluate --quick 86/0. Bench by 09 on .61 (old beta.197+4cafc97 vs new): OTA app+fs, githash f1bb2a0, bootcount 420 -> 422, health 200, MQTT, crashlog none, settings 164 unchanged; restored (settings diff 0). Evidence: 09 scratchpad b200/ (OLD-results.txt, NEW-results.txt, check200.py). TASK-1835 (beta.198): NOT provable at frame level on .61: no boiler and no bus listener (0 OT frames in 18 reboots on both builds), telnet starts after initOTDirect() (OTGW-firmware.ino:516 vs :496), and the handshake only follows a valid boiler status reply. Indicative proxy only, time to telnet after /ReBoot (3 runs): master OLD 15.7-15.8 s / NEW 15.6-16.1 s; gateway auto-detect off OLD 15.6-16.4 s / NEW 2.9, 15.1, 3.1 s (the ~12 s blocking boot status request without a boiler is gone in 2 of 3 runs; run 2 unexplained; OLD possibly skewed by an orphan load loop); gateway auto-detect on 20.8-21.3 / 20.9-21.3 s. AC3 stays open as a field check: a boot capture from a tester in OT-Direct gateway mode with a thermostat (tjfs). Status In Review for that field validation.
<!-- SECTION:NOTES:END -->
