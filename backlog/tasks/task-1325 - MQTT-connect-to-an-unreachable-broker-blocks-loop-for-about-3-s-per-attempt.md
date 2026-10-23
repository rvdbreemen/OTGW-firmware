---
id: TASK-1325
title: MQTT connect to an unreachable broker blocks loop() for about 3 s per attempt
status: To Do
assignee: []
created_date: '2026-10-23T16:23:00Z'
labels:
  - mqtt
  - performance
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 448000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1307 (bench .88.61, alpha.471, broker 192.168.88.32:1883 unreachable): every MQTT connection attempt stalls loop() ~3.02 s ('[loop-stall] 3017-3037 ms gap' right after 'Attempting MQTT connection', 16 stalls in 5 min); with MQTT disabled on the same build: 0 stalls, max loop gap 148 ms. doTaskEvery1s ticks are skipped during the stall, so state.uptime lags the wall clock. Any user whose broker is down or misconfigured gets this. Look at handleMQTT's connect path and the vendored espMqttClient (ADR-186/187) connect: make the TCP connect non-blocking or run it off the loop task; check the library-change rule (ADR-186 vendored copy).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduction: unreachable broker -> loop-stall lines ~3 s per attempt on the current build (red)
- [ ] #2 Fix: with an unreachable broker the max loop gap stays below 500 ms over 5 min, MQTT still connects when the broker comes back (green, bench)
<!-- AC:END -->
