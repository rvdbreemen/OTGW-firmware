---
id: TASK-834
title: 'feat(debug): add WiFi quality (%/text) to telnet welcome banner'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 15:55'
updated_date: '2026-06-07 12:56'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Backport of 2.0.0 TASK-835. Welcome banner (sendTelnetBanner) shows RSSI dBm only; add quality %% + text via signal_quality_perc_quad + dBmtoQuality.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Welcome banner WiFi line shows RSSI dBm + quality %% + text
- [x] #2 Build green (esp8266); evaluate.py --quick green
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WiFi quality %/text added to telnet welcome banner (networkStuff.ino sendTelnetBanner). Code shipped on origin/dev in beta.1 (commit 8bd1e164, swept in during a shared-worktree index race with a concurrent session). Verified present on origin/dev.
<!-- SECTION:FINAL_SUMMARY:END -->
