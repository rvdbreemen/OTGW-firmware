---
id: TASK-2991
title: >-
  Web UI: focus is lost after Enter or Space on a Settings rail button
  (renderSettings rebuilds the rail; next Tab goes to the skip link)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T02:07:00Z'
updated_date: '2026-11-21T07:32:00Z'
labels:
  - webui
  - a11y
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1116000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench finding by otgw-bench-tester on .63 (TASK-2982, beta.405+d2a9aee, evidence OTGW-field-data/bench-63/2026-11-21_task2982-ariacurrent/ui/t2982-ui-63/facts.json). Pre-existing since TASK-2958 (beta.404) made the rail items <button>: a headless probe on beta.404 and beta.405 both leave document.activeElement on <body> after Enter on a focused rail button. Cause: the click handler sets setActiveCat and calls renderSettings(), which empties #setRail (v2.js rail.textContent = '') and builds new buttons, so the focused element no longer exists. A keyboard user then starts over at the skip link (WCAG 2.4.3 focus order). Fix idea: before the rebuild remember the focused rail category, after the rebuild focus the new button with that data-cat.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After Enter or Space on a focused rail button, focus is on the rail button of the opened category (browser test: red on the current code, green on the fix)
- [x] #2 Search typing in the settings search field keeps its focus (no regression); full tests/webui sweep green
- [x] #3 Short bench on a unit (otgw-bench-tester, NAS Chromium)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-webui: renderSettings() remembers the focused rail category (document.activeElement inside #setRail) before it empties the rail and focuses the rebuilt button of that category afterwards; focus elsewhere (the settings search) is untouched. Test a11y-structure.test.mjs: after Enter and after Space on a rail button, document.activeElement is the rail button of the opened category: red on the current code (actual BODY, expected mqtt), green on the fix; typing 'mq' in #setSearch keeps focus and value in the search field (green). a11y-structure 12/12, settings-help 29/29. Full sweep, build and bench pending (heavy slot after the 08:00 beta).

Bench (otgw-bench-tester) 2026-11-21 07:30 on .61, beta.407+12d9e55, NAS Chromium (scripts/nas-bench/ui/task2991.mjs): PASS 4/4. Enter on rail MQTT -> section Connection, activeElement = MQTT rail button (aria-current true); Space on OT-Direct: same; Tab -> Sensors, Enter -> BLE sensors with focus kept; #setSearch keeps focus after typing 'mq', rail filters 12 -> 2; fresh load 1 aria-current per group; .seg ring 2px rgb(30,136,229) offset -2px inside the container; ini diff 0. Evidence: OTGW-field-data/bench-otgw32/2026-11-21_task2991-railfocus/ (facts.json, 3 screenshots, SHA256SUMS).

2026-11-21: full webui sweep on 12d9e5548 files=92 pass 435 fail 0; settings search keeps focus (a11y-structure test green, bench .61 and .63 smoke: #setSearch keeps focus while typing). Build beta.407+12d9e55 SUCCESS; bench .61 PASS 4/4. Pushed as bafd59f91 (rebased, git diff 12d9e5548 bafd59f91 -- src/ = 0 lines). Combined smoke beta.408+717a7fc on .63 PASS 3/3. Alpha post 1573601587723640945 covers beta.407 + beta.408.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
renderSettings() remembers the focused rail category and focuses the rebuilt rail button afterwards; the settings search focus is untouched. AC1: a11y-structure.test.mjs Enter/Space red on the old code (activeElement BODY, expected mqtt), green on the fix. AC2: search typing keeps focus (test + bench); full webui sweep 92/435/0. AC3: otgw-bench-tester PASS 4/4 on .61 (NAS Chromium), plus the combined smoke on .63 3/3. Pushed in bafd59f91 (beta.407); alpha post 1573601587723640945.
<!-- SECTION:FINAL_SUMMARY:END -->
