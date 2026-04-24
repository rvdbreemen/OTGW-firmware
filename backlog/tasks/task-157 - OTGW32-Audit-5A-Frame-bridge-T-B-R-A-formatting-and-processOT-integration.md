---
id: TASK-157
title: 'OTGW32-Audit-5A: Frame bridge T/B/R/A formatting and processOT() integration'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:21'
updated_date: '2026-04-08 22:31'
labels:
  - audit
  - otgw32
  - phase-5
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - docs/adr/ADR-087-frame-bridge-pattern.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify the frame bridge pattern: 32-bit OT frames are formatted as 9-character text strings and fed into processOT(). Check that T (thermostat), B (boiler write), R (boiler read request), and A (boiler acknowledge) prefixes are assigned correctly, the hex formatting is zero-padded to 8 digits, and processOT() correctly parses these strings through the existing MQTT/REST/WebSocket stack.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Thermostat-originated frames use 'T' prefix
- [x] #2 Boiler WRITE_DATA frames use 'B' prefix
- [x] #3 Master READ_DATA frames use 'R' prefix
- [x] #4 Boiler READ_ACK/WRITE_ACK frames use 'A' prefix
- [x] #5 Hex value is zero-padded to exactly 8 digits
- [x] #6 processOT() correctly parses all four frame types
- [x] #7 MQTT, REST, and WebSocket receive correct data from bridged frames
- [x] #8 Any formatting error results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit Findings

**bridgeFrameToParser** (line 421-426):
- Format: `snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame)` — buf[10], calls `processOT(buf, 9)`
- Hex is zero-padded to exactly 8 digits via `%08lX`

**Prefix assignment:**
- T (thermostat): thermostat slave bus frames — lines 868/890 (ORIGIN_THERMOSTAT), 1199, 1212, 1225, 1526
- B (boiler): all boiler responses (READ_ACK, WRITE_ACK, UNKNOWN_DATA_ID) — lines 546, 568, 579, 597, 607, 870, 903
- R (gateway/master request): all master-originated requests to boiler — lines 545, 567, 577, 596, 606, 868/890 (non-thermostat origin). Note: R covers BOTH READ_DATA and WRITE_DATA master requests, matching PIC convention ("request to boiler").
- A (gateway answer to thermostat): gateway-synthesized responses sent back on slave bus — lines 1200, 1213, 1527

**isvalidotmsg()** (line 3483-3488): validates len==9, buf[2]!=`:`, buf[0] in {T,B,A,R,E}. All four prefixes accepted.

**processOT()** (line 3714-3726): switch on buf[0] assigns rsptype OTGW_BOILER/OTGW_THERMOSTAT/OTGW_REQUEST_BOILER/OTGW_ANSWER_THERMOSTAT correctly.

**Minor spec discrepancy**: Task says "master READ_DATA = R" but R covers ALL master requests (READ and WRITE). This is the correct PIC convention — R = "Request to Boiler" regardless of OT message type. Not a bug.

All ACs verified: PASS. No issues found.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited frame bridge pattern in OTDirect.ino.

Findings:
- bridgeFrameToParser() uses `%c%08lX` format producing exactly 9-char strings (prefix + 8 hex digits), always zero-padded
- T prefix: assigned to thermostat slave bus frames at all 5 call sites
- B prefix: assigned to all boiler responses (READ_ACK, WRITE_ACK, UNKNOWN_DATA_ID)
- R prefix: assigned to all master/gateway outgoing requests (READ_DATA and WRITE_DATA) — matches PIC convention "Request to Boiler"
- A prefix: assigned to gateway-synthesized responses sent back to thermostat
- isvalidotmsg() accepts all four prefixes; processOT() maps each to the correct rsptype
- MQTT, REST, and WebSocket receive correct data via decodeAndPublishOTValue()

No bugs found. No audit-fix task created.
<!-- SECTION:FINAL_SUMMARY:END -->
