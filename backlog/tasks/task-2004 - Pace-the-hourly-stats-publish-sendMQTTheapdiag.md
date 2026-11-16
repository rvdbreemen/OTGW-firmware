---
id: TASK-2004
title: Pace the hourly stats publish (sendMQTTheapdiag)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T13:26:00Z'
updated_date: '2026-11-16T17:15:00Z'
labels:
  - mqtt
  - heap
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1025000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
At every wall-clock hour (ADR-086, doTaskMinuteChanged -> if (hourFlag) -> sendMQTTheapdiag(), OTGW-firmware.ino:998-1001) the firmware publishes 26 retained otgw-firmware/stats/* topics back to back (publishStatU32 -> sendMQTTData, MQTTstuff.ino ~2305-2330), with no burst window. 09 on .61 (OTGW32, no PSRAM, beta.351 bench build, 2026-11-16): hd_min_free_heap fell from 19,876 B to 2,180 B (below CRITICAL 3,072) at 14:00:00.002-.040, followed by '[drip] slowed to 10s (heap pressure)'; no reboot, free heap back at once. .63 (Classic with PSRAM, beta.350) showed no dip (hd_min 35.5 KB). The ADR-088 burst window only keeps the discovery drip out (MQTTstuff.ino:108-160); it does not reduce the 26 queued payloads. canPublishMQTT() blocks at CRITICAL (ADR-167), so the tail of the burst may be dropped. Fix: pace the stats like the discovery drip (a cursor, N per loop pass or one per ~100 ms), flat topics stay (ADR-101). Coordinator 2026-11-16: priority medium, high if 09's 15:00 mark on .61 with plain dev (352) shows the same dip; after the 20:00 beta tag.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduction red on the old code: hd_min drop at hh:00 on a board without PSRAM, plus the count of the 26 stats/* topics that reach the broker
- [x] #2 Host test on the real pacer (order, all topics once, respects canPublishMQTT)
- [ ] #3 Bench on .61 by the bench-tester
- [ ] #4 Fix green on the same reproduction (old vs fix on the same quantity, the old 353 went hd_min_free_heap 31,156 -> 6,484 at 16:00): on .61 with SAT enabled, hd_min_free_heap is unchanged across an hh:00 mark, the telnet free-heap prefix hh:59:50-hh:00:05 shows no drop of tens of KB, and 26/26 stats/* topics reach the broker
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Fix in MQTTstuff.ino: sendMQTTheapdiag() only starts a round (cursor 0); loopMQTTStatsPacer(), called each loop pass next to loopMQTTDiscovery(), sends one stat via publishStatAt(i) (a switch over the 26 existing publishStatU32 calls, same topics, ADR-101 flat topics, no JSON) at least 100 ms apart; while getHeapHealth() == HEAP_CRITICAL it waits instead of letting canPublishMQTT() drop it; a disconnect ends the round (next hour). Values are read when sent. Host test tests/test_mqtt_stats_pacer.py slices the real functions. Bench: hour mark on .61 with SAT enabled, criterion hd_min_free_heap stays above ~40 KB and 26/26 topics arrive.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (95) taken over from ba (coordinator). Status of the reproduction (ba): 14:00 dip to 2,180 B on .61 with ba's 351+597c163 and SAT enabled/inactive; 15:00 on 352 with SAT off: no dip, all 26 topics arrived. 09 runs the 16:00 mark on 353 with SAT enabled (inactive, no sim). Per the bug rule (reproduce, then prove) no code before that result: if 16:00 shows no dip, the 14:00 one is not reproduced and AC1 is open. Planned fix if it reproduces: a table/switch of the 26 stats driven by a cursor, one publish per ~100 ms from the loop (like the discovery drip), skipped while !canPublishMQTT(); flat topics stay (ADR-101); host test on the real pacer (order, every topic once, respects canPublishMQTT).

2026-11-16 16:0x reproduction (09 on .61, beta.353+3a14423, SAT enabled/inactive, no boiler; 09 scratchpad hour1600\): free heap 63,396 B at 15:59:59.577 (telnet prefix), hd_min_free_heap 31,156 -> 6,484 over the 16:00 mark; no reboot, no drops, no '[drip] slowed'; 26/26 stats/* on the broker, first at 16:00:00.757; the ONLY publishes in 16:00:00-03 were those 26 (telnet 'Sending MQTT'); no SAT topic, discovery or weather in that second. Loop dip snapshot hd_dip_free_heap 13,388 (max block 7,668, mqtt queue 10, mqtt tx 1,096 B): the 6,484 minimum lies inside a call the loop does not sample. Earlier marks: 14:00 (351, SAT enabled) 56,964 -> 2,180 (~54.8 KB); 15:00 (352, SAT off) 63,516 before, hd_min 19,260 -> 19,124 (a drop of at most ~44.4 KB, not measurable below the older watermark). Baseline before the mark was the same with SAT on and off (63.4 vs 63.5 KB), so the burst itself looks ~12 KB larger with SAT enabled: NOT explained (one sample each).

Mechanism, labelled: CONFIRMED from the S3 framework config (D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/*/include/sdkconfig.h): CONFIG_LWIP_TCP_OVERSIZE_MSS 1, CONFIG_LWIP_TCP_SND_BUF_DEFAULT 5744, CONFIG_LWIP_TCP_MSS 1436. INFERENCE, not verified: each small publish written and sent as its own segment holds an MSS-sized pbuf (~1.5-1.7 KB) until the broker ACKs it, 26 x ~1.7 KB = ~44 KB. Caveat against it: with SND_BUF 5744 lwIP's default send queue length (about 4 x SND_BUF / MSS = ~16 pbufs) should cap the unacked pbufs below 26, so the pbuf arithmetic alone may not reach 44-57 KB; the espMqttClient outbox (26 small packets, a few KB) adds to it. The exact split is not measured. The fix does not depend on it: pacing lets each send's buffers be freed before the next one; the bench at an hour mark decides.

