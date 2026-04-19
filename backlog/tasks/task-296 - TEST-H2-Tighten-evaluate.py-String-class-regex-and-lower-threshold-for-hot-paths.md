---
id: TASK-296
title: >-
  [TEST-H2] Tighten evaluate.py String-class regex and lower threshold for hot
  paths
status: To Do
assignee: []
created_date: '2026-04-18 19:16'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Current regex only matches 'String foo = ...' and warn-threshold is 5. Misses String foo;, String foo(x), String&. Two hot-path Strings exist (SATble.ino:187, SATweather.ino:148) and 3 more could land silently.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Regex also catches 'String foo;', 'String foo(...)', 'String& foo'
- [ ] #2 Threshold lowered to 1 and promoted to FAIL for SAT*.ino, OT*.ino, MQTT*.ino, restAPI.ino
- [ ] #3 Known-violations documented or allow-listed; new ones fail CI
<!-- AC:END -->
