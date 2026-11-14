---
id: TASK-1873
title: >-
  Web UI header: theme switch is taller than the language button on phones (50
  vs 40 px)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T20:35:00Z'
updated_date: '2026-11-14T21:33:00Z'
labels:
  - webui
  - bug
milestone: 2.0.0
dependencies: []
ordinal: 947000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter Sergeant D (Discord 821036882946424852), #dev-sat-mqtt https://discord.com/channels/812969634638725140/1105556725714649128/1570536148869324931 (2026-11-12 20:29:57 UTC), with a phone screenshot (dark theme, Greek): the language and theme cards in the header differ in height, asked whether it is intentional. Cause from reading the code (not bench-checked): v2.css @media(max-width:719px) (TASK-932 tap targets, line ~642) lifts .theme-switch button to min-height 44px, so the switch becomes 44 + 2x2 padding + 2x1 border = 50 px (box-sizing border-box), while .lang-menu summary keeps min-height 40px. On desktop both are 40 px (34 + 4 + 2 vs 40). Not intentional: TASK-1764 (language menu) and TASK-1830 (theme switch) were meant to match the OTGW site header. Fix candidates: give .lang-menu summary the same 44px+ touch height (and match the outer height), or size both from one token. Evidence for the fix: screenshots at 375 px and desktop, before and after.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 At a phone width (<= 719 px) the header language button and the theme switch have the same height, both >= 44 px (test on the real v2.html/v2.css, red on the old CSS)
- [x] #2 At desktop width both stay 40 px (no change outside the phone rule)
- [ ] #3 Bench: phone-width screenshot of the header on a device with the build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 Robert (DM 1571274522764513392, relayed by the coordinator, answer 'A'): go. Label needs-maintainer-ok removed.

2026-11-14 (92, lane wt-92-webui). Cause confirmed in the code: v2.css:642 (TASK-932, @media max-width 719px) lifts .theme-switch button to min-height 44px -> switch box 44 + 2x2 padding + 2x1 border = 50px; .lang-menu summary keeps min-height 40px (v2.css:50). Fix: in the same media block .lang-menu summary{min-height:50px}. New test tests/webui/header-controls-same-height.test.mjs (headless Chrome, real v2.html/v2.css via page-harness). OLD CSS: phone test FAILS 'language 40 px vs theme switch 50 px', desktop test passes (40/40). FIX: 2 pass 0 fail. Related tests one file at a time: lang-menu 7/7, theme-toggle-labels 4/4. evaluate.py --quick: 85 passed, 0 failed, 100.0%. Open: AC3 bench screenshot with the lane build (coordinator schedules).
<!-- SECTION:NOTES:END -->
