---
id: TASK-1856
title: >-
  OT-Direct loopback frames count as a real boiler for SAT: satOnBoilerDetected
  turns satsimulation off and persists it
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T02:36:00Z'
updated_date: '2026-11-12T05:02:00Z'
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
- [x] #1 The trip is reproduced on the real code: a host test (or a bench log from .61 with the frame and the call path) shows which caller sets bBoilerDetectedFlag in loopback; the root cause is named with file:line
- [x] #2 Host test that feeds loopback frames (and the boot handshake) through the real code and shows settings.sat.bSimulation stays on; red on the old code, green on the fix
- [x] #3 Loopback state never counts as boiler presence on any path that reaches satOnBoilerDetected()
- [x] #4 Re-enabling simulation works without a reboot after a detection (the one-way latch is cleared when presence goes away, or the false trip cannot happen)
- [x] #5 Bench on .61 by the bench-tester: loopback with simulation on for at least 15 min and across a mode switch, simulation stays on
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 analysis of 09's excerpt (b197/t1856-excerpt.txt, OLD/NEW-results.txt; reading, not proven): (1) the two settings.ini writes ~4 s apart are the VS= handler's own writeSettings(false) (OTDirect.ino VS= branch): OLD writes at 03:21:52 and :56 fall in the VS=15 (t+25.6 s) and VS=x (t+29.6 s) windows, NEW likewise; they are not a SAT teardown. They only wrote the RAM state, so settings.sat.bSimulation was already false in RAM, with no write of its own in the near-continuous command windows (t+13.6 s onward). (2) A RAM-only switch-off without a write fits the enable gate in updateSetting("SATsimulation") (settingStuff.ino:1184-1187: rejects and sets false when satBoilerHardwarePresent() is true); satOnBoilerDetected() (SATcontrol.ino:2790) and the backstop at SATcontrol.ino:7000 write the file themselves. (3) Lead hypothesis: the loopback path stores its synthetic answers in the boiler cache (otBoilerCacheStore, OTDirect.ino:1647), including otBoilerCacheValid[3], and the cache survives a mode switch (TASK-1807). After loopback -> master/gateway, otDirectBoilerPresent() (OTDirect.ino:323-326) returns that stale valid[3] = a boiler, so an enable is refused and the :7000 backstop tears simulation down; a reboot empties the cache, which matches 're-enable only after a reboot'. Asked for the 5-min capture: the OTD mode and SAT simulation state (REST) before and after each step, and every writeSettings line.

AC1 reproduction (otgw-bench-tester, .61 OTGW32, 2.0.0-beta.197+4cafc97, 2026-11-12; excerpt C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\t1856\run2\t1856-run2-excerpt.txt, raw telnet.log, events.txt, reenable.txt alongside): satsimulation stays True for 60 s in loopback (otdmode 4); POST otdmode=3 at 03:50:50.116 (host) -> telnet 03:50:48.788 'applyOTDirec(2827): OTD: mode -> master', writeSettings at .806-.967, then 03:50:48.972 'satOnBoilerD(2810): SAT-SIM: boiler appeared on bus -> simulation disabled completely', 0.2 s after the switch and before any master-mode reply (the next frame is the gateway's own BF01D0000 answer). Re-enable: POST satsimulation=true 200 with echo, GET stays False at +0.5, +3 and +28 s (the enable gate in updateSetting refuses while satBoilerHardwarePresent() is true). Caller: the SATcontrol.ino:7000 backstop (settings.sat.bSimulation && satBoilerHardwarePresent()) -> otDirectBoilerPresent() -> otBoilerCacheValid[3], which the loopback path filled (otBoilerCacheStore in sendMasterRequestAsync's loopback branch) and which survives the mode switch. Side finding in the same log: in loopback SAT detects a manufacturer from the synthetic MsgID 3 ('satDetectMan(284): Detected manufacturer: Remeha (MemberID 11)' at 03:49:49.319); noted, not part of this fix unless it shares the cause.

Fix (wt-95fw, on top of 1840/1836): OTDirect.ino presence block (markers otdBoilerPresence): a marker otdMsgId3FromBoiler says who stored MsgID 3 last; otdNoteMsgId3Source(response, realBoiler) sets it, called after otBoilerCacheStore() in the loopback branch of sendMasterRequestAsync() (false) and in handleMasterResponse() (true). otDirectBoilerPresent() now returns !loopback && otBoilerCacheValid[3] && otdMsgId3FromBoiler. The cache itself and the mode-switch code are untouched (agreed with d3, who owns TASK-1839's setOTDirectMode change; clearing the cache on a mode switch stays TASK-1807). d3's point on 1835: otdBootHandshake() reads MsgID 3 with a blocking sendRequest() that does not store the cache, so presence stays false until the scheduler's first MsgID 3 read, the same as the cache valid[3] on the old code; a false presence never triggers the SATcontrol backstop (it fires on true).

Evidence AC2/AC3/AC4: tests/test_otd_loopback_not_boiler_for_sat.py (6 tests; real presence block or the old otDirectBoilerPresent(), real otBoilerCacheStore(), real satBoilerHardwarePresent() and the backstop line of SATcontrol.ino, the real SATsimulation enable gate of settingStuff.ino; source checks for both call sites). Fix 6/6 OK. Old code (origin/dev ba41264e6): 4 FAIL, reproducing the bench: 'master present=1 sim=0 teardown=1' after loopback -> master, and 'reenable sim=0' (09: POST satsimulation=true accepted, GET stays False). Harness kept compiling: test/host/test_local_frame_consumers.py/.cpp (the marker as an optional slice; its cache3 world sets the marker too, the harness models a real MsgID 3 answer there): RESULT PASS; test_override_reply 4 OK; alt_slots, gateway_own_status, mode_switch_queue_reset, ps1_summary, return_temp_slot, slave_reply_gate, tt_pic_parity, sat_no_return_startup all exit 0. evaluate --quick: 92 checks, 0 failed, 0 warnings.

Open: AC5 bench by the bench-tester on .61: simulation on, loopback 15 min, then otdmode 4 -> 3 -> 1 -> 4, simulation stays True throughout (REST poll) and no 'SAT-SIM: boiler appeared on bus' line; then POST satsimulation=false/true works without a reboot. Side finding still open (not this fix): in loopback SAT detects a manufacturer from the synthetic MsgID 3 ('Detected manufacturer: Remeha (MemberID 11)').

2026-11-12 AC5 bench (otgw-bench-tester), .61 OTGW32 on 2.0.0-beta.203+61b69e0, scratchpad t1856/cap15.py. satsimulation=true (start state (True,3)), otdmode=4 (loopback) from 05:44:51 to 06:00:21 (15 min 30 s), then otdmode 3 -> 1 -> 4 -> 3 at 30 s per step. A REST poll every 1 s logged only state changes: (True,4) at 05:44:51, (True,3) 06:00:21, (True,1) 06:00:52, (True,4) 06:01:23, (True,3) 06:01:54. satsimulation never left True; there were 4 poll timeouts during loopback, with no state change before or after them. Continuous telnet (27086 lines) holds all 5 'OTD: mode ->' lines (loopback / master / gateway / loopback / master) and 0 lines 'SAT-SIM: boiler appeared on bus'. Crashlog none afterwards. Old reference (197, t1856/run2): satsimulation flipped to False 0.5 s after leaving loopback, with that line. Earlier 203 run (b203, 125 s loopback, 3 -> 1 -> 4 at 15 s): satsimulation false/true worked without a reboot. Evidence: scratchpad t1856/run15/events.txt, telnet.log.
<!-- SECTION:NOTES:END -->
