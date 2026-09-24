---
id: TASK-1163
title: >-
  Investigate: mqtt_sndbuf_skips climbs on a busy OT bus with a healthy link (GH
  #682)
status: Done
assignee:
  - '@claude'
created_date: '2026-09-24 18:36'
updated_date: '2026-09-24 20:00'
labels:
  - bug
  - mqtt
  - investigation
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 232000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On beta.5 jaronbor (GH #682) reports mqtt_sndbuf_skips 71 -> 235 over 1h43m with mqtt_desync_drops 0, WiFi 92-94%, stable heap and no disconnects after boot. The guidance given on #682 (climbing after boot = stalling link) does not fit that data and was corrected on 2026-09-24. Hypothesis: the send-buffer pre-flight defers publishes during Status bursts on a busy bus. A deferred value publish is a lost update unless something republishes it, so this may mean Home Assistant misses or lags values. The beta.5 bench runs had no bus traffic, so the baseline was never measured.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Skip rate measured on the bench gateway with simulated OT bus traffic, with the gateway publishing to a broker that is subscribed to losslessly
- [x] #2 For each deferred publish it is established from code whether the value is lost, retried, or republished later (on-change, heartbeat, discovery drip), with file:line
- [x] #3 Measured: whether values published by the device differ from what a lossless broker subscription received during the run (lost updates counted)
- [x] #4 Conclusion and next step posted on GH #682, stated as measured fact or explicitly as hypothesis
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Bench measurement 2026-09-24 (192.168.88.68, 1.7.6-beta.5+e0e0d3b)

Setup: simulator replay of 40 cycles x 16 msgids (status 5x per cycle, every value changing each cycle), 750 ms/line, 17m45s. Gateway pointed at a local mosquitto (via a Python forwarder, the firewall blocks mosquitto.exe inbound), lossless mosquitto_sub on the broker itself. Broker restored to homeassistant.local:1883 afterwards; password never touched.

Results:
- mqtt_sndbuf_skips 2641 -> 2687 (+46), desync 0, MQTT connected throughout, heap 15-17 KB.
- OT value topics: 484 of 484 expected on-change updates arrived, each exactly on its 24 s cycle; zero lost, zero delayed. (A first comparison reported 3 missing: harness bug, it counted a 45th cycle that started after the simulator was stopped.)
- The skips come in steps of 10-12 about every 5 minutes, not with bus traffic. A 5-minute capture with only the MQTT debug toggle on saw 0 deferrals: it fell between two steps.

Source (code + broker log): do5minevent() (OTGW-firmware.ino:411-417) publishes uptime, versioninfo, stateinformation, overrides and publishAllPICsettings() back to back, ~30 publishes within one second. The send buffer (TCP_SND_BUF = 2*MSS) fills, mqttFrameFitsSndbuf() (MQTTstuff.ino:377-393) gives up after 10 yield() calls, which is shorter than one ACK round trip to the broker, and the tail is deferred. In 5 of 6 blocks the ~12 otgw-pic/settings/* topics never reached the broker; the hourly sendMQTTheapdiag() burst (MQTTstuff.ino:1323, via hourFlag) lost its tail the same way (disc_republish_triggered onward).

What a deferral does (AC #2):
- OT values: the throttle slot commits only if a send in that frame succeeded (OTGW-Core.ino:4510-4516, ADR-076), so a deferred single-topic value is retried on the next frame of that msgid. Delayed, not lost. Not observed on the bench: no value topic was deferred.
- Multi-topic OT frames: the commit criterion is "any send in this frame succeeded", so a partial deferral can commit the slot while one topic is unpublished. Code reading only, not observed.
- Non-OT publishes (sendMQTTData from the 5-min / hourly tasks, PIC settings, stats): no retry. Lost until the next run of that task, and in this measurement the same tail was deferred again at nearly every run.

This is a regression introduced by TASK-1154. Before it, beginPublish() blocked while the buffer drained (ClientContext write blocks while it makes progress), so these tails were published, only slowly. The pre-flight turned a short wait into a drop.

Relevance to GH #682: jaronbor ~1.6 skips/min is consistent with the 5-minute burst tail (8 per block); nothing in his data points at a stalling link.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
mqtt_sndbuf_skips climbing on a healthy link (GH #682) is explained, measured, and reported.

- Bench (simulated busy bus, lossless broker subscription, 17m45s): 484 of 484 OT value updates arrived on time, desync 0. OT values are not affected.
- Source: do5minevent() publishes ~30 topics in one burst; mqttFrameFitsSndbuf() gives up after 10 yield() calls, shorter than one ACK round trip, so the tail (otgw-pic/settings/*, ~12 topics) is dropped every 5 minutes; the hourly heapdiag burst loses its tail the same way. Non-OT publishes have no retry.
- This is a regression from TASK-1154: before it, beginPublish() blocked briefly and these topics arrived.
- Correction to the earlier "stalling link" guidance and the measured conclusion are posted on GH #682.

Follow-up: fix (time-bounded pre-flight wait with a short back-off after a deferral) is proposed, not yet approved or implemented.
<!-- SECTION:FINAL_SUMMARY:END -->
