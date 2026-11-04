---
id: TASK-170
title: 'OTGW32-Audit-9A: Review ADR-063 — OTGW32 hardware support decision'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:24'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-9
milestone: m-1
dependencies: []
references:
  - docs/adr/ADR-063-otgw32-hardware-support.md
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Re-read ADR-063 and verify the implementation matches the documented decision. Check that all constraints, consequences, and related decisions listed in the ADR are still accurate given the current state of OTDirect.ino. Update the ADR if the implementation has evolved beyond what was originally decided.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-063 is read and compared against current OTDirect.ino implementation
- [x] #2 All architectural decisions in ADR-063 are still accurately reflected in code
- [x] #3 Any discrepancy between ADR and implementation is documented
- [x] #4 ADR is updated if implementation has evolved
- [x] #5 If a gap requires code change an audit-fix task is created
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-063 vs OTDirect.ino review:

- HW_MODE enum (UNKNOWN/PIC/OT_DIRECT/DEGRADED): matches ADR exactly
- isPICEnabled()/isOTDirectEnabled() guards: implemented correctly in OTGW-firmware.h
- Frame bridge pattern (snprintf_P + processOT): matches ADR, bridgeFrameToParser() uses correct format
- OTDirectMode enum: ADR-063 draft shows OT_MODE_* prefix; actual code uses OTD_MODE_* prefix (consistent with ADR-064)
- File size: ADR says ~200 lines, actual is 2423 lines — ADR note is wildly out of date
- Build targets: platformio.ini [env:otgw32] and [env:esp8266] match ADR
- HAS_DIRECT_OT / HAS_PIC flags in boards.h: match ADR
- Convenience flags: ADR-063 does not document them (covered by ADR-064)
- Protocol handshake (MsgID 2/3/124/126) at boot: not mentioned in ADR-063 but present in code — minor undocumented enhancement
- OT bus probe (probeOTBus): not in ADR, implements the "runtime detection" concept from ADR

Conclusion: ADR-063 is accurate for its architectural decisions. Two notes to update: (1) OTDirect.ino is now 2400+ lines not ~200; (2) OTDirectMode enum prefix is OTD_MODE_* not OT_MODE_* as shown in draft code.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-063 review against OTDirect.ino (2423 lines):

Matches:
- HW_MODE_* enum (UNKNOWN/PIC/OT_DIRECT/DEGRADED) is accurate
- isPICEnabled()/isOTDirectEnabled() guards implemented correctly
- Frame bridge format (snprintf_P PSTR("%c%08lX") + processOT) matches ADR
- HAS_DIRECT_OT / HAS_PIC compile-time flags in boards.h match ADR
- Build targets [env:esp8266] / [env:otgw32] match ADR
- Runtime OT bus probe and state.hw.eMode assignment present

Updated in ADR:
- OTDirect.ino size estimate corrected from ~200 to ~2400 lines

No audit-fix task needed: all discrepancies were documentation-only.
<!-- SECTION:FINAL_SUMMARY:END -->
