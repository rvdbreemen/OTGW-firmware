---
id: TASK-1149
title: Add a TX ring to SimpleTelnet so a busy console stops losing output
status: To Do
assignee: []
created_date: '2026-09-22 06:27'
updated_date: '2026-09-22 11:26'
labels:
  - bug
  - wontfix
dependencies:
  - TASK-1148
references:
  - 'src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:218-238'
  - '2.0.0 reference: src/AsyncSimpleTelnet.h:53-115 (SimpleTelnetRing)'
priority: low
ordinal: 228000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Deferred half of TASK-1148. That task makes SimpleTelnet::write() honest and adds a bounded retry, which costs no RAM but cannot stop loss under a sustained burst, because without a buffer there is nowhere to park an unwritten tail. This task adds that buffer.

Do not start before TASK-1148 has been measured on hardware. The point of the split is to see what the free fix buys before spending RAM.

DESIGN: backport SimpleTelnetRing<N> from the 2.0.0 async variant (src/AsyncSimpleTelnet.h:65) rather than writing a new one. It is self-contained, about 60 lines, no heap, template on capacity, and depends on nothing from SimpleTelnetCore. Its push() already returns the count actually stored and drops the overflow, which is the drop-newest policy a console wants.

- Add _tx[MAX_CLIENTS] plus the existing per-client drop counter from TASK-1148.
- write(): if the ring already holds data, push there FIRST and do not attempt a direct write, otherwise a direct write overtakes the buffered tail and output reorders. Only when the ring is empty may write() go straight to the socket, pushing just the remainder.
- loop() drains via _flushTx(i) per active client. The drain point already exists and is called from OTGW-Core.ino:550-551, OTGW-firmware.ino:431-432 and the flash-wait loop at :472-473, so it keeps draining during a PIC flash.
- _flushTx should write directly out of the ring in at most two contiguous spans rather than copying into a stack buffer. A ring wraps at most once, so two write() calls cover it. The 2.0.0 version copies into a 256 byte stack chunk; on 1.x that is 6 percent of the ~4 KB cont stack and it can nest under re-entrancy, so avoid it.
- _disconnectClient should flush once before stop(), matching what upstream a909731 did for the async side.

MEASURED RAM COST, from the ELF of 1.7.6-beta.4+215d3ef:
  .data 3,108 + .rodata 10,460 + .bss 39,144 = 52,712 of 81,920 bytes DRAM (64.3 percent)
  free for heap at boot 29,208 bytes; actually free while running about 17,900 bytes

Per instance at MAX_CLIENTS=1: buf 512 + head/tail 4 + full 4 + counter 4 = 524 bytes. Two instances exist, debugTelnet at networkStuff.ino:24 and OTGWstream at OTGW-Core.h:28, so 2x512 costs about 1,048 bytes. That is 1.3 percent of total DRAM, 3.6 percent of boot headroom and 5.9 percent of the heap actually free while running, which is the honest figure because it is permanently gone from the heap the heap gates steer on.

SIZING OPTIONS:
  1024 x 2 = 2,072 B   holds 22 percent of a burst
   512 x 2 = 1,048 B   the 2.0.0 size
   512 bridge + 256 console = 788 B
   256 x 2 =   536 B
   bridge only, 512 = 524 B

Measured burst shape on bench .88.68, idle, no boiler or thermostat: mean log line 107 bytes, p95 177, max 201, peak 43 lines in one second, so about 4.6 KB per burst. A 512 byte ring holds 4.8 lines, roughly 11 percent. The ring is therefore a smoothing buffer, not a burst reservoir: correctness comes from drain frequency, not capacity. Sizing it to swallow a whole burst would need about 4.6 KB per client, which is not available.

