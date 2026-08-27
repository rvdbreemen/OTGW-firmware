---
id: TASK-1094
title: Reconcile the 1.x DHW water total with the 2.0.0 peer decision
status: To Do
assignee: []
created_date: '2026-08-27 04:26'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 193000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-090 Must #2 mandates the same accumulation method on the 1.x and 2.0.0 lines so both firmwares report the same total for the same boiler. The 1.x implementation (TASK-1091, TASK-1093) was built without reading the 2.0.0 peer record, which lives in the other worktree. ADR-094 carries this as a known open point.

Read the peer record and compare the two implementations on the points that change the number a user sees: whether the peer persists the counter, what its gap rule is and what cap value it uses, and whether it applies each sample to the preceding or the following interval. Then either align the two or record, on both lines, why they deliberately differ.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The 2.0.0 peer record is identified by number and its decision on persistence, gap rule and cap value is summarised against the 1.x implementation
- [ ] #2 Any divergence that changes the reported total is either fixed on one line or recorded as deliberate in a decision record on both lines
- [ ] #3 ADR-094's open question on 2.0.0 parity is answered with a pointer to the outcome
<!-- AC:END -->
