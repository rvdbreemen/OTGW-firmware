---
id: TASK-335
title: 'ADR-079 audit: review OTGW-Core.h (619 lines) for type declarations'
status: To Do
assignee: []
created_date: '2026-04-19 18:34'
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
- [ ] #1 Every enum/struct in OTGW-Core.h audited and classified as state-adjacent vs interface vs protocol-primitive
- [ ] #2 State-adjacent types merged into OTBustypes.h OR new OTCoretypes.h as appropriate
- [ ] #3 OTGW-Core.h retains only function declarations, macros, and inline helpers
- [ ] #4 Both platforms build clean; aggregate OTGWState struct sizes byte-identical pre/post refactor
<!-- AC:END -->
