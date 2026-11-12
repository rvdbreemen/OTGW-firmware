---
id: TASK-1856
title: >-
  OT-Direct loopback frames count as a real boiler for SAT: satOnBoilerDetected
  turns satsimulation off and persists it
status: To Do
assignee: []
created_date: '2026-11-12T02:36:00Z'
updated_date: '2026-11-12T02:48:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 analysis of 09's excerpt (b197/t1856-excerpt.txt, OLD/NEW-results.txt; reading, not proven): (1) the two settings.ini writes ~4 s apart are the VS= handler's own writeSettings(false) (OTDirect.ino VS= branch): OLD writes at 03:21:52 and :56 fall in the VS=15 (t+25.6 s) and VS=x (t+29.6 s) windows, NEW likewise; they are not a SAT teardown. They only wrote the RAM state, so settings.sat.bSimulation was already false in RAM, with no write of its own in the near-continuous command windows (t+13.6 s onward). (2) A RAM-only switch-off without a write fits the enable gate in updateSetting("SATsimulation") (settingStuff.ino:1184-1187: rejects and sets false when satBoilerHardwarePresent() is true); satOnBoilerDetected() (SATcontrol.ino:2790) and the backstop at SATcontrol.ino:7000 write the file themselves. (3) Lead hypothesis: the loopback path stores its synthetic answers in the boiler cache (otBoilerCacheStore, OTDirect.ino:1647), including otBoilerCacheValid[3], and the cache survives a mode switch (TASK-1807). After loopback -> master/gateway, otDirectBoilerPresent() (OTDirect.ino:323-326) returns that stale valid[3] = a boiler, so an enable is refused and the :7000 backstop tears simulation down; a reboot empties the cache, which matches 're-enable only after a reboot'. Asked for the 5-min capture: the OTD mode and SAT simulation state (REST) before and after each step, and every writeSettings line.
<!-- SECTION:NOTES:END -->
