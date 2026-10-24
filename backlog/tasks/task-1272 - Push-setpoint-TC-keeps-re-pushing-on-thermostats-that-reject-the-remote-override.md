---
id: TASK-1272
title: >-
  Push setpoint (TC=) keeps re-pushing on thermostats that reject the remote
  override
status: Done
assignee:
  - '@claude'
created_date: 2026-10-20 18:17
updated_date: '2026-10-24T08:34:00Z'
labels:
  - sat
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 402000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SAT pushes the user target correctly (TC=20.0, TC=20.5 x2 at 11:38:50/11:41:50/11:46:50); the PIC answers MsgID 9 = 20.5 (A40091480), but the Honeywell Round keeps sending TrSet 19.0 and the PIC cancels the override after 3 mismatches (otgw-6.6 gateway.asm ~3026 'Make 3 attempts', roomsetptcancel): AC0090000. SAT re-pushes every 5 min (SAT_CMD_REFRESH_MS), so the override flaps ~3 min on / ~2 min off. Not a value bug.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Docs: TC= push only works on thermostats that accept a remote room-setpoint override
- [x] #2 Option D (Robert 2026-10-24, Sergeant D entries 14 and 16): SAT pushes TC= only when the target changes (thermo-nova climate.py:641-657), never on a timer and never re-pushes on MsgID 9; an adopted thermostat setpoint is not pushed back (climate.py:765); host test red on HEAD, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22: TASK-1292 ground-truth spec is on dev: docs/sat/thermo-nova-spec.md (cited, Alexwijn/SAT releases/thermo-nova @ 7beaaf5), docs/sat/firmware-sat-spec.md and docs/sat/sat-comparison.md (443 rows, 434 verified). This task waits for Sergeant D's validation of the thermo-nova spec; the plan must quote the thermo-nova code for its area with file:line citations (CLAUDE.md 'SAT ground truth' block) before it is resumed.

2026-10-23 cycle: skipped, changes how SAT pushes the room setpoint (TC=) -> needs Sergeant D's OK per the SAT rule.

