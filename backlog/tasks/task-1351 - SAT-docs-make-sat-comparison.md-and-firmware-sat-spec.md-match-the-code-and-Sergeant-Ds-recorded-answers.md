---
id: TASK-1351
title: >-
  SAT docs: make sat-comparison.md and firmware-sat-spec.md match the code and
  Sergeant D's recorded answers
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T16:27:00Z'
updated_date: '2026-10-24T16:30:00Z'
labels:
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 486000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #36 (audit IDs COLDCH-11, COLDCH-12, F16, F12(curve), PWM-17, CYC-12, CYC-13, STARTUP-10, MODS-12, DHW-11, S13) and #69 (ADR-069/071 wrong values, ADR-195 boundary/CH-rule wording): the reference docs contradict the code and present questions he already answered (entries 1-35) as open, so he could be asked twice. Docs only; Accepted ADRs are immutable: record ADR errors as a note in sat-comparison.md / a follow-up task, not by editing them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every item listed under audit #36 is corrected in sat-comparison.md / firmware-sat-spec.md with a citation (code file:line, entry N), checked by a second reader agent that opens each citation
<!-- AC:END -->
