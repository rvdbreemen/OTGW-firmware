---
id: TASK-2982
title: >-
  Web UI: the statically active tab carries no aria-current on the first page
  open (Monitor Log, Advanced PIC, SAT Thermostat) (TASK-2958 follow-up)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T17:12:00Z'
updated_date: '2026-11-21T02:07:00Z'
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
- [x] #2 After page and view changes still exactly one per group (existing test stays green); full tests/webui sweep green
- [x] #3 Short bench on a unit after the build (otgw-bench-tester)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-webui. Scope widened by the coordinator to the otgw-ux-designer D-list on TASK-2958: D1 initial aria-current (this task), D2 main nav ring clipped by .switcher overflow-x, D3 rail Enter not deterministic in the bench run. Changes: v2.html aria-current=true on the three statically active tabs (SAT view simple, Monitor mlog, Advanced pic); v2.js showPage('monitor') without a stored tab sets aria-current on the tab marked active (with a stored tab showTab already did); v2.css .seg:focus-visible{outline-offset:-2px} (colour stays from the one :focus-visible rule).

Evidence: tests/webui/a11y-structure.test.mjs extended (initial-state group check; D2 outline-offset on a focused .seg; D3 keyboard-only rail: Tab from one rail button to the next, Enter opens it, Tab + Space opens the next; D1 Monitor entry with no stored tab and with stored mvalues). On origin/dev data (beta.404): 4 fail = initial state, D2, Monitor default; the stored-tab case and D3 pass there too (no D3 bug: the bench facts-b miss looks like script timing). On the fix: 12/12 green. Also checked by the test though not visible in the bench evidence: a sr-only <h2> as first child of each .page and aria-hidden on #satEnableLbl. Full tests/webui sweep: pending the coordinator's heavy slot (new rule 2026-11-21).

2026-11-21 full tests/webui sweep in the coordinator's heavy slot: 92 files, 435 pass, 0 fail. Build beta.405+d2a9aee OK (01:54). Bench by otgw-bench-tester on .63 (NAS headless Chromium, evidence OTGW-field-data/bench-63/2026-11-21_task2982-ariacurrent/ui/t2982-ui-63/facts.json): (1) version + ini diff 0 PASS; (3) .seg ring inside the button PASS; (2) PARTIAL: with otgw-v2-montab=mvalues and no click, the hidden Monitor group still marks Log until Monitor opens; on opening it moves to Values. Coordinator decision: accepted as a known limit (the group is hidden at load, so assistive technology does not read it). (4) Enter/Space open the rail category PASS, finding: focus lands on <body> afterwards, so the next Tab goes to the skip link. Probe on origin/dev data (beta.404) and on beta.405 both give activeElement BODY after Enter: pre-existing since TASK-2958 made the rail items buttons (renderSettings rebuilds the rail and drops the focused button), not caused by this task; coordinator decision: separate a11y task, this task pushes as is.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
aria-current is present from the first load on the statically active tabs (SAT Thermostat, Monitor Log, Advanced PIC) and follows the active Monitor tab when Monitor opens; the main nav focus ring is drawn inside the .seg (outline-offset -2px) so the scrolling .switcher no longer clips it; a keyboard-only rail test (Tab, Enter, Space) is in a11y-structure. AC1: a11y-structure red on beta.404 (initial state, Monitor default, ring offset), 12/12 green on the fix. AC2: full tests/webui sweep 92 files 435 pass 0 fail. AC3: bench .63 beta.405+d2a9aee: version/ini PASS, ring PASS, keyboard rail PASS; known limit: a stored Monitor tab is only marked once Monitor opens (hidden group; accepted by the coordinator). Focus lost after a rail Enter/Space is pre-existing since beta.404 and moved to its own task.
<!-- SECTION:FINAL_SUMMARY:END -->
