---
id: TASK-1743
title: >-
  Web UI: the startup settings fetch has no retry, so one busy 503 leaves the
  SAT page's SAT settings empty until Settings is opened
status: To Do
assignee: []
created_date: '2026-11-08T22:56:00Z'
labels:
  - webui
  - bench-finding
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 848000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench (not by a tester), otgw-firmware-80, 2026-11-08 23:44, .61 beta.157+8b67ad0 under a 15-min soak (4 REST polls per 30 s) plus a headless UI load. v2.js:6431 calls fetchSettings() once at startup (TASK-989 staggered burst); fetchSettings() (v2.js:2629-2631) returns early on !r.ok and never retries. setData stays empty, so renderSatSettings() (v2.js:2822) renders nothing under the SAT page's 'SAT settings' heading; only showPage('settings') (v2.js:263) fetches again. Seen on .61: SAT page Control view showed an empty 'SAT settings' section after 15 s; after opening Settings first, 41 rows rendered. In the same soak 1 of 81 REST calls was a 503 on /api/v2/settings. Related: TASK-1741 (busy 503 keeps the shown state; two-tab 503s), ADR-165/188 (REST cap 2, TX cap 4), ADR-184/single-flight rule (at most 2 API calls in flight).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A browser test against the real v2.html/v2.js shows the SAT settings section empty after a startup settings 503 on the old code and filled after the retry on the fix
- [ ] #2 The retry respects the 2-in-flight rule and backs off (no tight loop while the device stays busy)
- [ ] #3 Bench .61: SAT page opened directly after load shows the SAT settings rows
<!-- AC:END -->
