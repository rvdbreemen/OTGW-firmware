---
id: TASK-1565
title: >-
  REST: a POST without a JSON content type reads the previous request's body
  (sat/settings got 18.5 for control_mode)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T18:59:00Z'
updated_date: '2026-11-03T18:22:00Z'
labels:
  - rest-api
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 716000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on bench .63, 2.0.0-beta.14+4d94f13, 2026-11-02 19:54 (TASK-1542 bench). Request 1: POST /api/v2/settings, Content-Type application/json, body {"name":"sattargettemp","value":18.5} -> ok. Request 2, right after: curl -X POST -d "pwm" /api/v2/sat/settings/control_mode (curl default Content-Type application/x-www-form-urlencoded) -> response {"status":"ok","setting":"control_mode","value":"18.5"}. The handler (restAPI.ino sat/settings branch, hasArgCompat(F("plain")) / argCompat(F("plain")) -> satExtractPostValue) got the value of the previous request's body. With JSON content type the same call returned value "pwm". Risk: a setting written with another request's value (here harmless: atoi("18.5")=18 matches no control mode). Needs a reproduction (host or bench: two requests in a row, the second form-encoded), the root cause in the async body/arg capture (stale plain buffer not cleared per request), and an old-vs-fix test.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduction on the bench or a host harness: a form-encoded POST right after a JSON POST gets the previous body on the old code
- [ ] #2 Fix: each request sees only its own body; a missing body gives 400 Missing value; host test red on the old code, green on the fix
- [ ] #3 Bench re-run of the same two curl calls on the fix returns value pwm or 400, never the previous value
<!-- AC:END -->
