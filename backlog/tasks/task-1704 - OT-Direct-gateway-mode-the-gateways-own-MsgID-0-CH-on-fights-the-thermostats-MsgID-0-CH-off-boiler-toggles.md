---
id: TASK-1704
title: >-
  OT-Direct gateway mode: the gateway's own MsgID 0 (CH on) fights the
  thermostat's MsgID 0 (CH off), boiler toggles
status: Done
assignee: []
created_date: '2026-11-07T17:07:00Z'
updated_date: '2026-11-07T17:34:00Z'
labels:
  - bug
  - otdirect
  - github
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 817000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs in #alpha-testing 2026-11-07 15:10 UTC (msg 1568643857133080729, log msg 1568644299887743006): thermostat at 17 C, room warmer, boiler keeps toggling between calling for heat and standby. His OT log (2069 lines, 16:06-16:12): thermostat T80000200 Status [-D---W--] (CH off) 137x, gateway's own R80000100 Status [C----W--] (CH on) 299x; slave status alternates [--------] / [-C------]. Code: loopOTDirect runs scheduleMasterRequest() in every mode except monitor (OTDirect.ino:1972-1976); its MsgID 0 entry sends buildStatusRequest() with otMasterStatusFlags (bit 0 default on), so in gateway mode the gateway originates its own status frames between the relayed thermostat frames. A PIC in gateway mode relays the thermostat's MsgID 0 and does not send its own. Fix direction: in gateway mode while a thermostat is connected the scheduler does not send its own MsgID 0 (the relayed thermostat frame with the ADR-208 CH= override is the only status frame); the offline probe and the thermostat-timeout setback keep working.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real scheduler: gateway mode with a connected thermostat sends no own MsgID 0 (red on old code), master mode and gateway-without-thermostat still do
- [ ] #2 Bench .61 cannot reproduce (no thermostat): tjfs confirms on his unit that the boiler stays off with the thermostat at 17 C, or a replay of his log shows no conflicting MsgID 0
- [ ] #3 ADR-208 CH= override on the relayed thermostat frame unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Duplicate of TASK-1701 (same tjfs report, filed 17:04 UTC by another session, In Progress there). My analysis for 1701: tjfs's log has thermostat T80000200 Status [-D---W--] 137x vs gateway R80000100 [C----W--] 299x; loopOTDirect runs scheduleMasterRequest() in every mode except monitor (OTDirect.ino:1972-1976) and its MsgID 0 entry sends buildStatusRequest().
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as duplicate of TASK-1701; analysis added there via notes.
<!-- SECTION:FINAL_SUMMARY:END -->
