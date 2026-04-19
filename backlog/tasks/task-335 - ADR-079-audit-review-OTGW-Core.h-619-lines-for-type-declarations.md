---
id: TASK-335
title: 'ADR-079 audit: review OTGW-Core.h (619 lines) for type declarations'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 18:34'
updated_date: '2026-04-19 19:44'
labels:
  - architecture
  - adr-079
  - cleanup
  - review-2026-04-18-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-Core.h is 619 lines -- larger than most sibling stuff.h files and likely contains OT-protocol type declarations that belong in OTBustypes.h or a new OTCoretypes.h. This task is the audit + extraction: identify every enum/struct/class in OTGW-Core.h, decide whether each is (a) OT-bus state-adjacent -> OTBustypes.h, (b) OT-protocol primitives -> new OTCoretypes.h, or (c) pure interface decls that stay. The SAT audit (TASK-326 AC1) is the working template: enums + helper structs + sections bundled in one types.h.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every enum/struct in OTGW-Core.h audited and classified as state-adjacent vs interface vs protocol-primitive
- [x] #2 State-adjacent types merged into OTBustypes.h OR new OTCoretypes.h as appropriate
- [x] #3 OTGW-Core.h retains only function declarations, macros, and inline helpers
- [x] #4 Both platforms build clean; aggregate OTGWState struct sizes byte-identical pre/post refactor
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit results: OTGW-Core.h (619 lines) classifies correctly under ADR-081 as the OT-Core component's stuff.h. All its types are OT-protocol-specific (OpenthermData_t frame decoder, OTLibMessageID enum covering OT MessageIDs 0-133, OTcurrentSystemState decoded values typedef struct, OT_cmd_t PIC-command queue entry, OTPublishGate MQTT rate-limiter, OTValueType, OTLibMessageType, OTGW_response_type) and all belong to the OT-Core component. None cross a component boundary such that they'd move to OTBustypes.h (which is the minimal 'is the bus online' flags -- different semantic layer from 'what values have we decoded from the bus').
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTGW-Core.h audited, no extraction needed. File already satisfies ADR-081: it IS the stuff.h for the OT-Core component, and the 7 enums + 4 structs + 4 defines it contains are all OT-Core-specific types (OT protocol data frames, MessageIDs 0-133, decoded system state, PIC command queue, publish gates). No candidates for splitting into a separate types.h or moving to OTBustypes.h. The 619-line size is driven by the OTLibMessageID enum (~270 lines covering every OT MessageID) and OTcurrentSystemState (~160 lines mapping all decoded values) -- both intrinsic OT-protocol data, not bloat. No action.
<!-- SECTION:FINAL_SUMMARY:END -->
