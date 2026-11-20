---
id: TASK-2952
title: UX heuristic review of the v2 web UI (Home, SAT, Settings, Monitor)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T13:40:00Z'
updated_date: '2026-11-20T14:07:00Z'
labels:
  - ux
milestone: 2.0.0
dependencies: []
ordinal: 1089000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Heuristic review (Nielsen H1-H10, WCAG 2.2 AA) of the v2 web UI by otgw-ux-designer, assigned by the coordinator. Deliverable: docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md with findings, severity, effort, evidence and a proposed AC per finding. The coordinator turns findings into otgw-webui tasks; nothing in the review is a decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Review file lists every finding with heuristic or WCAG SC, severity 0-4, effort S/M/L, evidence (screenshot name or file:line, measured contrast) and a proposed AC
- [x] #2 Screens covered match the captured matrix stated in the review (live .63 read-only and the webui mock), and every gap is listed under Not verified
- [x] #3 Keyboard walkthrough done on the mock where the mock allows, with results in the review
- [ ] #4 Findings list delivered to the coordinator for triage into otgw-webui tasks
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 evidence (local, not in git, folder D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\): live .63 shots 12:34-12:45 (15 png); mock shots mock-*-390-light.png (home, sat-simple, sat-control, settings) and mock-*-1400-light.png (sat-control, sat-technical, monitor); contrast scans mock-contrast-light.txt and mock-contrast-dark.txt (27-149 text styles per view, numbers recomputed in Python); Tab walks mock-kbd-home-1400-light.txt and mock-kbd-sat-control-1400-light.txt; mock-a11y-sat-control.txt; mock-focus-nav-light/dark.png; mock-disabled-sat-control.txt. 0 JS exceptions in every mock run. Review: docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, 21 findings F1-F21, each with sev, effort, evidence and Proposed AC. AC1: review file. AC2: Evidence + Not verified sections match the captured matrix. AC3: keyboard walk on Home and SAT Control (F19, F20, F21). Screen reader not done (listed under Not verified).
<!-- SECTION:NOTES:END -->
