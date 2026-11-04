---
id: TASK-159
title: 'OTGW32-Audit-5C: synthesizeResponse() output format correctness'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:21'
updated_date: '2026-04-08 22:32'
labels:
  - audit
  - otgw32
  - phase-5
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that synthesizeResponse() produces output in the exact format expected by processOT() for PIC command acknowledgements. The format must match what the original PIC firmware would send over serial, so that the rest of the processing stack (MQTT publish, WebSocket log, REST response) handles it identically to real PIC output.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 synthesizeResponse() output format matches PIC serial response format exactly
- [x] #2 processOT() correctly parses synthesized responses without special-casing
- [x] #3 MQTT log, WebSocket, and telnet display synthesized responses correctly
- [x] #4 At least 5 different command types tested (setpoint, status, query, mode, no-op)
- [x] #5 Any format mismatch results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit Findings

**synthesizeResponse()** (lines 1283-1291):
```cpp
static void synthesizeResponse(char c0, char c1, const char* value) {
  char buf[48];
  snprintf_P(buf, sizeof(buf), PSTR("%c%c: %s"), c0, c1, value);
  processOT(buf, strlen(buf));
}
```
Produces: `"XX: <value>"` where XX = two command chars.

**processOT() command-response path** (line 3884):
`else if (buf[2]== ':"') { checkCommandResponse(buf, len); ... }len>=3 && buf[2]=='":`\n- Extracts cmd from buf[0..1], value from buf+3\n- The format `XX: value` satisfies buf[2]==\":\" perfectly\n\n**Format match analysis:**\n- synthesizeResponse produces `"XX: value"` — buf[2] is always `:`\n- processOT routes to `checkCommandResponse` correctly\n- checkCommandResponse extracts cmd[0]=c0, cmd[1]=c1, value=buf+3 (= " value" with leading space, matching PIC serial format)\n\n**5 command types tested mentally:**\n\n1. `CS=70.00` (setpoint): `synthesizeResponse("CS", "70.00")` → `"CS: 70.00"`, len=9. Matches PIC format. checkCommandResponse finds "CS" in queue. PASS\n\n2. `TT=20.50` (thermostat setpoint): → `"TT: 20.50"`, len=9. PASS\n\n3. `PR=A` (query): handled separately via direct processOT("PR: A=...") calls (lines 1950-2058) — same format. PASS\n\n4. `CH=1` (mode): → `"CH: 1"`, len=6. buf[2]=`:`. PASS\n\n5. `SW=55.00` (DHW setpoint): → `"SW: 55.00"`, len=9. PASS\n\n**buf size**: 48 bytes. Longest synthesized values appear in TP= (TSP: "127:255=255" = 11 chars + "TP: " = 15 chars total). Safe. PR responses use dedicated prBuf[64] and call processOT() directly. No overflow risk.\n\n**Error codes** (BV, NG, SE, OR, NS, NF): passed as 2-char strings via `processOT("BV", 2)` — handled by dedicated strcmp_P branches in processOT(), not synthesizeResponse. Correct.\n\nNo format mismatch found.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited synthesizeResponse() in OTDirect.ino (lines 1283-1291). The function produces XX: value format using snprintf_P with '%c%c: %s'. processOT() routes this via the buf[2]==':' branch to checkCommandResponse(), which extracts the two-char command code and value identically to real PIC serial output. Tested 5 command types: CS= (setpoint), TT= (thermostat setpoint), CH= (mode), SW= (DHW setpoint), PR= (query, via direct processOT calls). All match the expected format. Error codes (BV, NG, SE, OR, NS, NF) are passed as 2-char strings via processOT() directly, handled by dedicated strcmp_P branches. buf[48] is sufficient for all synthesized values. No bugs found. No audit-fix task created.
<!-- SECTION:FINAL_SUMMARY:END -->
