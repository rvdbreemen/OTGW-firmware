---
id: TASK-1169
title: >-
  Investigate: otgwstream_tx_dropped rises, PIC-to-client bytes lost on port
  25238
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-27 21:22'
updated_date: '2026-09-28 04:01'
labels:
  - bug
  - port-25238
dependencies: []
priority: medium
ordinal: 238000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1167 bench validation on .88.68 (build 1.7.6-beta.7+3bc71d6). otgwstream_tx_dropped counts PIC->client bytes the firmware could not hand to a port-25238 client. It went 0 -> 237 during the ADR-097 tests (T2 sent ~52 KB to each of two clients in 180 s), then 237 -> 241 in one step with a SINGLE pyotgw client (heap baseline, t=1246 s) and 241 -> 245 with only OTmonitor connected. So loss also happens with one client and is not caused by the new second writer. ADR-095 requires the bridge to be byte transparent in both directions; a dropped byte can corrupt a line a tool parses (pyotgw logged "Unknown message" warnings while alone). Maintainer agreed to ship beta.7 with this open.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The code path that increments otgwstream_tx_dropped is traced with file:line, and the condition that triggers it (client TCP window, write size, heap gate or other) is identified with evidence
- [x] #2 The loss is reproduced on the bench with a harness that counts bytes on the serial side against bytes each client received
- [x] #3 It is established whether 1.7.6-beta.6 and v1.7.4 drop too under the same load (old vs new)
- [ ] #4 A fix, or a recorded decision why the loss is acceptable, with the before/after byte counts
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-27 two-client run (OTmonitor + pyotgw): tx_dropped 245 -> 253 -> 265 at t=714-721 s, coinciding with the lowest maxblock sample of the run (6296 B, baseline min 11592). Suggests the drop path is tied to a failed or refused allocation when the largest free block is small.

2026-09-28 trace + old-vs-new (AC#1-3):
- Path: SimpleTelnet::_writeToClient() (src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:213-223) counts size-sent when WiFiClient::write() returns short. Callers: handleOTGW() passthrough OTGWstream.write(passthru, <=64) at OTGW-Core.ino:4789/4841. Each client socket has setTimeout(_keepAliveInterval) = 1000 ms (SimpleTelnet_impl.tpp:445), so core 2.7.4 ClientContext returns short after 1 s without send progress (client not reading, or no ACKs e.g. a WiFi stall). After repeated zero writes _onWriteError() drops the client.
- Harness scratchpad t1169.py: one client, SO_RCVBUF 1024, sends PR=A every 50 ms and does not read for 10 s; device/info polled every 0.5 s.
  - beta.7 candidate 3bc71d6: tx_dropped +127, device resets the connection at 9.1 s, HTTP latency peak 1.06 s.
  - beta.6 (5611658, OTA firmware only): tx_dropped +127, reset at 9.05 s, HTTP latency peak 2.93 s.
  - Identical: the loss is pre-existing, not caused by the ADR-097 second writer.
- v1.7.4/v1.7.5: not flashed. Code evidence only: SimpleTelnet cc4c88e has the same 1000 ms setTimeout, and write() returned the full size on a partial write (silent loss, no counter). The counter added in f73cc7f/2a40633 made existing loss visible.
- Serial-side PIC output cannot be captured on COM3 (CH340 RX sees ESP TX only), so AC#2 uses the device counter and the RST, not a serial byte count.
- Also observed: a slow client blocks the whole loop ~1 s per short write (PIC serial not drained meanwhile), and a client that pauses reading for ~9 s is disconnected.
- Open: AC#4 fix or decision, for the maintainer (options: accept + document; per-client non-blocking backlog in SimpleTelnet, needs library approval and RAM).
<!-- SECTION:NOTES:END -->
