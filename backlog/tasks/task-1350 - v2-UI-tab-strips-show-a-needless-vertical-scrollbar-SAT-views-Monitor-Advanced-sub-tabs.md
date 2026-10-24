---
id: TASK-1350
title: >-
  v2 UI: tab strips show a needless vertical scrollbar (SAT views,
  Monitor/Advanced sub-tabs)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T14:31:00Z'
updated_date: '2026-10-24T14:34:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 485000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 14:25 UTC (#alpha-testing 1563559031140913224, screenshot): a small vertical scrollbar at the right of the SAT view tabs (Thermostat/Control/Technical), also on Monitor and Advanced. Cause (v2.css:292 .subtabs and :719 .sat-views): overflow-x:auto makes overflow-y compute to auto (CSS overflow rule), and the tab buttons use margin-bottom:-1px to sit on the 1px baseline, so the content is 1px taller than the box -> vertical scrollbar. Fix: overflow-y:hidden on both strips (keep horizontal scrolling on narrow screens). Bug fix, not a restyle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No vertical scrollbar on the SAT view tabs and the Monitor/Advanced sub-tabs (headless browser test: scrollHeight <= clientHeight for .sat-views/.subtabs; red on HEAD, green on fix); horizontal scrolling on a narrow viewport still works
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: tests/webui/tab-strip-no-vscroll.test.mjs (real v2.html + v2-bundle.css in headless Edge): fix 0 failures of 7 (overflow-y hidden on .sat-views and both .subtabs, overflow-x still auto); HEAD data 3 failures (overflow-y auto). v2-bundle.css regenerated with build.generate_v2_css_bundle (it is what the page loads).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The v2 tab strips (SAT views, Monitor and Advanced sub-tabs) no longer show a vertical scrollbar: overflow-y:hidden next to overflow-x:auto (overflow-x alone makes overflow-y auto, and the tabs' -1px bottom margin overflowed by 1px). Sideways scrolling on narrow screens stays. Evidence: browser test 7/7 on fix, 3 failures on HEAD.
<!-- SECTION:FINAL_SUMMARY:END -->
