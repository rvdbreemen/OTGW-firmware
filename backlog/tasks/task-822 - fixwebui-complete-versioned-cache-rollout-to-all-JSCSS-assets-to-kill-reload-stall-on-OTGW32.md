---
id: TASK-822
title: >-
  fix(webui): complete versioned-cache rollout to all JS+CSS assets to kill
  reload stall on OTGW32
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-04 23:02'
updated_date: '2026-06-04 23:02'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-793 added ?v=<fsHash> versioned long-cache (max-age=86400) for index.js and graph.js only. The other 4 JS (sat.js, sat-slider.js, echarts-theme.js, theme-toggle.js) fall through to streamFile() with no Cache-Control, and the 2 CSS use no-cache+ETag (revalidate each visit). On the strictly-serial ESP32 WebServer every page load is 8 serialized round-trips, which is the root of the 'cannot find sat-slider.js' reload stall (file serves 200 in isolation; requests stall under concurrent load). Complete the pattern: inject ?v=<hash> into all 8 local asset URLs in the index.html stream and give every JS+CSS GET handler the versioned max-age/no-cache branch. After one cold load, reloads should drop from 8 requests to ~1 (index.html revalidate). Validate via scripts/capture-mqtt-debug.bat browser.log: reflash then re-capture, confirm reload issues ~1 device request and no PENDING sub-resources.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ?v=<fsHash> injected into all 6 JS and 2 CSS link/script URLs in the streamed index.html (FSexplorer sendIndex)
- [ ] #2 sat.js, sat-slider.js, echarts-theme.js, theme-toggle.js each have a GET handler with the versioned-URL cache branch (max-age=86400 when ?v matches fsHash, else no-cache), mirroring /index.js, including .gz preference
- [ ] #3 ds-tokens.css and components.css served with versioned max-age long-cache when ?v matches (replacing pure revalidate), still falling back to no-cache when ?v absent/stale
- [ ] #4 Reflash + browser.log re-capture shows a warm reload issues ~1 device request (index.html revalidate) with no PENDING sub-resources
- [ ] #5 build.py (firmware+filesystem) green for esp32; evaluate.py --quick shows no new failures
<!-- AC:END -->
