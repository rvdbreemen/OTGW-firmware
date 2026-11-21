---
id: TASK-2982
title: >-
  Web UI: the statically active tab carries no aria-current on the first page
  open (Monitor Log, Advanced PIC, SAT Thermostat) (TASK-2958 follow-up)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T17:12:00Z'
updated_date: '2026-11-21T00:01:00Z'
labels:
  - webui
  - a11y
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1112000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench finding by otgw-bench-tester on .63, beta.404+9604698 (TASK-2958 PASS 8/8, OTGW-field-data/bench-63/2026-11-20_task2958-a11y/): on the first open of Monitor no .subtab has aria-current although Log is visibly active. Cause: v2.html marks the default tab with class=active only (Monitor Log :492, Advanced PIC :825, SAT view Thermostat :350); setAriaCurrent() runs in showTab/showAdvTab/satShowView, which only run on a click (the main nav Home already carries aria-current=page in the HTML). Fix: aria-current=true on those three in the HTML, so the markup's initial state matches its class.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On a fresh page load each group (main nav, SAT views, Monitor tabs, Advanced tabs) has exactly one aria-current, before any click; a11y-structure.test.mjs asserts the initial state, red on beta.404, green on the fix
- [ ] #2 After page and view changes still exactly one per group (existing test stays green); full tests/webui sweep green
- [ ] #3 Short bench on a unit after the build (otgw-bench-tester)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-webui. Scope widened by the coordinator to the otgw-ux-designer D-list on TASK-2958: D1 initial aria-current (this task), D2 main nav ring clipped by .switcher overflow-x, D3 rail Enter not deterministic in the bench run. Changes: v2.html aria-current=true on the three statically active tabs (SAT view simple, Monitor mlog, Advanced pic); v2.js showPage('monitor') without a stored tab sets aria-current on the tab marked active (with a stored tab showTab already did); v2.css .seg:focus-visible{outline-offset:-2px} (colour stays from the one :focus-visible rule).

Evidence: tests/webui/a11y-structure.test.mjs extended (initial-state group check; D2 outline-offset on a focused .seg; D3 keyboard-only rail: Tab from one rail button to the next, Enter opens it, Tab + Space opens the next; D1 Monitor entry with no stored tab and with stored mvalues). On origin/dev data (beta.404): 4 fail = initial state, D2, Monitor default; the stored-tab case and D3 pass there too (no D3 bug: the bench facts-b miss looks like script timing). On the fix: 12/12 green. Also checked by the test though not visible in the bench evidence: a sr-only <h2> as first child of each .page and aria-hidden on #satEnableLbl. Full tests/webui sweep: pending the coordinator's heavy slot (new rule 2026-11-21).
<!-- SECTION:NOTES:END -->
