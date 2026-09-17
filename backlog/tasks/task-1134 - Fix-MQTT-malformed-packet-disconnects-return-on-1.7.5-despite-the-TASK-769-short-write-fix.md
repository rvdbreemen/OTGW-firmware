---
id: TASK-1134
title: >-
  Fix: MQTT malformed-packet disconnects return on 1.7.5 despite the TASK-769
  short-write fix
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-17 20:20'
updated_date: '2026-09-17 20:48'
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
- [ ] #7 A host test compiled against the real PubSubClient reproduces the desync (partial header on the wire, link still up, next packet appended) and shows the caller contract prevents it, without dropping a healthy connection
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Falsify or confirm the re-entrancy hypothesis by tracing every yield/feedWatchDog/delayms site inside the chunked publish path (AC1).
2. If falsified, find the real desync source in the vendored PubSubClient and verify the premise it rests on against the ESP8266 core the firmware builds with (AC3).
3. Fix at the caller contract, one edit per file: beginMqttPublish() for the MQTTstuff.ino paths, one shared helper for the nine composer call sites in mqtt_configuratie.cpp.
4. Prove defect and remedy with a host test compiled against the REAL PubSubClient.cpp and a short-writing fake client (AC7).
5. Build, evaluate, commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 verdict: NOT reachable. The re-entrancy hypothesis in the task description is falsified.
- feedWatchDog() (OTGW-Core.ino:957-978) does I2C watchdog feed plus blinkLEDnow(); its own yield() is commented out at line 977.
- delayms() (helperStuff.ino:1296-1301) is the only caller of doBackgroundTasks(), and it is not on any publish path.
- yield() on ESP8266 switches to the SDK cont task and never re-enters loop(), so no user code and therefore no MQTTclient.loop() runs inside writeMqttChunk.
- handleMQTTcallback() (MQTTstuff.ino:624) does not publish; requestMQTTRepublishAll() only sets gates.
PINGREQ cannot be emitted from inside a publish. The reporter did see one adjacent to the corruption, but as a neighbour, not as the interleaver.

AC3: real desync source found in the header write, not the payload.
PubSubClient::beginPublish() (libraries/PubSubClient/src/PubSubClient.cpp:265-280) writes the fixed header plus topic with ONE _client->write() and returns rc == expected. On a short write it has already put bytes on the wire and returns false. Every one of the 12 call sites treated that false as nothing-happened and returned without dropping the link, so the connection stayed up carrying half a PUBLISH header. Whatever went out next (a PINGREQ, or the next publish) is read by the broker as the payload that header promised: malformed packet.

Premise verified one layer down, not assumed: ESP8266 core 2.7.4 ClientContext.h::_write_from_source (line 455) breaks its loop on _is_timeout() and returns _written, so a partial count from _client->write() is real. The 5 s socket timeout is reached when the lwIP send buffer stays full, which is a backpressure condition and needs no low heap. That fits #682: two reporters on different routers, no heap complaints, failures during bursts of small publishes where the header dominates the write.

Why TASK-769 did not cover this: it hardened the PAYLOAD half (writeMqttChunk/writeMqttProgmemChunk plus the composer failure branches) and left the header half open.

Note for future readers: PubSubClient::endPublish() is "return 1;" unconditionally, so every "if (!client.endPublish())" branch in MQTTstuff.ino and mqtt_configuratie.cpp is dead code. Left untouched here, unrelated surface.
<!-- SECTION:NOTES:END -->
