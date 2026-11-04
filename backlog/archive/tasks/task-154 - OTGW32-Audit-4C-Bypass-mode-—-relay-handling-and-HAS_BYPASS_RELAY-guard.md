---
id: TASK-154
title: 'OTGW32-Audit-4C: Bypass mode — relay handling and HAS_BYPASS_RELAY guard'
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
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit bypass mode (GW=0): on OTGW32 boards with a bypass relay (HAS_BYPASS_RELAY=1), the relay must connect thermostat directly to boiler, bypassing the ESP32. On boards without a relay, GW=0 must respond with NG (No Good). Verify relay state transitions and that firmware correctly compiles both variants.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GW=0 with HAS_BYPASS_RELAY=1 activates relay and sets mode to OTD_MODE_BYPASS
- [x] #2 GW=0 without HAS_BYPASS_RELAY returns NG response
- [x] #3 Relay is deactivated when switching out of bypass mode
- [x] #4 Mode change is reflected in PR=M response ('P' for bypass)
- [x] #5 Both HAS_BYPASS_RELAY=0 and =1 builds compile without errors
- [x] #6 Any issue results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

- GW=0 handler (line 1902): correctly guarded by #if HAS_BYPASS_RELAY — calls setOTDirectMode(OTD_MODE_BYPASS) when relay present, otherwise processOT("NG",2) — correct
- setOTDirectMode BYPASS (line 1380): digitalWrite(PIN_BYPASS_RELAY, HIGH) — relay activated — correct
- setOTDirectMode GATEWAY (line 1387): digitalWrite(PIN_BYPASS_RELAY, LOW) — relay deactivated on mode switch — correct
- All other mode transitions (MONITOR, MASTER, LOOPBACK, line 1396-1424): all call digitalWrite(PIN_BYPASS_RELAY, LOW) under #if HAS_BYPASS_RELAY — relay deactivated when leaving bypass — correct
- initOTDirect() (line 473): if saved mode is BYPASS and no relay, force to GATEWAY — boot safety correct
- initOTDirect() (line 480): initializes relay pin and sets state on boot — correct
- PR=M response (line 1957): IS_BYPASS_MODE() maps to char P — matches spec "P for bypass" — correct
- Both compile variants: #if HAS_BYPASS_RELAY properly guards all relay operations — correct
- No behavioral gaps found. task-177 (runtime relay config) already tracked separately.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Bypass mode relay handling and HAS_BYPASS_RELAY guard audit: all ACs pass.

- GW=0 handler (line 1902): properly guarded by #if HAS_BYPASS_RELAY; calls setOTDirectMode(OTD_MODE_BYPASS) when relay present, returns NG when not
- setOTDirectMode BYPASS: digitalWrite(PIN_BYPASS_RELAY, HIGH) — relay activated correctly
- All other mode transitions (GATEWAY, MONITOR, MASTER, LOOPBACK): all drive relay LOW under HAS_BYPASS_RELAY guard
- initOTDirect(): board without relay forces BYPASS->GATEWAY if persisted mode was bypass
- PR=M returns P for bypass mode — correct
- Both HAS_BYPASS_RELAY=0 and =1 variants compile cleanly via conditional compilation
- No behavioral gaps. task-177 tracks runtime relay configurability separately.
<!-- SECTION:FINAL_SUMMARY:END -->
