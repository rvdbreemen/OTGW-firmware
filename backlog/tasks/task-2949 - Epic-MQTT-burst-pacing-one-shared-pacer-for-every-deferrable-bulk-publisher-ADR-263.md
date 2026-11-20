---
id: TASK-2949
title: >-
  Epic: MQTT burst pacing for 2.0.0: espMqttClient fork as source, segment bound
  in the library, one shared pacer (ADR-264, ADR-265, ADR-266)
status: To Do
assignee: []
created_date: '2026-11-20T10:54:00Z'
updated_date: '2026-11-20T11:50:00Z'
labels:
  - mqtt
  - heap
  - architecture
  - epic
milestone: 2.0.0
dependencies:
  - TASK-2019
priority: medium
ordinal: 1078000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Architecture epic (otgw-architect, 2026-11-20); milestone 2.0.0 by Robert's decisions of 2026-11-20 (fork as dependency; everything in 2.0.0; stable moved to Sun 29 Nov). Mechanism (otgw-dev, beta.403 + source check): one MQTTclient.loop() writes the whole outbox (MqttClient.cpp:354); with TCP_NODELAY (ClientSync.cpp:34-36, :52-54) every packet is a segment of ~1.5 KB pbuf (TCP_OVERSIZE=MSS), at most TCP_SND_QUEUELEN 16 per pass: 20-24 KB transient. Order: .1 confirm + attribute the remaining 4-8 KB; .2 fork #191+TASK-1325 tag otgw.1; .4 switch the dependency (ADR-264); .3 fork Nagle switch + PUBLISH cap; .5 use in firmware + bench A/B (ADR-266); .6 mqttPaceAllows() + SAT gate; .7 drip; .8 connect stages; .9 override sweep + stats pacer; .10 evaluate gate (ADR-265); .11 upstream PR; .12 steady-state bench; .13 docs. Minimum for the stable: .1-.5 and .13 for what ships; .6-.10 cuttable step by step. Analysis: docs/plans/2026-11-20-mqtt-burst-pacing-analysis.md. The TASK-2019 trigger flags are the baseline that .6 removes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every subtask Done with its own evidence; no MQTTclient.queueSize()/mqttOutboxDepth() threshold remains outside mqttPaceAllows() (evaluate.py gate green)
- [ ] #2 Bench on .61: TASK-2019 takeover recipe and TASK-1757 W/C/B boots show no probe drop of 10 KB or more inside MQTTclient.loop() (at=13 from=12), old-vs-new numbers in the subtask
- [ ] #3 ADR-264 accepted (done 2026-11-20); ADR-265 and ADR-266 answered on all Open Questions and accepted by Robert, or rejected with the reason recorded
<!-- AC:END -->
