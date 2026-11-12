---
id: TASK-1873
title: >-
  Web UI header: theme switch is taller than the language button on phones (50
  vs 40 px)
status: To Do
assignee: []
created_date: '2026-11-12T20:35:00Z'
labels:
  - needs-maintainer-ok
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
