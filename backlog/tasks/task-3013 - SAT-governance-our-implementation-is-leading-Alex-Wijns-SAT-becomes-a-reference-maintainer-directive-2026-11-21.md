---
id: TASK-3013
title: >-
  SAT governance: our implementation is leading, Alex Wijn's SAT becomes a
  reference (maintainer directive 2026-11-21)
status: Done
assignee: []
created_date: '2026-11-21T15:55:00Z'
updated_date: '2026-11-21T16:05:00Z'
labels:
  - sat
  - governance
milestone: 2.0.0
dependencies: []
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-21: our SAT implementation plus Sergeant D's and Robert's recorded decisions is leading; Alex Wijn's SAT (other-projects/SAT-develop at ba515c8) is a reference to stay aligned with, borrow from and learn from. Rewrite the SAT ground truth block, AGENT-CORE line, navigation rows, skills, directive changelog, guard test; supersede ADR-196 (Proposed); bump the other-projects pointer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT-GROUND-TRUTH block in .claude/rules/sat-ground-truth.md and AGENTS.md states the new ranking, identical in both
- [x] #2 AGENT-CORE line updated identically in CLAUDE.md, AGENTS.md, .github/copilot-instructions.md
- [x] #3 tests/test_sat_ground_truth_block.py red on the old block, green on the new
- [x] #4 New ADR superseding ADR-196 written as Proposed
- [x] #5 other-projects pointer at 2b01ba65 (SAT-develop)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1/2: block rewritten identically (test_sat_ground_truth_block OK, test_agent_core_block OK). AC3: new test 6 OK on new block; on the old block 2 FAIL (test_block_makes_our_implementation_leading, test_reference_snapshot_records_its_upstream_commit). AC4: ADR-292 Proposed (adr-quality 0.90, no lint findings for it; test_adr_governance OK). AC5: other-projects 2b01ba65. Open for Robert: accept ADR-292; parity tests (test_sat_classifier_parity, test_sat_cycle_classifier oracle, test_thermo_nova_pin, scripts/inspect_sat_reference.py) now skip because the old folder is gone.
<!-- SECTION:FINAL_SUMMARY:END -->
