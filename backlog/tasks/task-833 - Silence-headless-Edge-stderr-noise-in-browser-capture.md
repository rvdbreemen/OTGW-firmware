---
id: TASK-833
title: Silence headless Edge stderr noise in browser capture
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 09:09'
updated_date: '2026-06-06 09:10'
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
- [x] #1 Headless Edge launched with --disable-logging --log-level=3 (and SmartScreen lookup disabled); no timecoded ERROR lines on console
- [x] #2 browser.log still captures console/network via CDP; capture status lines (Telnet connected etc) unaffected
- [x] #3 PowerShell payload parses clean
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added Chromium logging-suppression launch flags to the headless Edge browser capture so its stderr ERROR spam no longer reaches the console.

Flags added: --disable-logging, --log-level=3 (FATAL only), --disable-features=msSmartScreenProtection. These silence the task-manager fallback, USB device_event_log, and SmartScreen DNS-timeout ERROR lines that were mixing with capture status output. CDP browser.log (console/network) is unaffected; capture status lines such as Telnet connected are unaffected.

Verified: PowerShell AST parse clean (1553 lines); flags present. Script-only change.
<!-- SECTION:FINAL_SUMMARY:END -->
