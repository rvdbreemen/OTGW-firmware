---
id: TASK-821
title: >-
  tooling(capture): add headless-Edge CDP browser-log capture to
  capture-mqtt-debug.bat
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-04 22:23'
updated_date: '2026-06-04 22:37'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Extend the single-file capture-mqtt-debug.bat so it also captures browser devtools output (console.* + exceptions + Log-domain resource-load failures like 404s + network response timings) from the OTGW web UI, concurrently with the existing telnet+MQTT capture, and folds browser.log into the merged transcript.txt. Driven by headless Microsoft Edge (Chromium, ships on Win11) over the Chrome DevTools Protocol (CDP) from PowerShell. Stays single-file: the browser worker is a concurrent runspace inside the existing embedded PS payload, no extra files shipped. Motivation: reproduce the XHR latency ramp (85ms->7000ms on /device/time + /otgw/otmonitor) and capture app console logs + resource 404s (e.g. sat-slider.js) in the same upload tester already produces.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Browser capture launches headless Edge with a temp user-data-dir + remote-debugging-port and attaches via CDP before navigation so early page logs are not missed
- [x] #2 Captures Runtime.consoleAPICalled, Runtime.exceptionThrown, and Log.entryAdded (resource-load failures / 404s) into browser.log with timestamps and severity
- [x] #3 Captures per-request network timing (requestWillBeSent -> loadingFinished) as duration-ms + status + url into browser.log
- [x] #4 Browser worker runs concurrently with telnet+MQTT loop and honours the existing CancelFlag stop (Q / Ctrl+C / duration)
- [x] #5 browser.log is merged into transcript.txt and intermediate file cleaned up like the others
- [x] #6 Single-file delivery preserved: no new files shipped; all code embedded in the .bat payload
- [x] #7 Graceful degradation: if Edge is not found or CDP attach fails, the rest of the capture still runs and the summary records why browser capture was skipped
- [x] #8 New switches: -SkipBrowserCapture, -BrowserUrl, -BrowserDebugPort, -BrowserPath documented in -Help
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Validated end-to-end against live OTGW32 (192.168.88.64, alpha.160). browser.log captured CDP connect, echarts CDN load (274ms,200), and every local sub-resource PENDING/never-finished in a 30s window (sat-slider.js, index.js, sat.js, css). Confirms webserver sub-resource starvation = same root as XHR latency ramp. Standalone fetch of sat-slider.js returned 200/1736 bytes/679ms, proving the file is present and the browser 'cannot find' it only due to concurrent-load stall, not a 404. Smoke-tested: CDP send/receive/parse sequence, cross-runspace CancelFlag stop + EndInvoke summary, merge+cleanup. Payload parses clean (Parser::ParseFile, 0 errors). Note: navigate to device IP more reliable than otgw.local (mDNS) for headless sub-resource resolution.
<!-- SECTION:NOTES:END -->
