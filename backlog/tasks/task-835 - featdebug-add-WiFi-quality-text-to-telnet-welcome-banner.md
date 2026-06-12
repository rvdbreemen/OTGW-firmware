---
id: TASK-835
title: 'feat(debug): add WiFi quality (%/text) to telnet welcome banner'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 15:53'
updated_date: '2026-06-07 12:56'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The telnet welcome banner (sendTelnetBanner) shows RSSI in dBm but not the human-readable WiFi quality. Add quality percentage + text (signal_quality_perc_quad + dBmtoQuality) to the WiFi line so field debugging of weak-signal issues is one glance. Backport to dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Welcome banner WiFi line shows RSSI dBm + quality %% + quality text
- [x] #2 Build green esp32+esp8266; evaluate.py --quick green
- [x] #3 Backported to dev worktree
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WiFi quality (signal_quality_perc_quad %% + dBmtoQuality text) added to the telnet welcome banner WiFi line (sendTelnetBanner, networkStuff.ino) alongside the existing RSSI dBm. 2.0.0: committed 784e313a, pushed, alpha.165, build green (esp32+esp8266), evaluator green. Backported to dev: change landed on origin/dev (folded into the concurrent session's chore(release): beta.1 commit 8bd1e164 during a shared-worktree index race; code verified present on origin/dev, nothing lost).
<!-- SECTION:FINAL_SUMMARY:END -->
