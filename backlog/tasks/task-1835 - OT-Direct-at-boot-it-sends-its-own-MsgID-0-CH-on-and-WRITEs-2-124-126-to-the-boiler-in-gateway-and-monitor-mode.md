---
id: TASK-1835
title: >-
  OT-Direct: at boot it sends its own MsgID 0 (CH on) and WRITEs 2, 124, 126 to
  the boiler in gateway and monitor mode
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:45:00Z'
updated_date: '2026-11-12T00:56:00Z'
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
<!-- SECTION:NOTES:END -->
