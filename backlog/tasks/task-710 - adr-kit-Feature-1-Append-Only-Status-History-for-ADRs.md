---
id: TASK-710
title: 'adr-kit Feature 1: Append-Only Status History for ADRs'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 13:36'
updated_date: '2026-05-26 17:13'
labels:
  - adr-kit
  - phase-1
  - week-1
  - foundation
  - backend
milestone: v0.14.0
dependencies: []
references:
  - templates/adr-template.md
  - bin/adr-judge
  - bin/adr-lint
  - IMPLEMENTATION_STATUS_v0.14-v0.15.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add immutable status tracking to ADR template and implement parsing/appending logic in adr-judge. This is the foundation for retirement detection and audit trails.

PLAN - WEEK 1 (v0.14.0 Phase 1):

1. Template Update (DONE):
   - templates/adr-template.md updated with status_history YAML array
   - Each entry: {date, status, changed_by, reason, changed_via}
   - 5 fields documented with examples
   - Auto-migration strategy for v0.13 ADRs

2. Implement parse_status_history() in bin/adr-judge:
   - Parse YAML list from frontmatter
   - Handle missing section (backward compat with v0.13)
   - Return chronological list of history entries
   - Support both new YAML format and legacy formats

3. Implement append_to_status_history() in bin/adr-judge:
   - Takes ADR file path and new status entry
   - Never overwrites existing entries (true append-only)
   - Auto-creates status_history section if missing
   - Validates entry structure before appending
   - Returns True on success, False on error

4. Add Audit Gate to bin/adr-lint:
   - Validates status_history is append-only (no rewrites)
   - Validates all entries have required fields
   - Validates status values consistent with Status header
   - Warns on broken chains (future dates, missing transitions)
   - Position: after Completeness gate, before Evidence

5. Auto-Migration v0.13 → v0.14:
   - When adr-judge reads v0.13 ADR (no status_history)
   - Extract current status from Status header
   - Create initial entry: {date: today, status: extracted, changed_by: auto-migration, reason: v0.13→v0.14 upgrade, changed_via: adr-kit}
   - Append to ADR on next judge invocation
   - Track migration in audit log (optional)

6. Testing (20+ test cases):
   - Parse valid status_history (multiple entries)
   - Parse missing status_history (backward compat)
   - Append new entry to existing history
   - Append to missing history (auto-create)
   - Validate status_history structure
   - Detect invalid entries (missing fields, bad dates, future dates)
   - Auto-migration from v0.13
   - Status consistency checks (Audit gate)
   - Edge cases: empty history, duplicate dates, timezone handling
   - And 11 more edge cases

DELIVERABLES:
✓ templates/adr-template.md (status_history section + docs)
→ bin/adr-judge (parse + append functions)
→ bin/adr-lint (Audit gate)
→ tests/test_adr_status_history.py (20+ cases)

BACKWARD COMPATIBILITY:
- v0.13 ADRs work without modification
- Auto-migration silent on first run
- No breaking changes to existing workflows
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Status history field added to adr-template.md with YAML structure documented
- [ ] #2 parse_status_history() function implemented in adr-judge with backward compatibility
- [ ] #3 append_to_status_history() function implemented and never overwrites existing entries
- [ ] #4 Auto-migration code converts v0.13 ADRs on first judge run without data loss
- [ ] #5 20+ test cases passing (test_adr_status_history.py)
- [ ] #6 All existing tests still pass
- [ ] #7 Documentation updated with status_history examples
- [ ] #8 Performance verified: parse <50ms, append <100ms on 30-ADR set
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-26: Picked up by @codex for Phase 1 execution. Implementation target is the standalone repo D:\Users\Robert\Documents\GitHub\RvdB\adr-kit on branch v0.14-dev. Initial inspection found template scaffolding present, but bin/adr-judge parse/append/migration support, bin/adr-lint audit gate, and real status-history tests are not yet implemented.

Implementation decision: the drafted silent auto-migration on every pre-commit judge invocation is unsafe because judging staged content must not silently create unstaged ADR modifications. Phase 1 will provide explicit 'adr-judge --migrate-status-history' migration, preserving normal judge runs as read-only while meeting the no-data-loss upgrade objective.
<!-- SECTION:NOTES:END -->
