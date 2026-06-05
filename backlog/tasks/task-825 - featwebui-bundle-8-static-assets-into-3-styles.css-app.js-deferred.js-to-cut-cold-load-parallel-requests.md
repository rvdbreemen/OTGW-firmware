---
id: TASK-825
title: >-
  feat(webui): bundle 8 static assets into 3 (styles.css + app.js + deferred.js)
  to cut cold-load parallel requests
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-05 18:43'
updated_date: '2026-06-05 18:43'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reproducible on OTGW32 alpha.162: index.html (batched, ~0.8-1.5s) + 2 CSS complete, but the 6 parallel JS requests (index.js 346KB, sat.js 58KB, graph.js 47KB + 3 tiny) never finish on the serial single-connection ESP32 WebServer (connection/accept starvation, not reboot/heap/Nagle). Bundle to reduce parallel local requests 8 -> 3. Design: app.js = index.js+graph.js+sat.js (non-defer, source order); deferred.js = echarts-theme.js+theme-toggle.js+sat-slider.js (defer, source order - they DOM-hook so defer MUST be preserved -> 2 JS bundles not 1); styles.css = ds-tokens.css+components.css (tokens first). echarts CDN stays external (integrity). build.py stages a copy of data/, concatenates sources into the 3 bundles, removes the 8 sources, and rewrites index.html's asset tags to the 3 bundles; data/ keeps the 8 sources + 8-tag index.html so local dev (webui_launcher) and editing stay intact. FSexplorer sendIndex ?v= token list + serveVersionedAsset handlers updated to the 3 bundle names. ADR for the build-tooling change. Pairs with future gzip (size) - bundling is request-count only (does not shrink the 530KB total).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.py concatenates the 8 sources into styles.css + app.js + deferred.js in a staging dir (source order; ds-tokens before components; defer group separate) and mklittlefs's the staging dir, not data/ directly
- [ ] #2 Staging index.html references exactly 3 local assets (styles.css, app.js defer-less, deferred.js defer) + echarts CDN; the 8 source files are absent from the image
- [ ] #3 data/ keeps the 8 source files and the 8-tag index.html so local dev/editing is unaffected
- [ ] #4 FSexplorer sendIndex injects ?v=<hash> into the 3 bundle tags; serveVersionedAsset handlers exist for /styles.css /app.js /deferred.js
- [ ] #5 Device field-validate: cold-load completes ALL bundles (no PENDING), UI renders + functions (theme toggle, sliders, charts), no watchdog reboot
- [ ] #6 build esp32+esp8266 green; evaluate.py --quick no new failures; ADR written for the bundling build-tooling change
<!-- AC:END -->
