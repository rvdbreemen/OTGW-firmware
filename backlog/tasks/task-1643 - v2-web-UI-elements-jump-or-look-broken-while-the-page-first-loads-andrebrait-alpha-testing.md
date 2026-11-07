---
id: TASK-1643
title: >-
  v2 web UI: elements jump or look broken while the page first loads
  (andrebrait, #alpha-testing)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T23:56:00Z'
updated_date: '2026-11-05T09:30:00Z'
labels:
  - web-ui
dependencies: []
priority: low
ordinal: 764000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by andrebrait in #alpha-testing (msg 1567687106934083745, 2026-11-04 23:48 UTC): 'When I first load the page, the initial placement of some graphical elements is jarring/makes it look like it's not going to load properly. Can we try and get rid of them or overlay the page with a loading screen of some sort so that it feels a bit better?' Which elements, browser and access path (LAN or HA ingress) asked in the reply. Related: TASK-1640 (GitHub #692, pre-compressed assets: shorter first load). Tester feedback: needs Robert's OK before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The jarring first-load state is reproduced (screenshot or recording before the fix, synthetic data)
- [x] #2 The first paint shows either a stable layout or a loading state, with before/after screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 andrebrait screenshot (msg 1567690322044129351, Chrome on Android 17, LAN 10.0.254.112): page painted unstyled (serif text, default buttons, black SVG fills). Cause read in code: v2.html loads v2-bundle.css through a script-inserted <link> with retry-on-503 (v2.html:31-44, call at :68, TASK-978/989); a script-inserted stylesheet is not render-blocking, so the browser paints the bare HTML first (FOUC). Fix option (not implemented, needs Robert's OK): inline style hiding the page or a loading overlay until loadCss onload, with a timeout fallback so a failed CSS still shows the page; keeps the 503 retry and the one-connection-at-a-time order. Related TASK-1640 (gzip shortens the window). Not reproduced on the bench yet.

2026-11-05 andrebrait (msg 1567696952341172225): 'Why not just link the style sheet on the head of the page then?' Preferred option now: static <link rel=stylesheet href=v2-bundle.css> in <head> (render-blocking, no unstyled paint) with an onerror handler that re-inserts it with the existing backoff, and v2.js started from its onload to keep the one-file-at-a-time order (TASK-989, ADR-165). Needs a bench check that a 503 on the CSS still recovers.

2026-11-05 andrebrait (msg 1567699818757750835): 'embedding the style sheet is not an option?' Options for Robert: (1) head link + onerror retry (preferred, smallest change); (2) inline the whole bundle in v2.html (source sizes v2.html 55 KB, v2-bundle.css 76 KB): one request less, no FOUC, loses separate caching, build change, changes the ADR-192 asset set -> needs an ADR; (3) inline only first-paint critical CSS, keep the bundle.

2026-11-05 Robert (session): go for option 'head link with onerror retry' (render-blocking <link> in the head, re-added with the same backoff on a 503, v2.js started from its onload so the one-file-at-a-time order stays). Needs a bench check under a 503.

2026-11-05 bench .61 beta.94+c474756, Playwright: v2-bundle.css answered 503 for the first 3 requests (route-injected), loader retried at t=237/945/1356 ms, 4th passed at 1962 ms, v2.js requested only after it (2429 ms), fonts after that; page styled (body font Inter), stylesheet link in the head. Screenshot: .playwright-mcp/t1643-after-503-retry.png (local). Before: andrebrait's screenshot (#alpha-testing 1567690322044129351) shows the unstyled first paint.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Render-blocking <link> for v2-bundle.css in the head with an onerror re-add (same backoff, 8 retries 200..1600 ms) and v2.js started from the stylesheet's onload. Evidence: tests/webui/v2-css-render-blocking.test.mjs 4/4 (old v2.html: 1 FAIL, the render-blocking check); bench .61 beta.94+c474756 under 3 injected 503s: retry sequence and order css -> v2.js -> fonts observed, page styled. Before screenshot: andrebrait (Discord). Not verified: Safari/Firefox, a device-generated (not injected) 503.
<!-- SECTION:FINAL_SUMMARY:END -->
