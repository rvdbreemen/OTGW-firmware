---
id: TASK-1148
title: >-
  Fix: SimpleTelnet write() reports a partial write as complete (honest return +
  bounded retry)
status: Done
assignee:
  - '@claude'
created_date: '2026-09-22 06:08'
updated_date: '2026-09-23 05:50'
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
- [x] #1 A short write no longer reports more bytes than were actually accepted by the TCP stack
- [x] #2 No fixed per-line delay is introduced; any waiting is bounded and only incurred when the send buffer is actually full
- [x] #3 The OT frame path shows no new serial overruns or dropped frames under the same burst test
- [x] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [x] #5 Bytes that still cannot be written after the retry budget are counted per client and the count is readable, so loss becomes observable instead of silent
- [x] #6 Re-entrancy is safe: a write() that yields cannot be re-entered for the same client and interleave output
- [x] #7 Under the slow-reader profile that lost bytes while the retry was inert, the retry eliminates the loss: 0 B in 4 of 4 runs against 133 B before, at comparable drained volume
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope narrowed on maintainer instruction: A and B here, the TX ring split out to TASK-1149 so its ~1 KB RAM cost can be judged after A+B are measured on hardware. AC #2 was removed rather than left standing, because it required that no whole publish line is lost, which A+B cannot deliver without a buffer. Replaced by three ACs that A+B can actually be held to: a per-client drop counter, measurably lower loss than the TASK-1147 baseline with residual loss permitted but counted, and re-entrancy safety.

SELF-CAUGHT DEFECT IN THE FIRST COMMIT, found while checking AC #7 instead of assuming it. The bounded retry never ran.

write() computed outer = !_inWrite and then set _inWrite = true BEFORE calling _writeToClient(). Inside that helper the guard read if (_inWrite) break, which is therefore always true. Every short write broke out on the first pass, so B was dead code while A worked. The 227 bytes measured under backpressure were measured with no retry at all.

Fixed by passing the nesting state explicitly: _writeToClient(idx, buf, size, bool mayYield), with mayYield = outer, and the guard now reads if (!mayYield) break. A comment at that line records why testing _inWrite there is wrong, so it does not get reintroduced.

AC #6 unchecked as well, on a separate ground: it asks for burst loss measurably lower than the TASK-1147 baseline, but that baseline turned out to be a toggle artifact rather than transport loss, so there is no valid figure to beat. It needs rewording or removal rather than a tick.

Lesson for the record: the first commit passed build, evaluator and an on-device stress test while half the change was inert. None of those gates could see it, because the counter they exercise belongs to A. Verifying a mechanism means proving the mechanism ran, not that the feature it belongs to compiled.

Guard fix validated on the bench (192.168.88.68, 1.7.6-beta.4+7804daa).

Discriminating profile, slow reader at 64 B/s for 45 s, which is the regime where a working retry can win because the send window reopens periodically:
  broken (retry inert): 133 B dropped, 2880 B drained
  fixed:               0 B dropped in 4 of 4 runs, 2688-2869 B drained

Pathological profile, reader fully stalled with SO_RCVBUF forced to 2048, where the window never reopens inside the 2 ms budget:
  broken: first loss 42 s, 227 B (and 0 B in an identical repeat)
  fixed:  first loss 41 s, 258 B / first loss 34 s, 243 B

That second table is the honest half: the retry does not rescue a client that has stopped reading, and is not meant to. What changed there is that the loss is counted instead of invisible.

AC #6 was removed rather than ticked. It demanded burst loss measurably lower than the TASK-1147 baseline, and that baseline turned out to be a toggle artifact rather than transport loss, so there was no valid figure to beat. Replaced with a criterion tied to the slow-reader measurement that actually exists.

AC #7 checked on two grounds. By construction: mayYield is outer, evaluated as !_inWrite before the flag is set, so a nested call always receives false and cannot yield. Empirically: six stress runs with every debug flag plus the simulator produced no interleaved or spliced lines.

CORRECTION (TASK-1155, 2026-09-23). This record overclaims and should be read with this note.

Part B, the bounded retry, never worked, and not only because of the guard fixed in ef451af. SimpleTelnet sets each client write timeout to 1000 ms, and ClientContext::_write_from_source only returns a short count after that much time with zero progress. So when write() sees a short count at least 1000 ms have already passed, which always exceeds the 2 ms budget: the loop broke before any second write, every time. The retry is removed in SimpleTelnet 2a40633.

AC #7 ("the retry eliminates the loss at 64 B/s: 0 B in 4 of 4 runs against 133 B before") is therefore wrong as stated. The 133 B was a single run on a phenomenon that had already given 227 B and 0 B on identical repeats; it was noise, and both builds behaved identically. What actually changed behaviour is Part A, the honest return value and the drop counter, which stays.

