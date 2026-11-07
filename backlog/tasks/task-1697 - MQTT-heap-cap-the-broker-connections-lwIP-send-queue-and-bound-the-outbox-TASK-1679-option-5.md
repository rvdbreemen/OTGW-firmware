---
id: TASK-1697
title: >-
  MQTT heap: cap the broker connection's lwIP send queue and bound the outbox
  (TASK-1679 option 5)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T16:53:00Z'
updated_date: '2026-11-07T23:03:00Z'
labels:
  - mqtt
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 812000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1679 (Robert 2026-11-07). Cause A/B on .61: with MQTT off the two-tab minimum is 42-47 KB, with MQTT on 13-26 KB. beta.123 diag: the espMqttClient outbox reaches 48-49 messages after every boot and holds 0-41 messages at the dip, plus up to 4.9 KB queued in lwIP on the broker pcb (uncapped, TCP_SND_BUF 5744). Apply platformTcpCapSendBuffer (2872 B) to the broker pcb and bound the outbox (count or bytes) so publishes beyond it are deferred or dropped by the existing ADR-088 burst machinery, not queued. Outbox byte size needs a library change in the vendored espMqttClient (ADR-186/187): ask Robert before touching it. A burst pause during file streams conflicts with ADR-088 (binding) and is out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Two-tab A/B on .61 with MQTT on (5 runs): minimum free heap higher than beta.123 (8.1-18.8 KB), no MQTT messages lost beyond the existing drop counters
- [ ] #2 hd_mqtt_queue_hwm bounded at the new limit; ADR-088 gates in evaluate.py green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-07 (session): measure first. Two-tab A/B (TASK-1679 recipe, 5 runs, MQTT on) on beta.131, which already lowered the boot outbox peak 49 -> 17 (TASK-1699). Compare with beta.123 (min free 8.1-18.8 KB). Only if the dip still sits with MQTT, choose between an app-level outbox bound (new ADR partly superseding ADR-167, lost publishes counted) and a socket send cap in the vendored espMqttClient (ADR-186/187, needs Robert's OK).

2026-11-08 measure-first (Robert's choice) on .61 beta.133+d6407b1 (carries TASK-1699), same two-tab A/B as TASK-1679 (5 reboots, MQTT on, scratchpad b103/mqttdiag_ab.sh, log mqttdiag_b133.log): hd_min_free_heap 27884, 30740, 27820, 16972, 32440 (beta.123: 18784, 8080, 18680, 12212, 15900); min max block 31732 in every run (beta.123 10740-31732); outbox at the dip 0-9 (beta.123 0-41), hwm 17 (48-49); p95 1.61-1.84 s (2.09-4.44 s); 0 x 503, 2 timeouts in run 1 (beta.123 0). The deepest run (r4, 16972) had an empty outbox and 2 file streams in flight: web path, TASK-1696.

Robert 2026-11-08 (session, AskUserQuestion): close, solved by TASK-1699; AC2 (a new outbox bound) dropped, so no ADR partly superseding ADR-167 and no espMqttClient change. Not measured: an MQTT burst outside boot (broker reconnect, HA birth -> markAll).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed 2026-11-08 without a code change (Robert). AC1 met through TASK-1699 (beta.131): two-tab A/B on .61 beta.133, min free heap 17.0-32.4 KB (median 27.9) vs beta.123 8.1-18.8 KB (median 15.9), no new MQTT drops, outbox at the dip 0-9 vs 0-41. AC2 dropped by Robert: no outbox bound is added (it would partly undo ADR-167 and need a vendored-library change for the socket cap). Not verified: bursts outside boot (reconnect, HA birth). Remaining web-path dip: TASK-1696. Note: run 1 had 2 timeouts (beta.123 had 0); not investigated.
<!-- SECTION:FINAL_SUMMARY:END -->
