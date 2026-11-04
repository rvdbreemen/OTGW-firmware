---
id: TASK-171
title: 'OTGW32-Audit-9B: Review ADR-064 and ADR-087 — operating modes and frame bridge'
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
  - docs/adr/ADR-064-ot-direct-operating-mode-architecture.md
  - docs/adr/ADR-087-frame-bridge-pattern.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review ADR-064 (OT Direct operating mode architecture) and ADR-087 (frame bridge pattern) against the current implementation. Verify all five modes (gateway/monitor/bypass/master/loopback) are documented, the frame bridge T/B/R/A format is correct, and the synthesizeResponse() flow matches the documented pattern.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-064 documents all 5 operating modes with accurate descriptions
- [x] #2 ADR-087 frame format (T/B/R/A prefix + 8-digit hex) matches implementation
- [x] #3 synthesizeResponse() output flow matches ADR-087 description
- [x] #4 Both ADRs are updated if implementation details have changed
- [x] #5 Any gap between ADR and code results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-064 vs OTDirect.ino review:

Enum naming: ADR-064 shows OT_MODE_* prefix; actual code uses OTD_MODE_* prefix (OTD_MODE_GATEWAY=1, OTD_MODE_MONITOR=2, OTD_MODE_BYPASS=0, OTD_MODE_MASTER=3, OTD_MODE_LOOPBACK=4). Also enum values differ: ADR has GATEWAY=0 as default; code has BYPASS=0, GATEWAY=1. This is a documentation mismatch.

setOTDirectMode(): implemented; calls resetTransientState() on every mode change (not in ADR). ADR says "centralized hardware reconfiguration atomically" — code matches this intent.

Convenience flags: ADR-064 says boolean variables (otBypassActive, otMonitorMode, otMasterMode). Code uses macros (#define IS_BYPASS_MODE(), IS_MONITOR_MODE(), IS_MASTER_MODE(), IS_LOOPBACK_MODE()) instead. Same semantic purpose, different implementation. The state struct also has bBypassActive/bMonitorMode/bMasterMode.

Persistence: ADR says "stored in OTDirectSettingsSection and restored at boot". Code does this: settings.otd.iMode read in initOTDirect(), written via updateSetting("OTDmode",...) in setOTDirectMode().

Hardware transitions: relay, step-up — implemented for each mode case.

ADR-087 vs OTDirect.ino review:

bridgeFrameToParser(char prefix, unsigned long frame): matches ADR exactly. Uses PSTR("%c%08lX") format.

Origin chars T/B/R/A: all present and used correctly in sendMasterRequestAsync() and handleMasterResponse().

synthesizeResponse(): implemented as two overloads (c0+c1+value, or cmd+value). ADR says it formats "XX: value" matching PIC format — matches code.

Call sites: ISR callbacks → loopOTDirect() → bridgeFrameToParser(). Matches ADR description.

Loopback mode: not mentioned in ADR-087 but uses same bridgeFrameToParser() path — consistent.

Discrepancy found: ADR-064 enum values are wrong (GATEWAY=0 in ADR vs GATEWAY=1 in code). ADR needs updating.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-064 and ADR-087 review against OTDirect.ino:

ADR-064 discrepancies found and fixed:
- Enum prefix was OT_MODE_* in ADR but OTD_MODE_* in code — corrected
- Enum numeric values were wrong: ADR had GATEWAY=0; code has BYPASS=0, GATEWAY=1 — corrected with rationale note
- Convenience flags were documented as boolean variables; code uses macros (#define IS_*_MODE()) — ADR updated to show actual macros
- setOTDirectMode() also calls resetTransientState() on every mode change (not in ADR) — noted as implementation detail

ADR-087: all aspects match implementation exactly.
- bridgeFrameToParser() signature, format string, call sites all correct
- synthesizeResponse() two overloads match ADR description
- Origin chars T/B/R/A used correctly in code

No audit-fix task needed: all discrepancies were documentation errors now corrected.
<!-- SECTION:FINAL_SUMMARY:END -->