2026-10-23 field data: gruijter (Honeywell Round, alpha.461) set 20.0 in the SAT web GUI at ~10:10; the Round kept showing 19.5 (capture 20261023-094059.zip in #alpha-testing msg 1563110042952007690). Matches this task (thermostat does not adopt the TC= remote override).

2026-10-24 research (read-only agent; thermo-nova citations re-opened by the main session as second reader): thermo-nova DOES push a setpoint to the thermostat, once: async_set_target_temperature(temperature, cascade=True) (climate.py:641) calls coordinator.async_set_control_thermostat_setpoint() when push_setpoint_to_thermostat is set (climate.py:656-657, option const.py:60 default False :138); MQTT/serial send 'TC={value}' (coordinator/mqtt/opentherm.py:206-209). It runs on user/preset target changes (climate.py:568, 616, 624) and NOT when the thermostat itself changed the target (climate.py:765 cascade=False). No periodic re-push and no adoption check found (grep only). So the firmware's 5-min TC= refresh (SATcontrol.ino satEnqueueIfChangedTC, SAT_CMD_REFRESH_MS) is firmware-only and is what makes the override flap ~3 min on / ~2 min off on a Honeywell Round (PIC cancels after its attempts, gateway.asm ~3004-3029; capture cap-1138: MsgID 9 20.5 then 0 while TrSet stays 19.0). Entry 2 calls push 'firmware-only'; the push itself exists in thermo-nova, only the refresh does not. OT-Direct never cancels TC by itself (OTDirect.ino ~2774-2784). Options for the maintainer: (A) detect 'not adopted' (MsgID 9 back to 0 while TrSet != push, N=2) and stop re-pushing until the target changes or reboot, status field + narration + v2 note; (B) stop until toggled; (C) slow re-push; (D, likely simplest and thermo-nova aligned) drop the 5-min refresh: push only when the target changes, as thermo-nova; hybrid: re-push only when MsgID 9 reads 0 while a push is active (covers a PIC reset). A push-behaviour change also goes to Sergeant D (thermo-nova trace above). Waiting for a decision.

2026-10-24 maintainer decision (Robert, in session): HYBRID. Push TC= when the user target changes (as thermo-nova climate.py:641-657), and re-push only when MsgID 9 (TrOverride) reads 0 while a push is active (covers a PIC reset); drop the unconditional 5-min refresh. Next: plan with thermo-nova citations to Sergeant D for his OK (push-behaviour change), then implement.

2026-10-24 plan posted to Sergeant D for his OK (#alpha-testing): hybrid push with thermo-nova citations climate.py:641-657, :765, coordinator/mqtt/opentherm.py:206-209. Waiting for his answer; record it as entry 14 in docs/sat/sergeant-d-corrections.md before implementing.

2026-10-24 Sergeant D OK (03:18 UTC, 'yes it is ok') recorded as entry 14 in docs/sat/sergeant-d-corrections.md. Implementing the hybrid.

2026-10-24 review (opus) of the transition-based hybrid: BLOCKED. F1 HIGH: on PIC the MsgID 9 override comes back as a rewritten A frame after the boiler's B (gateway.asm:2932-2953); OTGW-Core.ino:5313-5334 marks it bAnswerOverride, print_f88 does not write TrOverride for it (validForMaster, :1888, :2759-2781) while setMsgLastUpdated does (:5466) -> 'seen' rarely trips (boiler-dependent). F2 HIGH: a refusing thermostat -> PIC cancels after 3 mismatches and clears the override (gateway.asm:3022-3036) -> MsgID 9 reads 0 -> re-push every cycle: harness 11 TC= in 10 min vs 3 on HEAD (scratchpad round.py). F3 MED-HIGH: adopt does not move fLastSentTC, so dial 20->21.5->20 is treated as an echo and SAT stays at 21.5 (conflicts with entry 2; scratchpad dialback.py). F4: code treats any far value as 'gone' (entry 14 said reads 0); F4b: pushes on boot/enable/online edge/toggle are firmware-only, not in entry 14. OT-Direct: re-push never fires (harmless), but after an adopt the stale TC override keeps replacing MsgID 16. Conclusion: with the PIC's signals the firmware cannot tell a gateway reset from a refusing thermostat reliably; a re-push on 'override gone' brings back the flapping. Decision needed from Robert (and possibly Sergeant D).

2026-10-24 Robert decided option D (exactly thermo-nova) after the BLOCKED review; Sergeant D informed 07:27 UTC (msg 1563453986630008849) and asked for an explicit OK in the consolidated question list (question 2). This drops entry 14's MsgID 9 re-push bullet; when he answers, record entry 15 naming entry 14 before committing. Re-review (opus) of option D: no correctness bug; R1 process (this note + entry 15 + AC1 rewrite), R2 a cache reset after an adopt sends the adopted value once (harmless, firmware-only), R3 docs drift (sat-comparison rows, manuals wording, spec citations), R4 fTCAccounted == fLastSentTC and iLastSentTCMs now write-only, R5 test docstring + else-branch coverage. Implementation is uncommitted in the dev tree, waiting.

2026-10-24 Sergeant D answered 'Yes' to option D (https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840), entry 16 names entry 14. Implementation in git stash 'TASK-1272 option D' can be committed.

2026-10-24 evidence: tests/test_sat_push_setpoint.py (real helpers + satFollowThermostatDecide cut from SATcontrol.ino): HEAD 'FAILED (failures=7)' - a/g: TC= at 100, 400, 700 s with an unchanged target; d/h: adopted value pushed back (110:TC=21.5); x: dial-back case; e: no resend after push off/on; fix 9/9 OK (one TC= per target change, no push back, dial-back adopted). test_sat_follow_thermostat 35 OK, test_sat_cmd_cache 4 OK, test_sat_pwm_override 63 OK, hosttest_ch 0 failures, evaluate 0 fail; build.bat combo SUCCESS; bench OTA app: SAT active, no crashlog (no thermostat on the bench, so TC= behaviour is host-tested only). Review rounds: hybrid blocked (F1-F3), option D re-review no correctness bug; R1 (records) via entries 14/16 + this AC rewrite, R3 docs fixed (sat-comparison rows, manuals EN/NL, spec), R4 iLastSentTCMs removed and fTCAccounted merged into fLastSentTC, R5 test docstring + else-branch wiring + scenario x. AC1 docs: manuals and spec say TC= push works only on thermostats that accept a remote override and SAT does not re-push.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Push setpoint now matches thermo-nova (option D, Sergeant D entries 14 and 16): TC= only on a target change, no 5-minute re-push, no MsgID 9 re-push, and an adopted thermostat setpoint is never pushed back; a dial-back after an adopt is followed. Evidence: host test of the real helpers red on HEAD (7 fails: periodic re-push, push-back) and green (9/9); regressions green; build + bench boot OK. Not verified on a real thermostat (gruijter's Honeywell Round is the field check).
<!-- SECTION:FINAL_SUMMARY:END -->
