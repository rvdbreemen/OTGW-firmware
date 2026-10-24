---
id: TASK-1363
title: >-
  v2 log view sometimes stops auto-updating after scrolling to the top and back
  down
status: To Do
assignee: []
created_date: '2026-10-24T21:32:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 498000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-24 17:57 UTC (#alpha-testing 1563612387402317837), alpha.404, Remeha/Techneco Toros heat pump: 'the log screen in webinterface sometimes stops auto updating once I scrolled it all the way up and down, but can not reliably replicate that'. Check the v2 log view's auto-scroll/follow state on the latest build (scroll-lock logic, follow flag after scrolling to top then bottom); asked him for a screenshot and time if it recurs on the latest build.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced in a browser test (scroll to top, then bottom: new lines must keep appearing and following resumes) red on the faulty build, green on the fix; or documented as not reproducible on the latest build with the test as evidence
<!-- AC:END -->