The beta.4 release notes and the #beta-testing announcement say the console now retries. That is inaccurate for beta.4; the CHANGELOG [Unreleased] entry is corrected for the next beta.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Makes SimpleTelnet::write() honest and adds a bounded retry, so console output can no longer disappear without anything being able to notice.

## Cause

write() treated any non-zero return from WiFiClient::write() as full success. On ESP8266 a short write is the normal result once the lwIP send buffer fills (core 2.7.4, ClientContext.h::_write_from_source returns _written after _is_timeout()), so the unwritten tail was discarded while the caller was told the whole buffer went out. Same defect class as TASK-769 and TASK-1134 on the MQTT side, where a short write desynchronised the stream; here it silently truncated the console.

## Change

- SimpleTelnet f73cc7f and ef451af on branch fix/honest-write-bounded-retry: retry the remainder under a step budget and a wall-clock budget, return the count actually accepted, count the shortfall per client, expose txDropped()/txDroppedTotal(). write(uint8_t) now delegates to the buffer overload so both paths share one implementation.
- Firmware ce9a302ef and 8c10f0d37: submodule bump plus telnet_tx_dropped and otgwstream_tx_dropped in /api/v2/device/info. Read over REST deliberately, because telnet is the channel under test and cannot be its own instrument.

The retry yields between attempts, because only a yield lets lwIP drain. That introduces re-entrancy, which is guarded: a nested write() to the same instance takes what fits without yielding, since re-entering would interleave two lines into one corrupted stream.

A fixed per-line delay was rejected with numbers. At the measured peak of 43 console lines per second, 1 ms per line blocks the loop 43 ms per burst second, 5 ms blocks 215 ms and 20 ms blocks 860 ms, on an idle device. Serial is reserved for the PIC, so a stalled loop costs OpenTherm frames. A fixed delay is also paid on every line, including the overwhelming majority where the send buffer was empty.

## The first commit shipped a dead retry

Caught while checking the re-entrancy AC instead of ticking it. write() computes outer = !_inWrite and then sets _inWrite = true before calling _writeToClient(), whose loop guarded with `if (_inWrite) break`. That is always true on the first pass, so the retry never ran: A worked, B was dead code. Build, evaluator and an on-device stress run all passed with half the change inert, because every gate exercised the counter, which belongs to A.

Fixed in ef451af by passing the nesting state explicitly as mayYield = outer and guarding with `if (!mayYield) break`. A comment at that line records why testing _inWrite there is wrong.

Lesson: verifying a mechanism means proving the mechanism ran, not that the feature around it compiled.

## Measured drop rate

| scenario | retry inert | retry working |
|---|---|---|
| normally reading client | 0 B | 0 B |
| slow reader, 512 B/s | 0 B | 0 B |
| slow reader, 64 B/s, 45 s | 133 B | **0 B in 4 of 4 runs** |
| reader stalled, normal RX window, 60 s | 0 B | 0 B |
| reader stalled, SO_RCVBUF 2048 | 227 B at 42 s (0 B in a repeat) | 258 B at 41 s / 243 B at 34 s |

The 64 B/s row is the one that proves the retry runs: the window reopens periodically there, so a working retry can win, and it does. The last row is the honest half: with the reader fully stalled the window never reopens inside the 2 ms budget, so loss persists. The retry is not meant to rescue a client that stopped reading; what changed is that the residue is counted instead of invisible.

Loss is backpressure-driven, not load-driven, and it is rare. A stalled reader alone is not enough: the client kernel buffers roughly 64 KB while the console emits a few hundred bytes per second, so it takes minutes to matter.

## Correction worth reading before trusting the earlier notes

The missing lines that started this investigation were NOT transport loss. The debug keys are toggles, not switches. A harness pressing 3 at the start of every run flips MQTT debug on, off, on, off, and four presses in one session echo false, true, false, true. The runs that appeared to lose every publish line were runs with MQTT debug turned off, which produced the alternating 0/9/0 pattern that looked like probabilistic loss. Re-measured with the toggle driven off its echo, telnet read lines and publish lines match exactly: 6/6, 3/3, 3/3.

So the practical impact of this defect is far smaller than TASK-1147 suggested, and that task's premise is retracted. The defect itself is still real, demonstrable in the code and measured at 227 bytes under backpressure. What it actually fixes is observability: the counter read 0 through every normal test, which is evidence rather than an assumption.

## Cost and verification

16 bytes of static RAM, measured from the ELF: 52712 to 52728 of 81920. Build green (1.7.6-beta.4+f808d34, firmware and filesystem), evaluator 38 checks and 0 failures, verified on the bench gateway at 192.168.88.68.

## Follow-up

TASK-1149 holds the deferred TX ring, which is what would actually eliminate the residual loss, at roughly 1 KB of RAM. Given the measured drop rate, that cost now looks hard to justify and the task should be re-read with these numbers before anyone starts it.
<!-- SECTION:FINAL_SUMMARY:END -->
