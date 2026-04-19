---
id: TASK-294
title: >-
  [SEC-H1] Register Origin/Referer in collectHeaders so CSRF check actually
  fires
status: To Do
assignee: []
created_date: '2026-04-18 19:16'
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
- [ ] #1 collectHeaders registers Origin, Referer on both ESP8266 and ESP32
- [ ] #2 Manual test: a POST from a cross-origin page is rejected, same-origin succeeds
- [ ] #3 Telnet log confirms isSameOriginRequest sees populated originBuf
<!-- AC:END -->
