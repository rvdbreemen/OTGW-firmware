---
id: TASK-1037
title: >-
  Fix: true heap leak drains ESP to OOM in ~1h (distinct from 1.7.0
  fragmentation gating)
status: To Do
assignee: []
created_date: '2026-07-19 09:45'
labels: []
dependencies: []
priority: high
ordinal: 156000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by martreides in Discord #nederlandse-ondersteuning (2026-07-17..19). OTGW V2.13 + WeMos D1 mini, PIC 6.6, reboots every 1-1.5h, HA unavailable. Three telnet captures (1.6.1 and 1.7.1) show an identical signature: heap flat around 20 KB for ~35-40 min after boot, then a monotone ramp down of roughly 800-1000 bytes/min until OOM about 15-20 min later, followed by Exception (2) epc1=0x40233cba excvaddr=0x00000008 (the known null 'new stcMDNS_RRAnswer' in LEAmDNS under OOM - a downstream symptom, not the cause).

This is NOT the fragmentation problem 1.7.0 gated. Evidence: free heap and maxBlock fall together (frag stays ~3%), and emergencyHeapRecovery reports 'before=888 after=888 delta=+0 actions=0x06' - recovery reclaims zero bytes, meaning everything allocated is still referenced. The 1.7.0 gates (canPublishMQTT, canServeHttp) are visibly working in the 1.7.1 capture and still cannot prevent death, because they throttle MQTT/HTTP/WS but do not gate mDNS allocations.

Cross-checks: 1.6.1 capture ran with NTP off and shows the same decay, so NTP/timezone lookup is exonerated. Version-independent across 1.6.1 and 1.7.1 points at a vendored library or Arduino Core leak rather than firmware feature code. Possibly related to the earlier geo83_44083 reports.

Ramp onset has no logged event in any of the three captures; all three were captured with a telnet client attached (and one with the web UI open, verifyAccess visible), so the leaking subsystem cannot be isolated from these logs alone.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Leak reproduced locally or on a bench device with a heap trace that identifies the allocating subsystem
- [ ] #2 Confirmed whether the leak persists with no telnet client and no web UI open (capture-mqtt-debug + USB serial from reporter)
- [ ] #3 Root cause identified at file/function level and documented with evidence
- [ ] #4 Fix keeps free heap stable over a 4h+ soak on a device previously showing the ramp
- [ ] #5 Reporter martreides confirms no reboots over 24h on a build with the fix
<!-- AC:END -->
