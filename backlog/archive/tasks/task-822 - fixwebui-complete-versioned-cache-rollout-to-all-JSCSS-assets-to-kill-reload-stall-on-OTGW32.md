---
id: TASK-822
title: >-
  fix(webui): complete versioned-cache rollout to all JS+CSS assets to kill
  reload stall on OTGW32
status: Done
assignee:
  - '@claude'
created_date: '2026-06-04 23:02'
updated_date: '2026-06-04 23:16'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-793 added ?v=<fsHash> versioned long-cache (max-age=86400) for index.js and graph.js only. The other 4 JS (sat.js, sat-slider.js, echarts-theme.js, theme-toggle.js) fall through to streamFile() with no Cache-Control, and the 2 CSS use no-cache+ETag (revalidate each visit). On the strictly-serial ESP32 WebServer every page load is 8 serialized round-trips, which is the root of the 'cannot find sat-slider.js' reload stall (file serves 200 in isolation; requests stall under concurrent load). Complete the pattern: inject ?v=<hash> into all 8 local asset URLs in the index.html stream and give every JS+CSS GET handler the versioned max-age/no-cache branch. After one cold load, reloads should drop from 8 requests to ~1 (index.html revalidate). Validate via scripts/capture-mqtt-debug.bat browser.log: reflash then re-capture, confirm reload issues ~1 device request and no PENDING sub-resources.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ?v=<fsHash> injected into all 6 JS and 2 CSS link/script URLs in the streamed index.html (FSexplorer sendIndex)
- [x] #2 sat.js, sat-slider.js, echarts-theme.js, theme-toggle.js each have a GET handler with the versioned-URL cache branch (max-age=86400 when ?v matches fsHash, else no-cache), mirroring /index.js, including .gz preference
- [x] #3 ds-tokens.css and components.css served with versioned max-age long-cache when ?v matches (replacing pure revalidate), still falling back to no-cache when ?v absent/stale
- [ ] #4 Reflash + browser.log re-capture shows a warm reload issues ~1 device request (index.html revalidate) with no PENDING sub-resources
- [x] #5 build.py (firmware+filesystem) green for esp32; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in FSexplorer.ino: injectVersionedAsset() (PROGMEM-correct token-list injection, replaces hardcoded per-asset offsets) + serveVersionedAsset() (generic versioned-cache serve, replaces duplicated index.js/graph.js handlers + serveCssRevalidated). All 6 JS + 2 CSS now get ?v=<fsHash> injected and max-age=86400 when versioned, no-cache fallback. Bumped alpha.160->161. Build green (esp8266+esp32 SUCCESS, ESP32S3 image, filesystem). evaluate.py --quick 0 fail/2 pre-existing warn. Committed b2571c88, pushed. AC4 (reflash + browser.log re-capture confirming ~1 warm-reload request, no PENDING sub-resources) = FIELD VALIDATION, pending on-device flash of alpha.161 by maintainer.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed TASK-793 versioned-cache rollout: all 6 JS + 2 CSS webui assets now use ?v=<fsHash> URLs + max-age long-cache (was index.js/graph.js only). Refactored brittle per-asset injection + duplicated handlers into injectVersionedAsset()/serveVersionedAsset(). Warm reload should drop from 8 serialized requests to ~1 on the single-connection ESP32 WebServer, fixing the 'cannot find sat-slider.js' stall. alpha.161, build+eval green. Field validation (reflash + browser.log re-capture) pending.
<!-- SECTION:FINAL_SUMMARY:END -->
