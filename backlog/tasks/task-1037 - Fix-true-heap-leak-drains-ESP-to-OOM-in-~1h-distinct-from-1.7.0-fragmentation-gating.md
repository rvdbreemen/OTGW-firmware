---
id: TASK-1037
title: >-
  Fix: true heap leak drains ESP to OOM in ~1h (distinct from 1.7.0
  fragmentation gating)
status: To Do
assignee: []
created_date: '2026-07-19 09:45'
updated_date: '2026-07-19 15:24'
labels: []
dependencies: []
references:
  - >-
    Discord #nederlandse-ondersteuning msg 1527629013210632323 +
    1528302834876022795 (martreides
  - 2026-07-17/19); logs otgw-161.log
  - otgw-171.log
  - otgw-171-2.log
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-19: Analysed both 1.7.1 telnet captures plus the 1.6.1 full capture (transcript-20260719-094605, OTGW-logs).

Crash chain, as far as the logs support it:
1. Heap descends ~800-1000 B/min from ~20 KB, accelerating. Source still unknown.
2. Around 5 KB free, maxBlock drops under 2048, canServeHttp() closes and never reopens (see TASK-1039).
3. MQTT gate follows at maxBlock < 1536. Publishing stops, HA marks the device unavailable.
4. emergencyHeapRecovery fires with delta=+0 every time (see TASK-1038).
5. Heap reaches ~880 bytes, maxBlock ~480. MDNS.update() at OTGW-firmware.ino:427 is ungated and runs ~1000x/s.
6. Next inbound mDNS query hits the unchecked new in _readRRAnswer(), gets NULL, constructor writes at this+8: epc1=0x40233cba excvaddr=0x00000008. Matches the decoded address exactly.

Ruled out for this reporter: the WS live-log path (emergencyHea actions never had bit 0x01 set, so no browser was connected) and NTP (the 1.6.1 capture ran with NTP off and decayed identically).

Speculative, not proven: mDNS may be both the leak and the crash site. LEAmDNS keeps per-query answer lists that grow, which fits the accelerating rather than linear decay; it is identical Core 2.7.4 code in 1.6.1 and 1.7.1, which fits version independence; and it is the confirmed crash site. Cheap discriminating test is a 1.7.1 build with MDNS disabled: flat heap means mDNS is both, continued decay without crashes means mDNS is only the crash site and we keep a live device to measure on.

Note on the 1.6.1 full capture: it is NOT usable as leak evidence. The capture script drives a headless Edge at 365 REST requests/min, and that run shows the fragmentation profile (maxBlock pinned at 5352 while free stayed 10-13 KB), not the leak ramp. Also found: his broker holds stale retained HA discovery from the 1.7.1 era (sw_version 1.7.1+c50cbcc on hvac_mode, hvac_action, uptime, fragskips, *_override) that the running 1.6.1 never fills, so those entities sit unavailable in HA independently of the reboots.

Related: TASK-1038 (recovery no-op), TASK-1039 (HTTP gate latch).

Discriminating experiment tracked as TASK-1040 (build 1.7.1-no-mdns.1).
<!-- SECTION:NOTES:END -->
