---
id: TASK-2979
title: >-
  otgw_capture.py: browser capture stops for good after the first CDP websocket
  error
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T16:49:00Z'
updated_date: '2026-11-27T05:21:00Z'
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
- [x] #1 While the run is going, a CDP websocket error does not end the browser capture: the tool re-attaches to the page target with backoff until it succeeds or the run stops
- [x] #2 Each successful re-attach logs one line 'Browser capture: reconnected' (with the time) in the transcript and summary
- [x] #3 A host test simulates a CDP websocket close during a run and sees the re-attach and the reconnected line; it fails on the code from before the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix in scripts/otgw_capture.py BrowserCapture: run() is now a launch/attach loop (launch_browser, find_page_target, pump, flush_pending). After the first successful attach a CDP error (websocket closed, clean close, browser process gone) is retried with back-off 1,2,4..30 s until the run stops; a dead browser process is started again. Each re-attach writes 'Browser capture: reconnected <time> (...)' to summary.txt and 'reconnected' to browser.log. A failure before the first attach still ends the capture as before.

Evidence: tests/test_capture_browser_reconnect.py (fake CDP http+ws, fake browser process). RED on the old code: 'the browser capture never re-attached ... thread alive=False connections=1', browser.log '[worker-error] [WinError 10054]', summary 'Browser capture: error - ...'; test_b also failed. GREEN with the fix: Ran 2 tests OK, 3 reruns OK. Not verified: a real Chromium/Edge CDP run.
<!-- SECTION:NOTES:END -->
