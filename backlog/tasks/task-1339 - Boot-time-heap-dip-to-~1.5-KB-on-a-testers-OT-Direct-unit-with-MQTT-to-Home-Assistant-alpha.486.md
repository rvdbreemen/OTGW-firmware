---
id: TASK-1339
title: >-
  Boot-time heap dip to ~1.5 KB on a tester's OT-Direct unit with MQTT to Home
  Assistant (alpha.486)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T07:29:00Z'
updated_date: '2026-10-24T07:45:00Z'
labels:
  - heap
  - mqtt
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 462000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D capture 2026-10-24 06:41 UTC (alpha.486, transcript-20261024-093559-...-otgw-1020BA16C6BC): banner at uptime 0 min shows minFree 1524 (low/warn/crit 2/0/0); the MQTT stats topic min_free_heap showed 608 (may be a retained value from an earlier boot). During the later telnet session the heap stayed >= 59792 B, so the dip is not the TASK-1333 telnet burst. His setup: OT-Direct, OT bus offline (no boiler), SAT enabled, BLE off, weather off, MQTT on to homeassistant.local (mDNS, real HA broker), NTP on. Bench (same firmware, local mosquitto on a wired PC): minFree stays ~22-34 KB. Hypothesis (not verified): the MQTT connect/discovery burst after boot queues many publishes while a slower WiFi broker ACKs late, holding send buffers (same mechanism as TASK-1333, but on MQTT/espMqttClient's outbox); other candidates: mDNS resolution of homeassistant.local, retained messages delivered on subscribe, OT-Direct with the bus offline.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The allocation that drives the boot-time dip is named with a bench reproduction (e.g. throttled or remote broker via the MikroTik router, mDNS broker name) or recorded as not reproducible with what was tried
- [ ] #2 If a fix follows: min_free_heap after boot before/after on the same reproduction
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 consolidated open questions re-posted to Sergeant D in #alpha-testing (Robert asked to make the wait explicit); waiting for his answer.

2026-10-24 bench (OTGW32 alpha.486, OT-Direct, no boiler, SAT on, BLE off, local mosquitto; reboot, read hd_min_free_heap after ~90 s, no telnet): broker by IP: 23036, 26036, 21280 B; broker by IP with the router throttling the unit to 64 kbit/s: 24572 B (slow-broker hypothesis not supported); broker by mDNS name Laptop-Robert.local: 14188, 25732, 12772 B. mDNS runs are lower in 2 of 3, so name resolution may add a ~9-11 KB transient, but the spread is too large to call it; none reached his 1.5 KB. Asked Sergeant D (question 5) for entity count, broker location and one capture with the broker set by IP.
<!-- SECTION:NOTES:END -->
