---
id: TASK-1403
title: >-
  Gate for ADR-201: a host test that fails when combo detection persists a
  negative (no-PIC) verdict
status: To Do
assignee: []
created_date: '2026-10-25T20:37:00Z'
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
- [ ] #1 tests/ gate fails on a mutant that persists a no-PIC verdict, passes on HEAD
- [ ] #2 ADR-201 frontmatter gate names the test; ADR becomes binding; CLAUDE.md/AGENTS.md binding list updated
<!-- AC:END -->
