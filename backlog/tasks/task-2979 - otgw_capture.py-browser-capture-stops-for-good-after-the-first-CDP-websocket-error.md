---
id: TASK-2979
title: >-
  otgw_capture.py: browser capture stops for good after the first CDP websocket
  error
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T16:49:00Z'
labels:
  - tools
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1111000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source: a beta tester's 24 h SAT field run on 2026-11-20 (#beta-testing, Discord msg 1573377316686405773). At 13:18 telnet and MQTT dropped and reconnected within 1 to 2 s, but the tool printed 'Browser capture: error - websocket closed' and the web UI console/network capture stayed dead for the rest of the run. Cause in code: scripts/otgw_capture.py BrowserCapture.run() ends the thread on the first exception (except branch around line 1757, result 'Browser capture: error - ...'); the CDP websocket recv raises ConnectionError('websocket closed') at line 1612. Telnet and MQTT have their own reconnect loops; the browser capture has none.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 While the run is going, a CDP websocket error does not end the browser capture: the tool re-attaches to the page target with backoff until it succeeds or the run stops
- [ ] #2 Each successful re-attach logs one line 'Browser capture: reconnected' (with the time) in the transcript and summary
- [ ] #3 A host test simulates a CDP websocket close during a run and sees the re-attach and the reconnected line; it fails on the code from before the fix
<!-- AC:END -->
