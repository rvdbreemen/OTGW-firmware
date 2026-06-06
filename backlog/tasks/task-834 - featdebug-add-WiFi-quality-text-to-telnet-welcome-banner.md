---
id: TASK-834
title: 'feat(debug): add WiFi quality (%/text) to telnet welcome banner'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 15:55'
updated_date: '2026-06-06 15:55'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Backport of 2.0.0 TASK-835. Welcome banner (sendTelnetBanner) shows RSSI dBm only; add quality %% + text via signal_quality_perc_quad + dBmtoQuality.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Welcome banner WiFi line shows RSSI dBm + quality %% + text
- [ ] #2 Build green (esp8266); evaluate.py --quick green
<!-- AC:END -->
