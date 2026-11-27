---
id: TASK-3131
title: >-
  Docs C3: flash layout for ADR-298 (safeboot2, USB migration, rollback) in the
  same commit as the table change
status: To Do
assignee: []
created_date: '2026-11-27T18:42:00Z'
labels: []
milestone: 2.0.0
dependencies:
  - TASK-3128
priority: high
ordinal: 1182000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Documentation part of the ADR-298 table change; lands in the same commit as the table change task (rides beta N+1). Covers the C4/flash-layout docs, the update and recovery guides and the manual chapters that state partition offsets or sizes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every doc that states app0 or LittleFS offset/size or the layout name says 2304K/704K and safeboot2; grep for 0x230000, 0x330000, 768K, 2240K lists each hit and its fix or why it stays (old-layout context)
- [ ] #2 Update/recovery guide: the one-time --migrate step (settings saved first, NVS kept), what an unmigrated unit shows (boot warning, partition_layout mismatch, /update blocked), rollback only with the old release's full merged image
- [ ] #3 docs/MANUAL.md regenerated if chapters change (test_manual_generated green); tests/test_docs_no_em_dash.py green
- [ ] #4 Reviewed by otgw-architect against ADR-298 before the table commit is pushed
<!-- AC:END -->
