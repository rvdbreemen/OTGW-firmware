---
id: TASK-1716
title: >-
  OT-Direct: fewer user-facing modes (Sergeant D: only monitor and gateway, as
  on the PIC)
status: In Progress
assignee:
  - '@system32-78'
created_date: '2026-11-08T07:11:00Z'
updated_date: '2026-11-08T14:40:00Z'
labels:
  - otdirect
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 825000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-08 05:55 #alpha-testing (msg 1568866608133308471), to Robert: 'Do we really need all these modes? In original OTGW we have only monitoring and gateway modes. Shouldn't OT-direct have the same modes?' Today OT-Direct has 5 (OTDirecttypes.h:50-54): bypass 0 (relay, thermostat straight to the boiler), gateway 1 (default), monitor 2, master 3 (no thermostat, sole OT master; needed for SAT without a thermostat), loopback 4 (internal test, simulated boiler). Proposal (not decided, Robert): the settings UI offers gateway and monitor only; master is entered automatically when SAT is on and no thermostat is seen (TASK-1694 auto-detect), bypass and loopback stay as service/test commands. Touches OTDmode setting/REST/MQTT, TASK-1694, TASK-1702.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides which modes stay user-facing
- [ ] #2 Settings UI, REST and MQTT match that decision; existing stored modes migrate without a surprise for the user (test)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-08: only gateway and monitor stay user-facing, as on the PIC. Master is entered automatically when no thermostat is seen (TASK-1694 auto-detect); bypass and loopback become service/test commands (REST/command only). Stored mode values must migrate without surprise: a stored master/bypass/loopback keeps working and the UI shows it as an automatic/service state. Before coding: check auto-detect covers SAT without a thermostat reliably (TASK-1694), since the user can no longer pick master.

Robert 2026-11-08 (after 54's check): PIC model. 'No thermostat' becomes a runtime state of gateway mode: detect the thermostat continuously, send own (standalone) frames while it is absent, relay again as soon as it appears (as gateway.asm:1740-1772); apply the TASK-1684 CH rule to the standalone frames. Then the UI shows only gateway + monitor. Needs an ADR (adr-kit) and a bench test with a thermostat plugged in late.

2026-11-08 auto-detect check (read-only, origin/dev 2f705e5e9 OTDirect.ino), the basis for Robert's PIC-model choice: SAT without a thermostat is covered (no frame in the 5 s window after step-up :945 + handshake -> master for this boot :1066-1091, SAT owns MsgID 1, TASK-1684 CH rule). Risk A: OT spec v4.2 sets no upper bound for a room unit's first frame after power-up (3.4.2.2; :575 gives the boiler 20 s for OT/+); a thermostat slower than handshake + 5 s is classed absent and in master mode answered from the cache (handleMasterModeSlaveFrame :2114), never relayed until the next boot. Risk B: bAutoDetect=false + stored gateway + no thermostat runs gateway standalone, whose own MsgID 0 keeps the board default CH enable (0x01) without a setpoint when SAT is off (buildStatusRequest clears bit 0 only in master mode). PIC: no master mode; missing thermostat detected continuously from the line voltage (gateway.asm:1740-1772), standalone frames while absent, re-attach when back. Not measured: real first-frame times of thermostats.

Robert 2026-11-08 (later, simplification): do the small variant now: hide only bypass and loopback in the settings UI (service/REST only), keep gateway, monitor and master visible; no runtime thermostat detection now. The PIC model (runtime detect) is parked as TASK-1728 until a field report needs it.

Small variant implemented 2026-11-08 (system32-78, wt-1716 lane-1716, not committed: commit + beta bump at the build slot). v2.js: OTD_SERVICE_MODES = {0: bypass, 4: loopback}; the otdmode select skips a service mode unless it is the stored value, which then shows selected as '<Name> (service mode)'. ENUM_OPTS.otdmode keeps all 5 labels (the TASK-1702 running note and the mode chip use them). REST: unchanged, so bypass/loopback stay settable as service modes (POST settings otdmode / commands). MQTT/HA: no OT-Direct mode entity exists (grep MQTT*/Discovery for otdmode/loopback: none), nothing to change. Stored values: untouched in firmware; a stored 0/4 keeps running and stays visible. Test tests/webui/otd-mode-selector-service-modes.test.mjs (real v2.html/v2.js, stored 1/3/0/4): old data 0 of 4 pass, fix 4 of 4. Regression otd-mode-running-note.test.mjs (TASK-1702) is flaky on its own: old data 1 of 2 runs fail, fix 2 of 2 pass.
<!-- SECTION:NOTES:END -->
