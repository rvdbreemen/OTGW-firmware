---
id: TASK-1212
title: >-
  Investigate: a Home Assistant command sent during a short MQTT reconnect is
  lost (GH #682 follow-up)
status: To Do
assignee: []
created_date: '2026-10-04 19:34'
updated_date: '2026-10-20 02:22'
labels:
  - bug
  - needs-info
  - mqtt
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 research (no code change):
- Feasibility: the build uses knolleary PubSubClient 2.8 (build.py lib list; D:/Users/Robert/Documents/Arduino/libraries/PubSubClient). It has connect(..., willMessage, cleanSession) (PubSubClient.h:149), subscribe(topic, qos) with qos 0..1 (:175, .cpp:609-631) and answers an incoming QoS 1 PUBLISH with PUBACK (.cpp:407). The firmware calls the 7-argument connect (clean session) at MQTTstuff.ino:971/976 and subscribes at QoS 0 (:287, :1028). A persistent session therefore needs no library change and no ESP heap (the broker holds the queue).
- Trade-off for AC #2 (maintainer decision): with a persistent session the broker also queues commands across LONG outages (Mosquitto 3.1.1 queues have no expiry, default max_queued_messages 1000), so a setpoint sent hours ago is applied on reconnect, and a burst of queued set/ messages arrives at once. Options: (A) persistent session + QoS 1 on set/#; (B) keep clean session and document that commands during a reconnect gap are lost (HA shows the state the gateway reports, so the user sees it did not apply).
- New field data, GH #682 mrfox7688 2026-10-19 on beta.2: daytime WiFi loss (87 router-ping episodes of ~2 min, RSSI -73..-75), Mosquitto 239 gateway disconnects (102 session taken over, 77 Success, 44 exceeded timeout, 16 closed by client), mqtt_desync counters not reported. "session taken over" = a reconnect with the same client id while the broker still held the old socket, so most gaps here are WiFi, not the deliberate desync close. AC #1 for jaronbor's two disconnects still needs his counters.
<!-- SECTION:NOTES:END -->
