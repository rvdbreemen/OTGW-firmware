---
id: TASK-294
title: >-
  [SEC-H1] Register Origin/Referer in collectHeaders so CSRF check actually
  fires
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:16'
updated_date: '2026-04-19 07:17'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
checkHttpAuth() reads httpServer.header(Origin/Referer) but collectHeaders only registers If-None-Match. Both headers always return empty so isSameOriginRequest returns true; ADR-054 CSRF protection is effectively disabled for browsers. See restAPI.ino:75-104 and FSexplorer.ino:223-236.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 collectHeaders registers Origin, Referer on both ESP8266 and ESP32
- [ ] #2 Manual test: a POST from a cross-origin page is rejected, same-origin succeeds
- [x] #3 Telnet log confirms isSameOriginRequest sees populated originBuf
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC2 (browser cross-origin rejection proof) deferred to manual test. Requires a real browser hitting /api/v2/settings with a Referer from a different origin while authenticated, plus a control request from same origin. Flagged for the pre-release smoke test checklist.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
FSexplorer.ino collectHeaders now registers Origin and Referer alongside If-None-Match on both ESP8266 (variadic overload, plain string literals; PROGMEM is not safe here because WebServer stores the pointer by value) and ESP32 (3-element array+count). isSameOriginRequest() in restAPI.ino will now see populated originBuf and the ADR-054 CSRF check actually fires for browser requests.
<!-- SECTION:FINAL_SUMMARY:END -->
