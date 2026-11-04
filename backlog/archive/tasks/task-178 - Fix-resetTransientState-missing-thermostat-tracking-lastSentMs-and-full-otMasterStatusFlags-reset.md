---
id: TASK-178
title: >-
  Fix: resetTransientState() missing thermostat tracking, lastSentMs, and full
  otMasterStatusFlags reset
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:31'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
resetTransientState() in OTDirect.ino has three gaps found in audit TASK-166:\n1. otThermostatSeen and otLastThermostatMs are not cleared, leaving stale thermostat connectivity state after GW=R reset.\n2. otSchedule[].lastSentMs is not zeroed, so slow-poll items (60s interval) are not forced to re-poll immediately after reset.\n3. otMasterStatusFlags is only partially cleared (~0x44 removes cooling+DHW-block), but CH2 enable (bit4) and DHW enable (bit1) runtime bits are left set. Should reset to 0x01 (CH=1 only default).\n\nA GW=R reset must return the system to the same state as a cold boot from the OT bus perspective.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otThermostatSeen is set to false in resetTransientState()
- [x] #2 otLastThermostatMs is set to 0 in resetTransientState()
- [x] #3 All otSchedule[].lastSentMs entries are set to 0 in resetTransientState()
- [x] #4 otMasterStatusFlags is reset to 0x01 (preserving persisted summer mode from settings.otd.bSummerMode only)
- [ ] #5 GW=R command triggers the same state as cold boot from OT bus perspective
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed resetTransientState() to fully reset all transient state on GW=R.

Added 4 missing resets:
1. otThermostatSeen = false
2. otLastThermostatMs = 0
3. Zero all otSchedule[].lastSentMs (forces immediate re-poll after reset)
4. otMasterStatusFlags reset to 0x01 (CH enable only), with summer mode re-applied from settings.otd.bSummerMode

Previously GW=R left stale thermostat state and partial status flag bits, causing inconsistent post-reset behavior.
<!-- SECTION:FINAL_SUMMARY:END -->
