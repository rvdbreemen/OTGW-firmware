---
id: TASK-2933
title: >-
  AP fallback: the softAP stays up next to a reconnected STA until the 5-min AP
  timer
status: To Do
assignee: []
created_date: '2026-11-19T21:56:00Z'
labels:
  - wifi
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1066000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by otgw-bench-tester on .61 (2.0.0-beta.401+47d08b1, TASK-2931 bench, 2026-11-19): after a 4-min outage the STA was back on the router at 22:42:34 (ping, REST, MQTT OK) while the AP fallback only stopped at the next 5-min AP timer, 22:44:30 ('WiFi AP: 5-min timer — attempting WiFi reconnect', 'WiFi reconnected from AP mode', 'fallback stopped, WiFi connected'). Existing behaviour (WIFI_AP_FALLBACK only checks on timerAPRetry, networkStuff.ino), not a regression: about 2 min with the softAP OTGW-xxxxxx (password otgw1234) and the STA side by side. Option: in WIFI_AP_FALLBACK, when WiFi.status() == WL_CONNECTED (the AP_STA mode lets the SDK reconnect), go to WIFI_RECONNECTED at once instead of waiting for the timer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 When the STA reconnects while the AP fallback is active, the softAP stops within seconds, not at the 5-min timer (host test on the real loopWifi state machine)
- [ ] #2 Bench (otgw-bench-tester): outage -> AP -> unblock: softAP gone within ~10 s after the STA is back
<!-- AC:END -->
