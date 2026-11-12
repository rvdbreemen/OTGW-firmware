---
id: TASK-1856
title: >-
  OT-Direct loopback frames count as a real boiler for SAT: satOnBoilerDetected
  turns satsimulation off and persists it
status: To Do
assignee: []
created_date: '2026-11-12T02:36:00Z'
labels:
  - otdirect
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 933000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the bench-tester (09) on .61 (OT-Direct, loopback), on beta.190 and beta.197, relayed by the coordinator 2026-11-12: with SAT simulation on, satOnBoilerDetected() (SATcontrol.ino:2790) runs, switches settings.sat.bSimulation off and persists it with writeSettings(false); re-enabling simulation only works after a reboot. A SAT-adjacent detection fix, not a SAT behaviour change.

What the code at origin/dev ba41264e6 shows (read, not run), so the cause is still open:
- Presence: satBoilerHardwarePresent() (SATcontrol.ino:2760-2772) uses otDirectBoilerPresent(), which already returns false in loopback (OTDirect.ino:323-326).
- Edge hook: satNotifyBoilerFrameSeen() sets state.sat.bBoilerDetectedFlag (SATcontrol.ino:2819-2822); satConsumeBoilerDetected() then runs satOnBoilerDetected() with no presence check of its own (:2825-2830). Callers: OTDirect.ino:1716 in handleMasterResponse(), which the loopback path does not reach (its comment), and OTGW-Core.ino:5791-5794 in processOT(), guarded by !localAnswer && !replayed; bridgeFrameToParser() marks a 'B' in loopback as localAnswer (OTDirect.ino, prefix == 'B' && IS_LOOPBACK_MODE()). Third caller: satSetDebugForceBoilerPresent() (:2782).
- So by code reading every path excludes loopback, and the path that trips on .61 is not found in the code. Candidates to check: a B frame bridged while the mode was not loopback yet (the boot probe and handshake in initOTDirect, TASK-1835/N5) or right after a mode switch, the OT frame queue draining with a stale mode, the SATcontrol.ino:7000 boot check, or the debug override.
Related: TASK-1185 (simulation must not switch itself off on synthetic traffic), TASK-1138 (loopback presence flag), TASK-1835 (boot frames).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The trip is reproduced on the real code: a host test (or a bench log from .61 with the frame and the call path) shows which caller sets bBoilerDetectedFlag in loopback; the root cause is named with file:line
- [ ] #2 Host test that feeds loopback frames (and the boot handshake) through the real code and shows settings.sat.bSimulation stays on; red on the old code, green on the fix
- [ ] #3 Loopback state never counts as boiler presence on any path that reaches satOnBoilerDetected()
- [ ] #4 Re-enabling simulation works without a reboot after a detection (the one-way latch is cleared when presence goes away, or the false trip cannot happen)
- [ ] #5 Bench on .61 by the bench-tester: loopback with simulation on for at least 15 min and across a mode switch, simulation stays on
<!-- AC:END -->
