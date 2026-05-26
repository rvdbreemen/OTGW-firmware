---
id: TASK-709
title: 'Feature 1: Append-Only Status History for ADRs'
status: To Do
assignee: []
created_date: '2026-05-26 11:20'
labels:
  - phase-1
  - core-feature
  - backend
milestone: v0.14.0
dependencies: []
references:
  - templates/adr-template.md
  - bin/adr-judge
  - bin/adr-lint
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add immutable status tracking to ADR template and implement parsing/appending logic in adr-judge. This is the foundation for retirement detection and audit trails.

Deliverables:
- Update templates/adr-template.md with status_history field (YAML array: date, status, changed_by, reason, changed_via)
- Enhance bin/adr-judge with parse_status_history() and append_to_status_history() functions
- Auto-migrate v0.13 ADRs to v0.14 format on first judge run
- Implement status_history validation in bin/adr-lint (new Audit gate)
- Create comprehensive test suite (20+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Status history field added to adr-template.md with YAML structure documented
- [ ] #2 parse_status_history() function implemented in adr-judge with backward compatibility
- [ ] #3 append_to_status_history() function implemented and never overwrites existing entries
- [ ] #4 Auto-migration code converts v0.13 ADRs on first judge run without data loss
- [ ] #5 Audit gate in adr-lint validates status_history is append-only and consistent
- [ ] #6 20+ test cases passing (test_adr_status_history.py)
- [ ] #7 All existing tests still pass
- [ ] #8 Documentation updated with status_history examples
<!-- AC:END -->
