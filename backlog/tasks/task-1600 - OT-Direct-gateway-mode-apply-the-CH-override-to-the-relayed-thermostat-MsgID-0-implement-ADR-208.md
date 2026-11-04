---
id: TASK-1600
title: >-
  OT-Direct gateway mode: apply the CH override to the relayed thermostat MsgID
  0 (implement ADR-208)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T05:48:00Z'
updated_date: '2026-11-04T07:31:00Z'
labels:
  - otdirect
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1238
priority: medium
ordinal: 741000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implements ADR-208 (Proposed, TASK-1238 option B): bit 0 of a relayed thermostat READ_DATA MsgID 0 gets otdEffectiveMasterFlags() (OTDirect.ino:761-785), parity recomputed, low byte passes; buildOverriddenReply() gets a MsgID 0 case that returns the boiler's slave status with the thermostat's original high byte (as the PIC statusreturn, gateway.asm:2745-2750).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test in the style of tests/test_otd_ch_override.py: relayed MsgID 0 with CH=0 while a CS override is held goes out with CH=1 (red on today's applyOverrides), reply carries the boiler low byte and the thermostat's original high byte; without an override the frame passes unchanged
- [x] #2 ADR-208 accepted by Robert before merge (open question: any CS override vs SAT-only answered)
- [x] #3 esp32-combo build, OTA on the OT-Direct bench unit, no regression in gateway-mode polling
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 Robert: ADR-208 accepted (any CS override, entry 264); implementation may start.

2026-11-04 implemented (sonnet agent; lead reran on the integration tree): applyOverrides() rewrites bit 0 of a relayed READ_DATA MsgID 0 in gateway mode via otdEffectiveMasterFlags (parity recomputed), buildOverriddenReply() MsgID 0 case returns the thermostat high byte + boiler low byte. tests/test_otd_status_relay.py old 4/9 fail, fix 9 OK; otd_ch_override 8, sat_command_delivery 16, boiler_unsupported_origin PASS, lowload contract 32 OK (agent), evaluate 0 failed (ADR-208 require_pattern satisfied). Tag beta.68. AC3 bench: no thermostat on the OT-Direct unit; check no regression in gateway-mode polling.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct gateway mode applies the CH override to the relayed thermostat MsgID 0 while any CS override is held, and returns the boiler status with the thermostat's own high byte (ADR-208 Accepted, entry 264, confirmed by Sergeant D entry 265). Evidence: tests/test_otd_status_relay.py old 4/9 fail, fix 9 OK; otd_ch_override 8 OK; beta.68 OTA on the OT-Direct bench unit: SAT CS resend through the command ring every 5 s, no errors, no crashlog. Not tested with a real thermostat (TASK-1601 field capture).
<!-- SECTION:FINAL_SUMMARY:END -->
