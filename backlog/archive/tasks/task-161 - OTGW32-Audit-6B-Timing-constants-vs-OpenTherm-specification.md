---
id: TASK-161
title: 'OTGW32-Audit-6B: Timing constants vs OpenTherm specification'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:21'
updated_date: '2026-04-08 22:31'
labels:
  - audit
  - otgw32
  - phase-6
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that the timing constants in OTDirect.ino (OT_STATUS_INTERVAL_MS=800, OT_TEMP_INTERVAL_MS=10000, OT_SLOW_INTERVAL_MS=60000, OT_WRITE_INTERVAL_MS=15000) comply with the OpenTherm specification requirements for master polling frequency. The OT spec mandates MsgID 0 every ≤1s; verify all intervals are within acceptable bounds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MsgID 0 status interval (800ms) meets OT spec requirement of ≤1000ms
- [x] #2 Temperature read interval (10s) is within acceptable OT spec range
- [x] #3 Write refresh interval (15s) keeps boiler values alive without excessive bus traffic
- [x] #4 Slow poll interval (60s) does not cause issues with boiler timeout behavior
- [x] #5 Intervals match OT-Thing reference implementation
- [x] #6 Any out-of-spec timing results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit Findings

### OT_STATUS_INTERVAL_MS = 800ms (MsgID 0)
- OT spec: MsgID 0 must be sent by master ≤1000ms (1s).
- 800ms is within spec. OT-Thing also uses 800ms.
- PASS.

### OT_TEMP_INTERVAL_MS = 10000ms (10s)
- Temperatures (Tboiler, Tret, Tdhw, etc.) are not time-critical for the boiler control loop.
- The OT spec imposes no upper interval limit on non-mandatory MsgIDs.
- 10s provides timely dashboard updates without unnecessary bus traffic.
- PASS.

### OT_WRITE_INTERVAL_MS = 15000ms (15s)
- Periodic write refresh keeps TSet, TrSet, MM, SW, SH alive.
- OT spec does not mandate a refresh interval; 15s is conservative and well within any practical boiler timeout.
- PASS.

### OT_SLOW_INTERVAL_MS = 60000ms (60s)
- Config/diagnostic MsgIDs that change rarely (slave config, fault codes, counters).
- No OT spec requirement on polling frequency for these; 60s is acceptable.
- PASS.

### Comparison with OT-Thing reference
- OT-Thing uses 800ms status, 10s temps, 60s slow — exact match.
- OT-Thing uses 30s for write refresh; OTGW32 uses 15s (more aggressive, but within spec).
- Minor difference on write refresh is safe and conservative.
- PASS.

### Verdict
- All timing constants are OT-spec compliant. MsgID 0 at 800ms satisfies the ≤1s mandatory requirement. No audit-fix needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit of timing constants vs OpenTherm specification — all PASS, no issues found.

Findings:
- OT_STATUS_INTERVAL_MS=800ms: within the mandatory ≤1000ms requirement for MsgID 0.
- OT_TEMP_INTERVAL_MS=10000ms: no spec constraint; 10s matches OT-Thing reference.
- OT_SLOW_INTERVAL_MS=60000ms: no spec constraint; 60s matches OT-Thing reference.
- OT_WRITE_INTERVAL_MS=15000ms: no spec constraint; 15s is more aggressive than OT-Thing (30s) but still safe and spec-compliant.
- Comment on MsgID 0 in schedule table correctly notes "mandatory, ≤1s".

No audit-fix task required.
<!-- SECTION:FINAL_SUMMARY:END -->
