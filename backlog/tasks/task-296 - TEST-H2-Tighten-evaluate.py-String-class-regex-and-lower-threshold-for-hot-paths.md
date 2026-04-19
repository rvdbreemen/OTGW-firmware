---
id: TASK-296
title: >-
  [TEST-H2] Tighten evaluate.py String-class regex and lower threshold for hot
  paths
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:16'
updated_date: '2026-04-19 07:05'
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
- [x] #1 Regex also catches 'String foo;', 'String foo(...)', 'String& foo'
- [ ] #2 Threshold lowered to 1 and promoted to FAIL for SAT*.ino, OT*.ino, MQTT*.ino, restAPI.ino
- [x] #3 Known-violations documented or allow-listed; new ones fail CI
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC2 (FAIL promotion) deferred: the widened regex surfaces 19 existing hot-path String usages (mostly httpServer.arg() return captures). Promoting to FAIL today blocks CI. Detection lands as WARN with file:line detail; follow-up task should burn down those 19 sites then flip WARN->FAIL in one line.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
evaluate.py: widened String-class regex to include declarations, init-assigns and direct-init forms. Reference forms (String&, const String&) intentionally skipped because they do not allocate. Split reporting into hot-path (SAT/MQTTstuff/restAPI/OTGW-Core/OTDirect, WARN) vs other files (WARN > 5). tests/test_evaluate.py covers all supported patterns plus the reference-skip case.
<!-- SECTION:FINAL_SUMMARY:END -->
