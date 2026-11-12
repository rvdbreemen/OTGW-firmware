---
id: TASK-1835
title: >-
  OT-Direct: at boot it sends its own MsgID 0 (CH on) and WRITEs 2, 124, 126 to
  the boiler in gateway and monitor mode
status: To Do
assignee: []
created_date: '2026-11-11T23:45:00Z'
updated_date: '2026-11-12T00:10:00Z'
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
- [ ] #1 in gateway and monitor mode no gateway-made frame leaves before the first thermostat frame or before the no-thermostat decision
- [ ] #2 host test on the real initOTDirect slice with mock master: zero requests in those modes; red on current code
- [ ] #3 bench capture of a boot with a thermostat: first frame to the boiler is a T
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): whether SAT or the boiler-presence flag (otBoilerCacheValid[3]) depends on the boot READ 3.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