RECOMMENDATION, for the maintainer to confirm: 512 for OTGWstream and 256 for debugTelnet, 788 bytes total, putting the RAM where loss is most expensive. Port 25238 carries the raw OT bridge under ADR-095 byte transparency, where a dropped byte corrupts a command or response stream; the console is diagnostics, and TASK-1148 already improves it for free. The maintainer asked for 2x512 in principle, so this is a recommendation and not a decision.

ADR: still open. This adds a buffering layer to the serial bridge path and spends RAM, which the project rules would normally treat as architecturally significant. Decide before implementing, and never self-accept.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Ring sizes are chosen deliberately by the maintainer and the resulting static RAM cost is measured from the ELF, not estimated
- [ ] #2 Output ordering is preserved: once the ring holds data no direct write may overtake it
- [ ] #3 Against a broker as lossless observer, a repeat of the TASK-1147 three-run test shows no whole publish lines lost from the telnet capture
- [ ] #4 Port 25238 keeps ADR-095 byte transparency: ordering intact and any overflow counted, never silent
- [ ] #5 No new serial overruns or dropped OT frames under the burst test
- [ ] #6 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
WONT DO, closed on maintainer decision 2026-09-22. The ring would not solve enough to be worth its RAM.

The case for it collapsed once TASK-1148 was measured on hardware. A ring only helps where the bounded retry cannot, and that turns out to be a corner nobody reaches:

  normally reading client        0 B lost
  slow reader, 512 B/s           0 B lost
  slow reader, 64 B/s            0 B lost after the retry fix (was 133 B)
  reader stalled, normal window  0 B lost over 60 s
  reader stalled + SO_RCVBUF 2048   ~250 B, the only case still losing

That last row is the whole remaining problem, and it needs a client that has stopped reading AND an artificially shrunk receive window. A real client would have to stall for minutes before its kernel buffer, roughly 64 KB against a console emitting a few hundred bytes per second, even filled.

Against that, the cost measured from the ELF: about 1,048 bytes static for 512 bytes per instance across the two SimpleTelnet<1> instances. That is 5.9 percent of the roughly 17.9 KB actually free while running, permanently gone from the heap the heap gates steer on. And a 512 byte ring holds 4.8 of the 43 lines in a measured peak burst, about 11 percent, so it would not even absorb the burst: it smooths, and correctness would still come from drain frequency.

Spending 6 percent of live heap to improve a case that only appears under a deliberately crippled client is the wrong trade on a device with about 40 KB of usable RAM.

What replaces it: TASK-1148 shipped the honest return value, the bounded retry and per-client drop counters, readable as telnet_tx_dropped and otgwstream_tx_dropped in /api/v2/device/info. If those counters ever climb in the field, that is the evidence that would justify reopening this. Until then there is nothing to fix.

The design work is not lost. This record keeps the backport plan, the ordering rule that a direct write must never overtake a buffered tail, the sizing table and the measured RAM figures, so reopening would start from the numbers rather than from scratch.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not implemented. Closed as WONT DO on 2026-09-22 because the TX ring would not solve enough to justify its RAM.

After TASK-1148 was measured on hardware, the only scenario still losing bytes is a client that has stopped reading AND has an artificially shrunk receive window (SO_RCVBUF 2048), which costs about 250 B. Every realistic profile, including a slow reader at 64 B/s and a stalled reader with a normal window, now loses nothing.

The ring would cost about 1,048 bytes of static RAM, 5.9 percent of the roughly 17.9 KB free while running, and would not even absorb a burst: 512 bytes holds 4.8 of 43 measured peak lines. Wrong trade on a device with about 40 KB usable.

TASK-1148 covers the real need: honest return value, bounded retry, and per-client drop counters exposed as telnet_tx_dropped and otgwstream_tx_dropped. Those counters are the trigger to reopen this: if they climb in the field, the evidence exists. Until then there is nothing to fix.

The design notes, ordering rule, sizing table and measured RAM figures are kept in the task so a reopen starts from the numbers.
<!-- SECTION:FINAL_SUMMARY:END -->
