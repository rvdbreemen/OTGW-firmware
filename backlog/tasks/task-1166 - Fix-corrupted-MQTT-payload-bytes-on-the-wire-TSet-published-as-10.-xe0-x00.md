---
id: TASK-1166
title: 'Fix: corrupted MQTT payload bytes on the wire (TSet published as 10.\xe0\x00)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-26 15:04'
updated_date: '2026-09-27 07:48'
labels:
  - bug
  - mqtt
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 235000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 (GH #682, 2026-09-26, on 1.7.6-beta.5) saw Home Assistant reject a payload on OTGW/value/<id>/TSet: b'10.à
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The code path that publishes TSet is traced from formatting to write, with file:line, and every buffer it passes through is shown to outlive the write (or the defect is identified)
- [ ] #2 The failure is reproduced on the bench or the mechanism is demonstrated in a host test, before any fix
- [ ] #3 Fix verified: after the fix, a forced short write/retry on the payload path cannot put bytes on the wire that differ from the formatted payload
- [ ] #4 Reporter informed on GH #682
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause: mqttDropLinkOnDesync() (mqtt_configuratie.cpp:2272) calls PubSubClient::disconnect(), which writes the 2-byte DISCONNECT packet E0 00 into the unfinished PUBLISH before closing. The broker reads it as payload (or as header bytes on the header-short path).

Scope: otgw-1.x.x only (2.0.0 has no mqttDropLinkOnDesync; checked ../OTGW-firmware). No version bump (release-prep only). No push.

1. Host test first (test/host/test_mqttPayloadDesyncDisconnect.cpp, added to run_tests.bat), against the REAL vendored PubSubClient: fake client with a TOTAL byte budget (exactly N bytes then stall) plus a mini broker-side parser that returns completed PUBLISH frames. Red case: payload 2 bytes short + PubSubClient::disconnect() -> parser delivers topic .../cooling_enable with payload 4F E0 00 (field symptom, byte for byte). Also 1-short (00 parsed as next header) and header-short.
2. Fix: mqttDropLinkOnDesync() closes the transport (wifiClient.stop(), extern in the .cpp) and writes nothing. Counter unchanged. PubSubClient::connected() then sees the dead socket and moves _state CONNECTED -> CONNECTION_LOST; handleMQTT() reconnects on connected()==false as today.
3. Green cases: after a transport stop, the parser delivers no frame for 1-, 2-, >2-short and header-short; nothing is appended after the truncation point; connected()==false and a later publish writes nothing.
4. Correct the existing test_mqttBeginPublishDesync case 2, which models the drop as mqtt.disconnect() (the defect).
5. build.bat (firmware+fs), evaluate.py --quick, run_tests.bat.
6. Reporter reply on GH #682 after build (AC#4); a retained publish hit by this may stay corrupt on the broker until the next republish.

Limit: mqttDropLinkOnDesync itself is not compiled on the host (mqtt_configuratie.cpp is too entangled); the test proves the transport-close contract the new body uses. Stated in Final Summary.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-26: asked mrfox7688 on GH #682 for device/info counters (desync_drops, sndbuf_skips, uptime, fwversion), recurrence and topics (HA log "Can't decode payload"), the Mosquitto log around 10:45:47, and a capture-otgw.sh transcript.

2026-09-26 19:56 UTC: mrfox7688 answered on GH #682 (1.7.6-beta.5+58d490d, uptime 1d 00:36):
- mqtt_sndbuf_skips 5548 (~3.8/min, up from ~1.6/min reported earlier), mqtt_desync_drops 13.
- 3 more "Can't decode payload" hits 18:09-18:42, all b'Oà

2026-09-27 root-cause trace (AC#1):
- Publish path: sendMQTTData() MQTTstuff.ino:1132 -> beginMqttPublish() :426 (PubSubClient::beginPublish writes header+topic) -> writeMqttChunk() :299 (payload straight from caller pointer via MQTTclient.write) -> endPublish (no-op). On a short payload write, :1151 calls mqttDropLinkOnDesync() (mqtt_configuratie.cpp:2272) = client.disconnect().
- PubSubClient 2.8 disconnect() (libraries/PubSubClient/src/PubSubClient.cpp:660) writes MQTTDISCONNECT = bytes E0 00 BEFORE stop(). Mid-frame, the broker reads those 2 bytes as the rest of the promised payload.
- Matches every hit: payload length right, last 2 bytes E0 00. Exactly 2 bytes missing -> PUBLISH completes with E0 00 and is delivered to HA; 1 missing -> E0 ends payload and 00 is parsed as next packet header (malformed); >2 missing -> broker waits, gets EOF, discards. The "OFF" payload is a string literal, so the source buffer cannot be the corruption: the bytes are the DISCONNECT packet.
- So the TASK-769/1134/1155 desync remedy itself injects the corruption.
- Separate latent issue found: wifiClient.setSync(true) (MQTTstuff.ino:709, commit 0d6942a9f, since v1.4.1) makes lwIP tcp_write reference caller memory without TCP_WRITE_FLAG_COPY (core 2.7.4 ClientContext.h:518). wait_until_sent() gives up after 300 ms without progress, write() still returns the full count, caller buffer (static/stack/PubSubClient buffer) is then reused while unacked bytes may still be retransmitted from it.
<!-- SECTION:NOTES:END -->
