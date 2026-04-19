---
id: TASK-327
title: 'Write ADR-080: binding ADR rules must have a CI gate'
status: To Do
assignee: []
created_date: '2026-04-19 17:11'
labels:
  - architecture
  - meta
  - review-2026-04-18-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Meta-ADR triggered by the 2026-04-18 review retrospective. ADR-004 (no String in hot paths) was documented as binding but not enforced; 19 quiet violations accumulated in hot-path files. ADR-080 makes it a rule: any ADR with Status Accepted that declares a code-level pattern (not a system-level choice like HTTP-only) gets a corresponding check in evaluate.py or tests/ within one PR cycle. ADRs that cannot be reasonably automated get downgraded from binding to guideline with an explicit label. This prevents the drift pattern observed during review.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New docs/adr/ADR-080-*.md created, Status Accepted, cross-links to ADR-004 and ADR-051
- [ ] #2 CLAUDE.md 'ADR Guidelines' section references ADR-080 as the gate-enforcement rule
- [ ] #3 Existing binding ADRs audited: every pattern-level ADR has an evaluate.py or tests/ check, or is explicitly downgraded to guideline in its own header
<!-- AC:END -->
