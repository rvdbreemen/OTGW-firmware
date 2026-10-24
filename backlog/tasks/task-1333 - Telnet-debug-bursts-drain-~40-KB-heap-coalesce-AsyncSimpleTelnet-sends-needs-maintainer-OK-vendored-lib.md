---
id: TASK-1333
title: >-
  Telnet debug bursts drain ~40 KB heap: coalesce AsyncSimpleTelnet sends (needs
  maintainer OK, vendored lib)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T21:30:00Z'
updated_date: '2026-10-24T01:48:00Z'
labels:
  - heap
  - telnet
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 456000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1328. A burst of telnet debug output (the capture script turns on every debug toggle and sends 'q' read-settings + 'D' dump on each connect) drains free heap from ~72 KB to ~32 KB within 0.5 s after only ~10 KB of output, and the native watermark reaches 776 bytes on the OTGW32 bench (Sergeant D's unit: 732, alpha.474). Without a telnet client minFree stays ~24.7 KB. Mechanism (src/libraries/SimpleTelnet/src/AsyncSimpleTelnet.h write()/_flushTx): every write() does c->add(COPY)+c->send() immediately, so each debug fragment becomes its own small TCP segment with its own lwIP pbuf / WiFi dynamic TX buffer (CONFIG_ESP_WIFI_TX_BUFFER_TYPE 1, 32 dynamic; TCP_SND_BUF 5744) and AsyncTCP ack events. Not split yet which layer holds most. lwIP/WiFi are prebuilt (ADR-185 wrapper cannot reach them). Options: (A) coalesce in AsyncSimpleTelnet: only send() when the ring holds >= one MSS or on a short timer / line end (library change, needs Robert's OK); (B) firmware-side: debug writers build a whole line in one buffer and call write once per line (fewer, larger segments; no lib change); (C) capture script: do not flip all toggles / no q+D dump by default. Open: Sergeant D's unit went silent ~70 s after the dump and came back with 'Power on' (asked whether he power-cycled it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The layer that holds the heap is named (lwIP pbufs, WiFi TX buffers or AsyncTCP events) with a bench measurement
- [x] #2 A fix chosen by the maintainer lowers the watermark in the same bench scenario (all toggles on, q+D, emulator, MQTT): before 776 B, after measured
- [ ] #3 Whether the device can wedge under this burst is settled (Sergeant D's answer or a bench run of 10+ minutes)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Option A (maintainer 2026-10-24): coalesce telnet sends inside AsyncSimpleTelnet (submodule rvdbreemen/SimpleTelnet, branch + PR). Root cause found: the library sets setNoDelay(true) on server and client (AsyncSimpleTelnet.h:207, :305, since 42f5858, no recorded reason), so Nagle is off and every write -> add(COPY)+send() leaves as its own tiny segment with its own pbuf/WiFi TX buffer. Step 1 (smallest): setNoDelay(false) on both, so lwIP merges small writes into one segment while an ACK is outstanding. Step 2 only if step 1 is not enough: coalesce in write() (send only when the ring holds >= one chunk or on the next loop tick). Verify with the TASK-1328 reproduction on the bench (fresh reboot, temp broker, sat_boiler_emulator, telnet all toggles on, q + D): baseline minFree 776 B; record old vs new min_free_heap and the trace heap minimum, plus interactive echo latency of a telnet key (should stay usable). Commit in the submodule on a branch, push, open a PR; bump the OTGW submodule pointer to that commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 maintainer decision (Robert, in session): option A, coalesce sends inside the vendored AsyncSimpleTelnet (src/libraries/SimpleTelnet, submodule) - send only when a full segment is ready or after a short timer; this is explicit approval to change that vendored library. Note: SimpleTelnet is a git submodule; the change has to land in its own repo and the pointer bumped (feedback: submodule pointer must be pushed, ask before pushing the submodule repo).

2026-10-24 maintainer decision (in session): SimpleTelnet change goes on a branch in rvdbreemen/SimpleTelnet with a PR; the OTGW submodule pointer points at that branch commit; Robert merges the PR.

2026-10-24 evidence: cause narrowed to the send path: AsyncSimpleTelnet set TCP_NODELAY (Nagle off) on server and clients, so every add()+send() left as its own segment, each holding a heap pbuf and a WiFi dynamic TX buffer until ACKed (which of the two dominates was not split; turning Nagle on removes the effect, which pins it on per-segment send buffering). Fix (option A): SimpleTelnet gets setNoDelay(bool) (default true, unchanged), OTGW calls debugTelnet.setNoDelay(false) in startTelnet(); port 25238 (OTGWstream) keeps NoDelay for the PIC request/response. SimpleTelnet branch perf/coalesce-telnet-tx commit a1067d4, PR https://github.com/rvdbreemen/SimpleTelnet/pull/8 (Robert merges); OTGW submodule pointer -> a1067d4. Bench (OTGW32, fresh reboot per run, temp broker, sat_boiler_emulator on 25238, telnet all toggles on + q + D, 60 s): old alpha.485: min_free_heap 1064 B, trace heap min 13452 B, q output 31127 B; Nagle on (first unconditional, then the configurable version): min_free_heap 23752 / 24348 B (unchanged by the session), trace heap min 64036 / 70252 B, q output 57911 / 58597 B (more of the burst delivered). Console key latency with Nagle: 36-197 ms (8 samples). Port 25238 bridge kept working (emulator connected and sent). Build SUCCESS. OPEN AC3: whether Sergeant D's unit wedged (asked 2026-10-23, no answer yet).
<!-- SECTION:NOTES:END -->
