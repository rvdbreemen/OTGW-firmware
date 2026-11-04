---
id: TASK-318
title: '[ARCH-L1] Reconcile state.otBus member name with struct docstring'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:24'
updated_date: '2026-04-19 07:23'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-firmware.h:291 struct OTBusState carries comment 'state.otgw' but member is 'otBus' (56 call sites). Breaks sister-section naming convention (otd, sat, pic, mqtt are short).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Pick one: either rename member back to otgw OR update struct docstring to state.otBus
- [x] #2 All call sites and docs consistent with the chosen name
- [x] #3 No behavior change
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision 2026-04-19 by Robert: keep the member name otBus. It reflects OT bus traffic semantically (as pic/otd/sat do for their own subsystems) and is the better choice over gateway-role naming. The 56 call sites stay unchanged; only the struct docstring is updated to match.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTGW-firmware.h:291: updated OTBusState struct docstring from 'state.otgw' to 'state.otBus' so the comment matches the actual member name. No rename; no behaviour change; 56 existing call sites unaffected. Naming rationale documented inline.
<!-- SECTION:FINAL_SUMMARY:END -->
