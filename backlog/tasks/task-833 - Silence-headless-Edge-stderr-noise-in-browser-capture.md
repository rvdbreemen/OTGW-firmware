---
id: TASK-833
title: Silence headless Edge stderr noise in browser capture
status: To Do
assignee: []
created_date: '2026-06-06 09:09'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Headless Edge prints Chromium logging ERROR lines (task_manager fallback, SmartScreen DNS timeout, USB device_event_log) to the console, mixing with capture status. Add Chromium logging-suppression launch flags so these no longer reach the console; CDP browser.log is unaffected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Headless Edge launched with --disable-logging --log-level=3 (and SmartScreen lookup disabled); no timecoded ERROR lines on console
- [ ] #2 browser.log still captures console/network via CDP; capture status lines (Telnet connected etc) unaffected
- [ ] #3 PowerShell payload parses clean
<!-- AC:END -->
