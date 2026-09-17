---
id: TASK-1134
title: >-
  Fix: MQTT malformed-packet disconnects return on 1.7.5 despite the TASK-769
  short-write fix
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-17 20:20'
updated_date: '2026-09-17 20:50'
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
- [x] #1 Determine whether MQTTclient.loop() (and therefore PINGREQ) is reachable between beginMqttPublish() and endPublish(), by tracing every yield/feedWatchDog/delayms site inside the chunked publish path; record the verdict with file:line evidence either way
- [x] #2 If not reachable: the alternative desync source is identified from mrfox7688's Mosquitto log and named with evidence, and this task is re-scoped accordingly
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
- [ ] #5 Field validation by mrfox7688 and/or jaronbor on 1.x: no malformed-packet disconnects over at least 3 days
- [x] #6 A host test compiled against the real PubSubClient reproduces the desync (partial header on the wire, link still up, next packet appended) and shows the caller contract prevents it, without dropping a healthy connection
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

Fix (2 files, 12 call sites, no duplication):
- MQTTstuff.ino beginMqttPublish(): MQTTclient.disconnect() added to the beginPublish failure branch. Covers sendMQTTData char*, the PROGMEM overload and the third site at line ~1331.
- mqtt_configuratie.cpp: new static beginDiscoveryPublish(client, topic, payloadLen) drops the link on failure; the nine composer call sites now route through it instead of calling client.beginPublish() bare.

Deliberate: the disconnect is unconditional on false. beginPublish collapses "wrote nothing" and "wrote part of the header" into the same false and the caller cannot tell them apart without changing the library, so the link is dropped either way. A needless reconnect costs one gap; a desynchronised stream costs every packet after it. Users on #682 will still see gaps, but a clean drop instead of a malformed-packet drop.

Evidence (AC7): test/host/test_mqttBeginPublishDesync.cpp, 15 checks, compiled against the REAL libraries/PubSubClient/src/PubSubClient.cpp with a fake Client that short-writes and replays a CONNACK. Case 1 reproduces the defect: beginPublish returns false, bytes ARE on the wire, connection still up, and a following publish is appended to the desynchronised stream. Case 2 shows the caller contract drops the link and nothing is appended. Case 3 shows a healthy connection is not dropped.
Harness: new test/host/pubsub_shim/ (Arduino.h, Client.h, Stream.h, IPAddress.h) emulates the platform only; run_tests.bat gained the test and an /I for the shim.

First run of the test failed 5 of 12 checks because beginPublish is gated on PubSubClient::connected(), which reports the SESSION state; without a CONNACK no bytes are ever written. Fixed by opening a real session in each case.

Validation run 2026-09-17:
- testun_tests.bat: 3 suites, 51 checks, 0 failures, exit 0 (18 jsonStuff + 18 dhwWaterMeter + 15 new).
- build.bat: "Build completed successfully", exit 0, 0 compile errors. Fresh artifacts OTGW-firmware-1.7.6-beta.3+b2b1b89.ino.bin and .littlefs.bin written 22:46:48 local.
- python evaluate.py --quick: 37 checks, 35 passed, 0 warnings, 0 failed, 2 info, health 100 percent, exit 0.
- version.h diff is build churn only (_VERSION_BUILD, githash, date/time); _VERSION_PRERELEASE stays beta.3, so no beta tag was spent.

Note on AC4 wording: build.bat was used, not python build.py --firmware. Project policy is that build.bat is the entrypoint and that firmware AND filesystem are built; build.bat covers the AC4 gate and more.

AC2 removed: it was the 'if reachable' branch of AC1 and AC3 is its mutually exclusive twin. AC1 falsified reachability, so AC2 described work that must not happen. Removed rather than left unchecked, so the remaining unchecked box is the genuine one (field validation).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closes the header half of the MQTT stream desync that TASK-769 only closed for payloads, and that came back as malformed-packet disconnects on 1.7.5 (GH #682).

What was wrong
PubSubClient::beginPublish() writes the fixed header and the topic to the socket with a single _client->write() and returns only whether the full count went out. On ESP8266 that write returns a PARTIAL count once the lwIP send buffer stays full until the 5 s socket timeout (core 2.7.4, ClientContext.h::_write_from_source returns _written after _is_timeout()). All 12 call sites treated the resulting false as "nothing happened" and returned with the link still up, leaving half a PUBLISH header on the wire. The broker then reads the next packet, a PINGREQ or the next publish, as the payload that header promised, and drops the client: "malformed packet".

This needs no low heap, only send-buffer backpressure, which is why two reporters on different routers hit it during bursts of small value publishes and why reducing MQTT traffic did not cure it. The 42 s reconnect gap they both measured is timerMQTTwaitforconnect (MQTTstuff.ino:775), the firmware backoff, not a network property.

The re-entrancy hypothesis the task was opened on was falsified first: feedWatchDog() has its yield() commented out, delayms() is the only doBackgroundTasks() caller and is not on a publish path, and yield() does not re-enter loop() on ESP8266. PINGREQ cannot be emitted from inside a publish.

What changed
- MQTTstuff.ino: beginMqttPublish() drops the TCP link when beginPublish fails. Covers all three MQTTstuff call sites.
- mqtt_configuratie.cpp: new beginDiscoveryPublish() helper does the same; the nine discovery composers route through it instead of calling beginPublish bare. One helper, not nine pasted disconnects, so a tenth composer cannot quietly miss it.

The disconnect is unconditional on false by design: beginPublish collapses "wrote nothing" and "wrote a partial header" into one false and a caller cannot separate them without forking the library. Dropping a connection that wrote nothing costs one reconnect; keeping one whose stream is desynchronised costs every packet after it. User-visible effect: the gaps #682 reporters complain about become clean drops instead of malformed-packet drops.

Tests
New test/host/test_mqttBeginPublishDesync.cpp, 15 checks, compiled against the REAL vendored PubSubClient.cpp with a fake Client that short-writes and replays a CONNACK. It reproduces the defect (bytes on the wire, link up, next publish appended to a desynchronised stream), shows the caller contract prevents it, and shows a healthy connection is not dropped. New test/host/pubsub_shim/ emulates the platform only.

Validation: run_tests.bat 51 checks 0 failures; build.bat exit 0 with fresh artifacts; evaluate.py --quick 37 checks, 0 failed, 100 percent.

Risk and follow-up
Unproven on hardware. Field validation by mrfox7688 and jaronbor over at least 3 days is the remaining acceptance criterion, so the task stays In Progress. A 13th call site added later would reintroduce the gap; a source-level guard for that is not part of this change. PubSubClient::endPublish() returns 1 unconditionally, so every "if (!endPublish())" branch in both files is dead code, noted but deliberately left alone.
<!-- SECTION:FINAL_SUMMARY:END -->
