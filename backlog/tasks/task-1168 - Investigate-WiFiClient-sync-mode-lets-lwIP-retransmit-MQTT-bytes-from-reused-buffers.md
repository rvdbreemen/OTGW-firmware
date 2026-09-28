---
id: TASK-1168
title: >-
  Investigate: WiFiClient sync mode lets lwIP retransmit MQTT bytes from reused
  buffers
status: Done
assignee:
  - '@claude'
created_date: '2026-09-27 07:48'
updated_date: '2026-09-28 06:00'
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
- [x] #1 The heap cost of setSync(false) is measured on the bench (free heap and max block during a discovery burst), not estimated
- [x] #2 Whether a >300 ms stall followed by buffer reuse changes retransmitted bytes is demonstrated (bench or host) or ruled out, with evidence
- [x] #3 A decision (keep sync, drop sync, or copy before write) is recorded with the numbers
- [x] #4 Bench A/B experiment run with setSync(true) and setSync(false) on the same build and load, reporting free heap, max free block and heap fragmentation for both (idle, discovery burst, 5-minute housekeeping burst)
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

2026-09-28 CORRECTION of the task premise: the buffer-reuse hazard does not exist. Core 2.7.4 builds lwIP with LWIP_NETIF_TX_SINGLE_PBUF 1 (tools/sdk/lwip2/include/lwipopts.h:1670, "needed by esp8266 physical layer"), and lwIP tcp_write() then forces TCP_WRITE_FLAG_COPY whatever the caller passes (lwIP STABLE-2_1_2 src/core/tcp_out.c:422-425). ClientContext.h:518 leaving the flag off in sync mode therefore changes nothing: data is always copied. setSync only decides whether write() additionally waits (wait_until_sent, up to 300 ms) for the ACK.

Bench A/B (.88.68, same source, only setSync differs: A = 1.7.6-beta.7+63a2f72 true, B = local beta.8+74aa2a7 false, not committed). Per variant: clean 480 s (120 s idle, discovery burst every 60 s) + stall 600 s (receive window closed 5.5-9 s mid discovery burst). device/info every 1 s, phases from broker publish timestamps. Scratchpad ram_syncT_summary.json / ram_syncF_summary.json.
- Free heap avg (true / false): idle 17942 / 18071, discovery 17914 / 17869, stall-run discovery 17808 / 17893, during stalls 17631 / 17549, housekeeping 17915 / 17643.
- Max block avg: idle 17460 / 17662, discovery 17432 / 17464, during stalls 17189 / 17261.
- Minimums scatter both ways by up to ~1.4 KB (e.g. discovery 16336 / 15760, stall-run discovery 15328 / 15664); fragmentation avg 6-7 % / 6 %, max 20 % / 15 %.
- Differences are within run-to-run noise at 1 s sampling; no consistent winner. The "~1 KB saved" in commit 0d6942a9f is not visible.
- Integrity: 0 corrupt / malformed / non-UTF-8 / foreign topic / invalid JSON in either variant (A 1830, B 1806 publishes). Desync drops in the stall run: true 2, false 5 (small n).
- Bench restored to the released v1.7.6-beta.7 firmware afterwards.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decision: keep wifiClient.setSync(true). No code change.

Why:
- The feared defect does not exist: lwIP on core 2.7.4 always copies TCP data (LWIP_NETIF_TX_SINGLE_PBUF forces TCP_WRITE_FLAG_COPY in tcp_write), so sync mode cannot send bytes from reused caller buffers. Bench: 0 integrity faults in ~3600 publishes across both variants, including 39 mid-burst stalls.
- RAM: measured A/B on the bench with identical source and load, free heap and max block differ by less than the run-to-run noise in every phase (idle, discovery burst, housekeeping burst, stalls). Neither option uses measurably less memory.
- With no RAM or correctness gain, the tie goes to the shipped configuration: no change, no retest, and in the stall run sync=true saw fewer desync drops (2 vs 5, weak evidence).

Correction: the task description and the TASK-1166 note calling sync mode a latent buffer-reuse issue were wrong; see the correction note.
<!-- SECTION:FINAL_SUMMARY:END -->
