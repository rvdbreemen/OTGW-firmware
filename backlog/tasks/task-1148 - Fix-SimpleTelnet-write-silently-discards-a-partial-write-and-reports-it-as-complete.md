---
id: TASK-1148
title: >-
  Fix: SimpleTelnet write() reports a partial write as complete (honest return +
  bounded retry)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-22 06:08'
updated_date: '2026-09-22 06:27'
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
- [ ] #2 No fixed per-line delay is introduced; any waiting is bounded and only incurred when the send buffer is actually full
- [ ] #3 The OT frame path shows no new serial overruns or dropped frames under the same burst test
- [ ] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [ ] #5 Bytes that still cannot be written after the retry budget are counted per client and the count is readable, so loss becomes observable instead of silent
- [ ] #6 Burst loss is measurably lower than the TASK-1147 baseline against a broker as lossless observer; residual loss is permitted and must show up in the counter
- [ ] #7 Re-entrancy is safe: a write() that yields cannot be re-entered for the same client and interleave output
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
SCOPE NARROWED by maintainer decision: do A (honest return) and B (bounded retry) here. The C option, a TX ring, is deferred to its own task so its RAM cost can be judged after A+B are measured.

RAM cost of this task: a per-client uint32 drop counter only. Two instances of SimpleTelnet<1> exist (debugTelnet at networkStuff.ino:24, OTGWstream at OTGW-Core.h:28), so 8 bytes total. Effectively free, against a measured static budget of 52,712 of 81,920 bytes DRAM and roughly 17.9 KB free heap while running.

WHAT A+B CAN AND CANNOT DO. Without a ring there is nowhere to park an unwritten tail, so under a sustained burst bytes will still be lost. A+B change two things that matter anyway: the function stops claiming it wrote bytes it discarded, and it tries harder before giving up. The remaining loss becomes counted rather than invisible, which is the property that cost two false diagnoses in TASK-1147.

IMPLEMENTATION

A. write(const uint8_t*, size_t) returns the number of bytes actually accepted, not size. Check the call sites first: the Stream contract lets callers act on the return value, and Print::write chains on it. DebugTf ignores it today, but the contract change is observable and must be deliberate.

B. Bounded retry of the remainder. On ESP8266 an immediate retry is pointless because only a yield lets lwIP drain, so the loop must be write / yield / write under a hard budget expressed in both iterations and elapsed millis. Budget stays small: the measured peak is 43 lines per second on an idle bench, and the OT path must not stall because Serial is reserved for the PIC.

RE-ENTRANCY, the main hazard this task introduces. yield() inside write() is exactly the documented re-entrancy route: doBackgroundTasks can be re-entered through feedWatchDog and yield, and anything it reaches may call DebugTf again, which would nest a second write() into the same client and interleave two lines into corrupted output. Guard with a per-instance in-write flag: a nested call takes the single-attempt path with no yield, accepts what it can, counts the rest and returns honestly. That degrades to current behaviour under nesting, which is acceptable; silent interleaving is not.

Do not introduce a fixed per-line delay. Measured cost at the 43 line per second peak: 1 ms costs 43 ms of blocked loop per burst second, 5 ms costs 215 ms, 20 ms costs 860 ms, and that is on an idle device. The library already rejected this in _drainClient with the comment that it deliberately avoids delay() in a cooperative scheduler. A fixed delay also pays on every line including the overwhelming majority where the send buffer was empty, whereas a retry pays only where congestion actually occurs.

VERIFICATION reuses the TASK-1147 rig: local mosquitto subscribed as lossless observer, bench pointed at it with a single-field settings POST of mqttbroker only so the stored MQTT password is never touched, then the three-run sensor-simulator test. Baseline to beat: telnet lost all six sensor publish lines in two of three runs and half the sensor read lines in one. Restore the broker setting afterwards.
<!-- SECTION:PLAN:END -->
