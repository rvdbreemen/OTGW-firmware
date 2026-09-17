---
id: TASK-1134
title: >-
  Fix: MQTT malformed-packet disconnects return on 1.7.5 despite the TASK-769
  short-write fix
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-17 20:20'
updated_date: '2026-09-17 20:32'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 217000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported on GitHub #682 by mrfox7688 (Nodoshop ESP8266, 1.7.5+db658f8, PIC 6.8, Mosquitto 7.1.1) and independently corroborated by jaronbor (Fritzbox, different network). Mosquitto drops the client with 'disconnected: malformed packet' during a burst of small OTGW/value/<id>/... publishes, with a PINGREQ interleaved in the same burst. Reconnect follows ~42s later; that constant is timerMQTTwaitforconnect (MQTTstuff.ino:775), so it is the firmware backoff, NOT evidence about WiFi. mrfox7688 falsified his own router hypothesis: ASUS RT-AX88U roaming/OFDMA disabled gave 16h stability, then the fault recurred on 2026-09-16 19:31:02 with those settings unchanged.

TASK-769 (Done) fixed truncated-payload desync by disconnecting instead of calling endPublish() on a short write. 1.7.5 carries that fix, so this is a different desync source. Leading hypothesis: a second writer entering the socket mid-PUBLISH. The chunked path (writeMqttChunk / writeMqttProgmemChunk, MQTTstuff.ino:285-333) calls feedWatchDog() after every 128-byte chunk and feedWatchDog()+yield() on every short-write retry, all between beginPublish() and endPublish(). feedWatchDog() itself has its yield() commented out (OTGW-Core.ino:957-978), but helperStuff.ino:1300 calls doBackgroundTasks() -> handleMQTT() -> MQTTclient.loop() (MQTTstuff.ino:784, 942), which is what emits PINGREQ. If any publish path can reach that re-entry, PINGREQ bytes land inside a half-written PUBLISH and the broker resyncs at the wrong offset - exactly the reported signature.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Determine whether MQTTclient.loop() (and therefore PINGREQ) is reachable between beginMqttPublish() and endPublish(), by tracing every yield/feedWatchDog/delayms site inside the chunked publish path; record the verdict with file:line evidence either way
- [ ] #2 If reachable: a publish in progress cannot be interrupted by any other socket writer (guard, or PINGREQ deferred until the packet is complete)
- [ ] #3 If not reachable: the alternative desync source is identified from mrfox7688's Mosquitto log and named with evidence, and this task is re-scoped accordingly
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
- [ ] #6 Field validation by mrfox7688 and/or jaronbor on 1.x: no malformed-packet disconnects over at least 3 days
<!-- AC:END -->
