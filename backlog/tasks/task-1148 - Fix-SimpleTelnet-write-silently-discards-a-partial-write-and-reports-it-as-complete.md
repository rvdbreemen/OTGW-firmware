---
id: TASK-1148
title: >-
  Fix: SimpleTelnet write() silently discards a partial write and reports it as
  complete
status: To Do
assignee: []
created_date: '2026-09-22 06:08'
labels:
  - bug
dependencies: []
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
