---
id: TASK-3105
title: >-
  T2 regression: the MQTT pacer's liveness bound never re-arms outside
  satPublishMQTT(), SAT state topics stop publishing when the largest block
  stays under the pacer minimum (beta.450 on .63: 0 SAT publishes, live stuck at
  1)
status: To Do
assignee: []
created_date: '2026-11-26T11:42:00Z'
labels:
  - bug
  - mqtt
milestone: 2.0.0
dependencies: []
ordinal: 1167000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench-tester on .63, 2.0.0-beta.450+944e29d, SAT on: R1 (beta.438) published 7-18 SAT topics per 5 min, T2 published 1 in 25 min, set sat/target not echoed within 60-75 s, retained sat/climate_target stale. Cause: the liveness allowance and wait timer are re-armed only in satPublishDeferredClear() (SATmqttPublish.cpp, called only by satPublishMQTT(), SATcontrol.ino:5570), but the gate is also asked outside a pass (satPublishClimateBridge/satPublishCoreState every SAT loop SATcontrol.ino:8074-8075, BLE helpers SATble.ino:910-917, MQTTstuff.ino:3827-3836); after one forced publish every later ask was refused for good. The block 19444 < MQTT_PACE_MIN_BLOCK 20480 made the gate permanently closed on .63. Fix: liveness in time (one forced ask per 100 ms after a 5 s closed stretch, stretch ends after 1 s without refusal) and MQTT_PACE_MIN_BLOCK 16384 = EMC_MIN_FREE_MEMORY (espMqttClient Config.h:32, refusal Packets/Packet.cpp:315). Follow-up TASK-3103 keeps the per-pass ask cache and calibration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code red on 381797b7c, green on the fix: 20 changed values in a row, block under the pacer minimum, asked outside any pass, each published within 6 s, liveness counter counts the forced ones (modes: no pass, pass every 500 ms, pass every 30 s); the heartbeat publish survives a closed gate; mutant (allowance consumed once) fails the test
- [ ] #2 Full run_tests.py and evaluate --quick green on the fix commit
- [ ] #3 Bench on .63 (bench-tester): SAT-on echo of a set within 6 s, 10 of 10, zero silence over 5 min with a changing value, SAT topic traffic back at about the R1 rate (10-18 per 5 min) before the soak
<!-- AC:END -->
