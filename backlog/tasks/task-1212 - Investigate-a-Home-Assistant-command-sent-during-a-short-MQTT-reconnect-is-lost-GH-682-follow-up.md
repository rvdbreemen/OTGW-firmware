---
id: TASK-1212
title: >-
  Investigate: a Home Assistant command sent during a short MQTT reconnect is
  lost (GH #682 follow-up)
status: To Do
assignee: []
created_date: '2026-10-04 19:34'
labels:
  - bug
  - needs-info
  - mqtt
dependencies: []
priority: medium
ordinal: 241000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by jaronbor on GH #682 (2026-10-01) after running 1.7.6 (since beta.7): two short MQTT disconnects in about 5 days, and one of them made Home Assistant miss an override. Mosquitto log: 'Client OTGWA4CF12B09DDE disconnected.' (a clean close, no malformed packet), then reconnects 8 s and 17 s later with (p4, c1, k60): MQTT 3.1.1 with a clean session. mrfox7688 confirmed (2026-09-30) that the 'Can't decode payload' errors are gone.
Reading (not verified): these are probably the deliberate closes from the 1.7.6 fix, where a stalled write drops the link so a half-written publish is discarded (mqtt_desync_drops). With a clean session the broker keeps no subscription state and queues nothing, so a command HA publishes to the set/ topic during the gap is lost.
Open questions:
- Do jaronbor's mqtt_desync_drops / mqtt_sndbuf_skips counters in /api/v2/device/info rise at those times?
- Is a persistent session (clean session off, QoS 1 subscription to set/#) feasible on the ESP8266 heap, and does the PubSubClient fork support it?
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The cause of each disconnect is identified from the reporter's counters or a capture (deliberate desync close versus something else)
- [ ] #2 Decided with the maintainer whether commands during a reconnect gap must survive (persistent session plus QoS 1, or a documented limitation)
- [ ] #3 If a change is made: reproduced old vs fix on the bench (a set/ command published during a forced reconnect is applied on the fix and lost on the old build); build green for esp8266; evaluate.py --quick shows no new failures
<!-- AC:END -->
