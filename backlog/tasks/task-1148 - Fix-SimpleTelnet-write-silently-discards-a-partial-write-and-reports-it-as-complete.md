---
id: TASK-1148
title: >-
  Fix: SimpleTelnet write() silently discards a partial write and reports it as
  complete
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-22 06:08'
updated_date: '2026-09-22 06:17'
labels:
  - bug
dependencies: []
references:
  - 'src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:218-238'
  - 'Discord #nederlandse-ondersteuning / indigo_light / 2026-09-21'
priority: medium
ordinal: 227000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The debug telnet console loses output under load, which is how two false diagnoses were reached on 2026-09-22 (see TASK-1147). The cause is in the vendored library, at src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:218-238.

WiFiClient::write() has three possible outcomes and the code knows only two. Zero bytes is treated as a dead connection. Anything above zero is treated as success. The case of 1..size-1 falls into the second branch: the unwritten tail is not buffered, not retried and not reported, and the function returns 'size' regardless. The caller is told everything went out.

On ESP8266 Core 2.7.4 a short write is the normal result once the lwIP send buffer stays full until the socket timeout (ClientContext.h::_write_from_source returns _written after _is_timeout()). So the loss is load-dependent and probabilistic, and strictly subtractive.

This is the same defect class already fixed twice in this codebase on the MQTT side: TASK-769 (payload half of a publish) and TASK-1134 (header half). There a short write desynchronised the MQTT stream; here it silently truncates the console. Third occurrence of the same shape: treating a short write as a complete one.

Measured evidence (bench .88.68, idle, no boiler or thermostat): average 0.6 log lines per second but a peak of 43 lines in one second. Against a broker subscribed as a lossless observer, telnet lost all six sensor publish lines in two of three runs and half the sensor read lines in one.

A fixed delay was considered and rejected as the primary fix. At the measured 43-line peak, 1 ms per line costs 43 ms of blocked loop, 5 ms costs 215 ms and 20 ms costs 860 ms, and that is on an idle device. Serial is reserved for the PIC on this platform, so a blocked loop risks serial overrun and lost OT frames, which trades a cosmetic logging problem for real data loss. The library already rejected this approach explicitly in _drainClient ('Deliberately no delay() - we are in a cooperative scheduler'). A fixed delay also pays on every line including the overwhelming majority where the buffer was empty.

Options, to be decided before implementing:
A. Honest return value only. Return the actual byte count instead of 'size'. Three lines, stops the function lying, does not stop the loss.
B. Bounded retry. Keep writing the remainder with yield() between attempts under a hard budget. Costs nothing when the buffer is empty and only pays where congestion actually occurs.
C. Small TX ring drained from loop(), which is what the 2.0.0 async variant already does. Most correct, largest surface.

Note the Stream contract: callers may legitimately rely on the return value, so A changes observable behaviour for anything that checks it. Check call sites before choosing.

The user owns this library and has given standing permission to improve it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A short write no longer reports more bytes than were actually accepted by the TCP stack
- [ ] #2 Console output survives a burst at least as well as the measured baseline: against a broker subscribed as lossless observer, telnet no longer loses whole publish lines in a repeat of the TASK-1147 three-run test
- [ ] #3 No fixed per-line delay is introduced; any waiting is bounded and only incurred when the send buffer is actually full
- [ ] #4 The OT frame path shows no new serial overruns or dropped frames under the same burst test
- [ ] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Maintainer chose A+B+C combined. Backport the ring from the 2.0.0 async variant rather than inventing one.

1. Backport SimpleTelnetRing<N> into the 1.x header. Self-contained, ~60 lines, no heap, template on capacity, depends on nothing from SimpleTelnetCore. Its push() already returns the count actually stored and drops the overflow, which is the drop-newest policy we want for a console.

2. Add _tx[MAX_CLIENTS] at SIMPLETELNET_TX_BUF_LEN (512 default, overridable) plus a per-client dropped-byte counter.

3. write(buf,size) becomes: if the ring already holds data, push there FIRST and do not attempt a direct write, otherwise output reorders; else try the direct write and push only the remainder. Then one bounded inline flush (B). Return written + buffered (A).

4. loop() calls _flushTx(i) per active client (C). loop() is already called from OTGW-Core.ino:550-551, OTGW-firmware.ino:431-432 and the flash-wait loop at :472-473, so the drain point exists and runs during PIC flashing too.

5. _flushTx: peekN into a 256 byte stack chunk, client.write, discard what was accepted, stop on a zero-byte write or empty ring, bounded iterations so it cannot spin.

6. _disconnectClient flushes once before stop(), matching what upstream a909731 did for the async side.

SIZING EVIDENCE (bench .88.68): mean log line 107 bytes, p95 177, peak burst 43 lines in one second on an IDLE device. A burst is therefore about 4.6 KB, and a 512 byte ring holds only 4.8 lines, roughly 11 percent of it. The ring is a smoothing buffer, not a burst reservoir: correctness depends on drain frequency from loop(), not on capacity. Sizing it to swallow a whole burst would cost about 4.6 KB per client, which is not available (free heap on the bench is about 17.9 KB). Two instances at 512 bytes costs 1 KB static, which is the accepted price.

Consequence to state plainly: a sustained burst can still overflow. The difference is that it then increments a counter instead of vanishing silently, which is the actual defect being fixed.

RISKS TO HANDLE:
- Re-entrancy: doBackgroundTasks can re-enter through feedWatchDog and yield while _flushTx is mid-write, so an in-flush guard is needed.
- OTGWstream on port 25238 shares this write path, where ADR-095 byte transparency applies. The ring strictly improves that case because ordering is preserved and less is lost, but the overflow policy must be a counted drop, never a silent one.

OPEN FOR MAINTAINER: whether this needs an ADR. It adds a buffering layer to the serial bridge path and spends RAM, which the project rules would normally treat as architecturally significant.
<!-- SECTION:PLAN:END -->
