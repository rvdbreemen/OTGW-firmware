---
id: TASK-1310
title: >-
  SAT page: put end-user controls first, move Hot water and Burner cycle mode
  down or behind an advanced toggle
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T07:51:00Z'
updated_date: '2026-10-23T14:31:00Z'
labels:
  - webui
  - sat
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 433000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 07:40 (#alpha-testing, screenshot of the SAT Thermostat tab): the Hot water card (standalone DHW control + DHW setpoint) and the Burner cycle mode card sit prominently near the top although they are for edge cases; burner cycle mode is not something an end user should need. General principle from the tester: the interface targets OTGW users, not developers: end-user functions and information first, advanced info and configuration further down or hidden until opened. UI-only change (no SAT control behaviour change).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On the SAT Thermostat (simple) tab the dial, presets and the user-facing status come first; Hot water and Burner cycle mode move below the history or into a collapsed 'Advanced' section (or to the Control tab), decision recorded in the task (before/after screenshot)
- [x] #2 No control behaviour or setting changes: the same REST calls are made by the moved controls (tests/webui or bench check)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Decision (navigation concept A, docs/design/v2-navigation-concept.md): the SAT view buttons (Thermostat/Control/Technical) stay a cumulative level selector (settles the design note's open point; restyling as exclusive tabs would mislead). On the Thermostat view: dial + presets, user-facing status, then history; the Hot water card moves below the history into a collapsed <details class=grp> 'Hot water' (stays reachable for DHW boost); the Burner cycle mode card moves to the Control level only. Open state of the group follows rule 4 (user click stored, e.g. localStorage otgw-v2-satopen). No control/setting change: the moved controls keep their element ids/handlers, so the same REST calls happen. Files: data/v2.html (+ v2.js if cards are built in JS, v2.css only if needed; regenerate v2-bundle.css via build). Verification: tests/webui check that the moved controls still POST the same endpoints/bodies (DHW setpoint, standalone DHW, burner cycle) and that the Thermostat view shows dial before history before the Hot water group; Burner cycle hidden in Thermostat view, visible in Control; before/after screenshots desktop + 390 px (synthetic data). Design note updated (SAT row + open point resolved). Cross-line: no.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23: depends on the navigation concept decision in TASK-1312 (maintainer); skipped by the cycle until that decision is recorded, to avoid moving the SAT controls twice.

2026-10-23: navigation concept decided in TASK-1312 (option A: top tabs everywhere, collapsible grouped sections inside a page); this task is unblocked and follows that concept.

2026-10-23 16:40 evidence: facts on HEAD: history, Hot water and Burner cycle were all in .sat-l2 (Control level) -> the tester was on the Control view (stored view). Implemented: history into .sat-l1 after the dial, Hot water in a collapsed details.grp (stored on summary click, otgw-v2-satopen), Burner cycle alone in .sat-l2 (single-column row sat-row-1up after review). tests/webui/sat-page-order.test.mjs fix 15 PASS / 0 FAIL; HEAD 5 FAIL (order/visibility/group) while the request check passes on both: identical POSTs ['/api/v2/sat/settings/dhw_setpoint 45','/api/v2/sat/settings/dhw_enabled false','/api/v2/sat/mode pwm'] = same REST calls (AC2). Regression: v2-navigation 50, sat-status-pill 14, tret-pr-events 7, evaluate 0 fail. build.bat combo SUCCESS (littlefs 16:29); bench fs flash, settings 178/178 differing 0; real browser on the device: thermostat dial/hist/dhwGrp visible, group closed, order ok, burner hidden; control burner visible. Screenshots before/after in docs/evidence/task-1310/. Review (cavecrew): no blockers; half-width burner card fixed; live value in the Hot water summary left as a tester suggestion (YAGNI).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT Thermostat view puts end-user controls first: dial, presets, status, temperature history, then Hot water in a collapsed group; Burner cycle mode only on the Control view; the view buttons stay a cumulative level selector (design note open point resolved). Evidence: AC1 browser test (order, visibility, closed default, persistence) red on HEAD and green on the fix, before/after screenshots, device DOM check; AC2 identical POST log on HEAD and fix. Not verified: a real phone.
<!-- SECTION:FINAL_SUMMARY:END -->
