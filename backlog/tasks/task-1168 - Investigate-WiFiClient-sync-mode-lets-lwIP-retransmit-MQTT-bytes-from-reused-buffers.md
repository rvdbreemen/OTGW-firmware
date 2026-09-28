---
id: TASK-1168
title: >-
  Investigate: WiFiClient sync mode lets lwIP retransmit MQTT bytes from reused
  buffers
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-27 07:48'
updated_date: '2026-09-28 04:47'
labels:
  - bug
  - mqtt
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: medium
ordinal: 237000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while tracing TASK-1166. startMQTT() sets wifiClient.setSync(true) (MQTTstuff.ino:709, commit 0d6942a9f, in every release since v1.4.1). In core 2.7.4 sync mode makes ClientContext::_write_some() call tcp_write() WITHOUT TCP_WRITE_FLAG_COPY (ClientContext.h:518), so lwIP references the caller's memory instead of copying it. write() then waits in wait_until_sent(), which gives up after 300 ms without progress (WIFICLIENT_MAX_FLUSH_WAIT_MS) and still returns the full byte count. The caller's buffer (PubSubClient buffer for header+topic, static/stack buffers for payloads) is reused afterwards, while unacked bytes can still be retransmitted from it. Under a stalled link that can put wrong bytes on the wire: a plausible second source of the GH #682 malformed-packet drops.

The commit rationale ("eliminates the TCP_SND_BUF temporary copy in WiFiClient, ~1 KB") is unverified: in core 2.7.4 the non-sync path copies into lwIP pbufs only while data is unacked, not into a WiFiClient buffer. Deciding between sync off (heap cost during bursts) and keeping it is a RAM-versus-correctness call for the maintainer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The heap cost of setSync(false) is measured on the bench (free heap and max block during a discovery burst), not estimated
- [ ] #2 Whether a >300 ms stall followed by buffer reuse changes retransmitted bytes is demonstrated (bench or host) or ruled out, with evidence
- [ ] #3 A decision (keep sync, drop sync, or copy before write) is recorded with the numbers
- [ ] #4 Bench A/B experiment run with setSync(true) and setSync(false) on the same build and load, reporting free heap, max free block and heap fragmentation for both (idle, discovery burst, 5-minute housekeeping burst)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Same firmware source, only wifiClient.setSync() differs: A = beta.7 as shipped (true, 1.7.6-beta.7+63a2f72), B = local build with setSync(false), firmware-only OTA, not committed.
2. Per variant, identical load (scratchpad variant.sh): clean run 480 s (120 s idle, then POST /api/v2/otgw/discovery every 60 s; 5-minute housekeeping burst occurs naturally) + stall run 600 s (stall broker closes its receive window 5.5-9 s mid discovery burst).
3. device/info sampled every 1 s: freeheap, maxfreeblock, hd_fragmentation_pct. Samples classified by phase using broker publish timestamps (idle / discovery / housekeeping / stall).
4. Integrity per run (AC#2): broker checks every PUBLISH for foreign topics, invalid discovery JSON, non-UTF-8, E0 00 tails, reserved packet types.
5. Compare A vs B per phase; decision with the numbers for the maintainer.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-27 (Robert): parked until a 1.x bench is available. Then run the setSync true/false experiment on the bench. RAM impact MUST be measured and weighed in the decision; no decision on correctness alone.
<!-- SECTION:NOTES:END -->
