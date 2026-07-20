---
id: TASK-1045
title: >-
  Selective debug toggles in the heap-leak capture: keep REST API and NTP,
  silence the rest
status: Done
assignee:
  - '@claude'
created_date: '2026-07-20 19:58'
updated_date: '2026-07-20 20:13'
labels: []
dependencies: []
priority: high
ordinal: 164000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The no-mdns capture (TASK-1040, 2026-07-20) produced the sharpest evidence so far: 36 consecutive byte-identical heap samples (20992 free / 14416 maxBlock) and then, immediately after the NTP resync at 20:11:31, a monotone decay that never stopped until the device died at 20:34:47. The same pattern is visible in the stock 1.7.1 capture: heap wobbles around 20200 until the full resync at 22:40:23, then declines monotonically from 22:41 onward.

But that capture could not settle the cause, because -QuietDebugToggles switches off REST API logging along with everything else. Browser and HTTP load, the load we already know this reporter generates from two open tabs, was invisible for the whole run. The quiet preset solved instrument perturbation and created a blind spot on the prime suspect.

NTP logging was also off, so the resync sequence is only visible through three unconditional lines. The delta, the sync candidate, timezone errors and the WAITFORSYNC transitions were all suppressed.

Needed: selective silencing instead of blanket silencing. Keep REST API and NTP on, since together they cost only a few lines per second and cover both candidate explanations. Silence OTmsg, MQTT, MQTTGate and Sensors, which are the high-volume ones that made earlier captures unreadable.

Restore must stay symmetric: a toggle is a flip, so re-sending the same key restores it whichever direction it was moved. The existing restore path already works that way; it only needs to also track toggles that were switched ON.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A -KeepDebugToggles option accepts a comma-separated label list that is left on, and switched on when the device has it off
- [x] #2 Labels not on the keep list are silenced as before
- [x] #3 Restore returns every toggle the run moved, in either direction
- [x] #4 capture-heap-leak.bat keeps REST API and NTP on by default
- [x] #5 Run summary records both the policy and the keep list
- [x] #6 Verified on hardware: telnet log shows REST requests and the full NTP sync sequence, and no per-message OT or MQTT gate output
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds -KeepDebugToggles to the capture worker and wires capture-heap-leak.bat to keep REST API and NTP on while silencing the high-volume toggles.

Motivation: the no-mdns capture (TASK-1040) proved blanket silencing was too blunt. It hid REST/browser load (the prime suspect) and reduced the NTP resync to three lines exactly when the heap began collapsing.

Verified on bench 192.168.88.68: policy line reads 'silence all except [REST API, NTP]', REST API and NTP kept on, OTmsg silenced, toggles restored on exit. Committed 033dcfa9.
<!-- SECTION:FINAL_SUMMARY:END -->
