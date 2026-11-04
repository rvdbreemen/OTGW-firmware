---
id: TASK-1403
title: >-
  Gate for ADR-201: a host test that fails when combo detection persists a
  negative (no-PIC) verdict
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T20:37:00Z'
updated_date: '2026-11-01T01:50:00Z'
labels:
  - adr
  - combo
  - tests
milestone: 2.0.0
dependencies: []
ordinal: 542000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-201 Open Question 2, Robert 2026-10-25: yes, add a gate. Host test that cuts the real board-detection block (OTGW-firmware.ino ~369-451) and the second persist site (OTGW-Core.ino:5647-5650) and asserts: auto + PIC found -> iBoardMode 1/3 persisted; auto + no PIC -> OT-Direct this boot, iBoardMode stays 0, no writeSettings; forced 2 + PIC -> corrected to Classic; nothing writes 2 on its own. Red on a mutant that persists 2. Then name the gate in ADR-201 (binding per ADR-080).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 tests/ gate fails on a mutant that persists a no-PIC verdict, passes on HEAD
- [x] #2 ADR-201 frontmatter gate names the test; ADR becomes binding; CLAUDE.md/AGENTS.md binding list updated
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Gate tests/test_board_detect_persist_gate.py compiles the real board-detection blocks (OTGW-firmware.ino forced-2 recovery / forced PIC / auto detection with 3 retries and the writeSettings guard; OTGW-Core.ino banner-handler persist site), cut by string markers, with stubs only for hardware probes (detectPIC scripted), initOTDirect, writeSettings (records the persisted value), delay, LEDs and logging. It asserts ADR-201: auto + PIC -> 1 (or 3 on the Pro) written once; auto + no PIC -> OT-Direct this boot, iBoardMode 0, no write; forced 2 + PIC -> corrected and written; nothing ever persists 2 (all mode/PIC/board combinations of both sites). MutantGate builds a mutant at test time (no-PIC branch sets iBoardMode = 2) and requires it to be caught.
Evidence: run on HEAD 7/7 OK (MSVC); the same mutant as a file via OTGW_INO fails 2 (test_auto_no_pic_runs_otdirect_without_persisting, test_nothing_persists_otdirect_on_its_own). ADR-201 frontmatter binding: true, gate names the test; CLAUDE.md and AGENTS.md binding lists updated; tests/test_adr_governance.py 15/15.
Not covered: setup() call order around the blocks; the real detectPIC() (stubbed).
<!-- SECTION:FINAL_SUMMARY:END -->
