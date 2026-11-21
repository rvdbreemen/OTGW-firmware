---
id: TASK-2997
title: >-
  Web UI contrast and structure sweep: rail focus gap, detector contrast items,
  undefined --warn/--ok, heading skips (impeccable critique 2026-11-21)
status: To Do
assignee: []
created_date: '2026-11-21T07:58:00Z'
labels:
  - webui
  - ux
  - a11y
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1123000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the impeccable critique of 2026-11-21 by otgw-ux-designer (dev web UI after TASK-2956 and TASK-2991, origin/dev a31f97664). (1) Rail focus ring: outline-offset 2px with a 2px gap between rail items (v2.css:41, :438) overlaps a neighbour's active tint (about 2.94:1 in dark, not visually verified); use gap 4px or outline-offset:-2px as on .seg (v2.css:47). (2) Detector-only contrast fails in the static v2.html (impeccable detect, positions not traceable, line 0): #4caf50 on #f1f7f8 2.6:1, #8a8f92 on #f1f7f8 3.0:1, #f44336 on #fff 3.7:1, white on #d17d00 3.2:1, #258395 on #f1f7f8 4.1:1; locate each element first. (3) --warn and --ok are undefined, so the MQTT onboarding step falls back to #e0a300 / #27c469 on #fff (about 2.2-2.3:1). (4) h2 followed by h4 under SAT, Settings, Advanced and Diagnose (skipped heading level). The wizard #04222c-on-accent buttons and the SVG chip at v2.html:727 are NOT in this task (separate high task).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The rail focus ring no longer overlaps a neighbouring item (gap or inset offset), checked in dark and light with a screenshot via otgw-bench-tester
- [ ] #2 Each detector-only contrast pair is traced to its element and reaches >= 4.5:1 for text (3:1 for large text and UI components), re-run with impeccable detect showing none of these pairs
- [ ] #3 --warn and --ok are defined or the MQTT step uses existing tokens, with its texts >= 4.5:1 in both themes
- [ ] #4 No skipped heading levels on SAT, Settings, Advanced and Diagnose (impeccable detect skipped-heading gives 0)
- [ ] #5 python evaluate.py --quick shows no new FAIL and tests/webui pass
<!-- AC:END -->
