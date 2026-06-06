---
id: TASK-835
title: 'feat(debug): add WiFi quality (%/text) to telnet welcome banner'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 15:53'
updated_date: '2026-06-06 15:54'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The telnet welcome banner (sendTelnetBanner) shows RSSI in dBm but not the human-readable WiFi quality. Add quality percentage + text (signal_quality_perc_quad + dBmtoQuality) to the WiFi line so field debugging of weak-signal issues is one glance. Backport to dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Welcome banner WiFi line shows RSSI dBm + quality %% + quality text
- [ ] #2 Build green esp32+esp8266; evaluate.py --quick green
- [ ] #3 Backported to dev worktree
<!-- AC:END -->