AC wording (advisor 2026-11-16): hd_min_free_heap is a lifetime watermark (31,156 B already 45 min after boot on 353), so an absolute threshold (~10 KB or ~40 KB) cannot tell old from fix; AC2 is restated as 'unchanged across hh:00' on the same board and SAT setting. Callers checked (untruncated git grep over src/tests/scripts/evaluate.py/docs): sendMQTTheapdiag() has one caller, the hourly block in doTaskMinuteChanged(); evaluate.py's 'sendMQTTheapdiag arithmetic' check reads the body and passes when it holds no char[N] buffer (unchanged).

AC1 evidence: 09's 16:00 run above (old code = 353: hd_min 31,156 -> 6,484, 26/26 topics). AC2 evidence: tests/test_mqtt_stats_pacer.py (slices publishStatU32 + sendMQTTheapdiag + publishStatAt + loopMQTTStatsPacer from MQTTstuff.ino): FIX 6/6 OK; OLD (OTGW_MQTT_REV=HEAD, 3a14423f8-line code) 4 FAIL: 'sendMQTTheapdiag() published 26 topics in one call (burst)', gaps all 0 ms, 26 dropped under CRITICAL instead of waiting, 26 sent despite the mid-round disconnect.

Gates on the rebased lane (onto origin/dev 1dde4dcda, beta.352-355 for 1665/1999/1751/2004), 2026-11-16 17:14-18:15, foreground in chunks of 8 (scripts/run_tests.py --no-host -j 2): 292 tests/ files, 290 PASS, 2 FAIL that fail identically on origin/dev and are not from this lane: test_adr_governance test_live_index_complete ('ADR-252 (Proposed) is MISSING from docs/adr/README.md', d3) and test_settings_float_roundtrip ('SATpwmswitchmargin: no clamp range found in updateSetting()', after TASK-1942 b95bc21ec). Host harnesses for the touched areas: test_diag_recorder, test_diag_recording_routes, test_mqtt_broker_address, test_mqtt_print_error PASS. evaluate.py --quick: 87 passed, 0 failed. The rest of test/host/ was not run.
<!-- SECTION:NOTES:END -->
