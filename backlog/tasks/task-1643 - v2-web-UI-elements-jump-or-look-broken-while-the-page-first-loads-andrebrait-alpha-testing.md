---
id: TASK-1643
title: >-
  v2 web UI: elements jump or look broken while the page first loads
  (andrebrait, #alpha-testing)
status: To Do
assignee: []
created_date: '2026-11-04T23:56:00Z'
updated_date: '2026-11-05T00:26:00Z'
labels:
  - web-ui
  - needs-maintainer-ok
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
- [ ] #1 The jarring first-load state is reproduced (screenshot or recording before the fix, synthetic data)
- [ ] #2 The first paint shows either a stable layout or a loading state, with before/after screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 andrebrait screenshot (msg 1567690322044129351, Chrome on Android 17, LAN 10.0.254.112): page painted unstyled (serif text, default buttons, black SVG fills). Cause read in code: v2.html loads v2-bundle.css through a script-inserted <link> with retry-on-503 (v2.html:31-44, call at :68, TASK-978/989); a script-inserted stylesheet is not render-blocking, so the browser paints the bare HTML first (FOUC). Fix option (not implemented, needs Robert's OK): inline style hiding the page or a loading overlay until loadCss onload, with a timeout fallback so a failed CSS still shows the page; keeps the 503 retry and the one-connection-at-a-time order. Related TASK-1640 (gzip shortens the window). Not reproduced on the bench yet.
<!-- SECTION:NOTES:END -->
