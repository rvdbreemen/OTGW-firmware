---
id: TASK-155
title: 'OTGW32-Audit-4D: Master/standalone mode — sole OT master behavior'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:20'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-4
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit master mode (GW=2/S): ESP32 acts as the sole OT master with no thermostat connected. The schedule drives all boiler communication. Verify thermostat connectivity tracking is suspended, slave ISR is disabled/ignored, and the control loop operates on internal state only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In master mode, schedule drives all boiler communication
- [x] #2 Thermostat connectivity tracking is suspended (no setback triggered)
- [x] #3 Slave ISR frames are ignored (no thermostat connected)
- [x] #4 PR=M returns 'S' (standalone) for master mode
- [x] #5 SAT control loop can drive CS=/MM= setpoints in master mode
- [x] #6 Any issue results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

- GW=2 and GW=S both map to setOTDirectMode(OTD_MODE_MASTER) (line 1915-1917) — both accepted — correct
- setOTDirectMode MASTER (line 1405): relay LOW, then checks bEnableSlave: if false calls otSlave.end() (pure standalone), if true keeps slave running for thermostat — correct
- loopOTDirect() (line 1175): IS_MASTER_MODE() handled first — calls handleMasterModeSlaveFrame() which responds with cached boiler data, does NOT forward to boiler — correct
- checkThermostatTimeout() (line 1453): setback only engages if otCurrentMode == OTD_MODE_GATEWAY — NOT in master mode — thermostat tracking suspended correctly
- PR=M (line 1959): IS_MASTER_MODE() maps to char S (standalone) — matches spec — correct
- scheduleMasterRequest() runs in master mode driving all boiler communication — correct
- handleMasterModeSlaveFrame (line 1488): WRITE_DATA MsgID 1 causes setOverride+updateWriteCache so SAT/CS commands can drive setpoints — correct
- No behavioral gaps found.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Master/standalone mode audit: all ACs pass.

- GW=2 and GW=S both accepted (line 1915-1917), both map to OTD_MODE_MASTER
- setOTDirectMode MASTER: relay LOW; slave interface stopped (otSlave.end()) when bEnableSlave=false for pure standalone
- loopOTDirect() handles IS_MASTER_MODE() first (line 1175): handleMasterModeSlaveFrame() responds with cached boiler data — does NOT forward to boiler master bus
- checkThermostatTimeout() (line 1453): setback guard is otCurrentMode==OTD_MODE_GATEWAY — not triggered in master mode, thermostat connectivity tracking suspended correctly
- scheduleMasterRequest() drives all boiler communication as sole OT master
- handleMasterModeSlaveFrame: WRITE_DATA MsgID 1 -> setOverride+updateWriteCache enables SAT/CS= to drive CH setpoints
- PR=M returns S (standalone) — correct
- No behavioral gaps found.
<!-- SECTION:FINAL_SUMMARY:END -->
