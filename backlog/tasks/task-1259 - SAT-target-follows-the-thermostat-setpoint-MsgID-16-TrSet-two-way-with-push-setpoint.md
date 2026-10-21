---
id: TASK-1259
title: >-
  SAT target follows the thermostat setpoint (MsgID 16 TrSet), two-way with
  push-setpoint
status: Done
assignee:
  - '@claude'
created_date: 2026-10-19 17:33
updated_date: '2026-10-21T19:57:00Z'
labels:
  - feature
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 391000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request by gruijter 2026-10-19, endorsed by Sergeant D. Today SAT only pushes its target to the thermostat (satpushsetpoint, TC= command, SATcontrol.ino ~407-415); a setpoint change on the wall thermostat is ignored, so the thermostat is useless as UI. Request: opt-in 'SAT target follows thermostat setpoint' that takes MsgID 16 TrSet as SAT target when it changes. Must avoid a feedback loop with satpushsetpoint (TC= echo). Sergeant D's rule: thermostat syncs only the room setpoint; with SAT active SAT drives the boiler; on SAT inactive/malfunction fall back to the thermostat. Open: thermostat may send CH enable off when it thinks the room is warm, decide who wins.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Design agreed (option name, loop guard, CH-enable arbitration)
- [x] #2 Thermostat setpoint change is adopted as SAT target within one TrSet cycle, and a SAT target change pushed via TC= does not bounce back (bench or simulator evidence)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. New setting SATfollowthermostat / settings.sat.bFollowThermostat. Default: units without a stored value get bPushSetpoint (Sergeant D: on when push-setpoint is on); a stored value always wins. Wired like overshoot_protection: settings.ini, REST GET/POST + knownSettings, SAT REST route follow_thermostat, MQTT sat/follow_thermostat command + sat/follow_thermostat_enable retained state, HA switch, v2 UI, docs.
2. Source: MsgID 16 TrSet Write-Data from the thermostat (OTcurrentSystemState.TrSet, PIC and OT-Direct), valid 5-30 C.
3. Edge-triggered: remember the last TrSet seen; a change >= 0.1 C is adopted as SAT target through the same code as a target change from REST/MQTT, and the active preset becomes NONE without restoring the pre-preset temperature. The first TrSet after boot or after enabling the option is only recorded. No minimum time between changes.
4. Loop guard: a TrSet within +-0.05 C of the last TC= value SAT pushed is SAT's own echo and is not adopted.
5. CH enable: unchanged; SAT keeps control via its own CS/CH while active and healthy, releases (CS=0) when off or faulted. Verify on the bench.
6. Out of scope: thermostats that reject TC= (TASK-1272).
Verification: host test compiling the real edge/echo function (red on HEAD: function absent / no follow); bench replay with thermostat T frames MsgID 16 20.0 -> 21.5 (target follows, preset cleared) and, with push-setpoint on, SAT target 21.0 -> TC=21.0 -> replayed TrSet 21.0 not adopted again; settings default check on a unit with push on and no stored key.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 Sergeant D (#alpha-testing): adopt the thermostat setpoint only on a room setpoint CHANGE (edge-triggered), not by periodic sync. CH-enable arbitration question still open.

2026-10-20 maintainer decision: CH enable from the thermostat is ignored while SAT is active and healthy (SAT wins); when SAT is off or faulted the thermostat applies again.

2026-10-21 maintainer: pick it up now (gruijter calls it essential for the Wife Acceptance Factor, #alpha-testing 2026-10-21). Plan goes to Sergeant D first (SAT control behaviour rule).

2026-10-21 plan posted to Sergeant D in #alpha-testing (message 1562523233524121862) with three open points: default on with push-setpoint?, clear an active preset on a thermostat change?, minimum time between adopted changes? Waiting for his OK before implementing; cycle continues with other tasks.

2026-10-21 Sergeant D (#alpha-testing 18:29 UTC, msg 1562533353125707929) answered the open points: (1) yes, default the option ON when push-setpoint is on; (2) yes, a thermostat change clears an active preset ('the override happens for a reason'); (3) no minimum time between adopted changes; the SAT target step default should be 0.5 like most thermostats: settings.sat.fTargetTempStep already defaults to 0.5 (SATtypes.h:455), no change. Design agreed -> AC1. Implementation details from these answers: the option defaults to the push-setpoint value for units that have no stored SATfollowthermostat yet (stored values win); on adoption the active preset becomes NONE WITHOUT the preset-none restore of fPreCustomTemp (that path would undo the thermostat value).

2026-10-21 RED bench (alpha.445+9b7628e, OTGW32 .88.61, /simulate replay %TEMP%\bench-1259\otgw_simulation.log: thermostat T frames MsgID 16 Write-Data 20.0 for 3 min then 21.5, plus status/flow): otmonitor roomsetpoint 20 -> 21.5 at t=182 s, SAT target_temp stayed 21 and active_preset stayed comfort (3) for the whole 8 min run. Evidence red-446.json.

2026-10-21 GREEN round 0 (alpha.446+47f7cb0 build of the first implementation): replay thermostat 20.0 -> 21.5: SAT target 21 -> 21.5 and active_preset comfort (3) -> none (0) at t=182 s (red: unchanged). Default rule live: settings.ini had no SATfollowthermostat key, SATpushsetpoint true -> after the firmware upgrade follow_thermostat true, written to settings.ini. Source deviation (agent, accepted): edge detection hooks the thermostat's own MsgID 16 Write-Data T frame in processOT, not OTcurrentSystemState.TrSet (with a TC= override active TrSet carries SAT's own pushed value from the R frame, so a thermostat change would never show). Opus review round 1: echo guard too weak on PIC (boot/PIC reset reverts the thermostat to its schedule and that was adopted; late replies bounce; rounding thermostats break the strict guard; iSense clear-before-set flicker suspected) -> fix: synced-state rule (prev within 0.25 C of last TC= while push on) + two-frame debounce. Rejected: filtering replayed frames (the replay is the bench harness); noted: window-close restore can undo an adoption.

2026-10-21 GREEN round 1 (alpha.446+47f7cb0 build with the synced rule + two-frame debounce, OTGW32 .88.61, push-setpoint on, follow on, preset comfort): synced replay (thermostat shows SAT's 21.0 for 3 min, then 21.5): roomsetpoint 21.5 at t=182 s, SAT target 21 -> 21.5 and preset comfort -> none at t=193 s (second frame = debounce). Flicker replay (synced 21.0, one frame 19.0, back to 21.0): roomsetpoint 19 at t=181 s, target stayed 21. (The preset clear at t=10 s in the flicker run is the thermostat going 21.5 -> 21.0 from the end of the previous replay while synced: a genuine change, adopted correctly.) Host test tests/test_sat_follow_thermostat.py 32 OK; 10 fail against the round-0 function (boot, unsynced prev, rounding, flicker, debounce cases). Bench restored: push off, follow off, target 20, sim log restored; settings diff vs baseline only the new satfollowthermostat key.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT can follow the wall thermostat's room setpoint (new setting 'Follow thermostat setpoint', MQTT sat/follow_thermostat + retained _enable state, HA switch, UI). Agreed with Sergeant D: defaults on for units with push-setpoint on (stored value wins), a thermostat change clears the active preset without restoring the old temperature, no minimum time. Source is the thermostat's own MsgID 16 Write-Data T frame (TrSet carries SAT's own value while a TC= override is active). Review round 1 added a synced rule (while push is on, adopt only when the thermostat was showing SAT's last TC= within 0.25 C) and a two-frame debounce, so a reboot/PIC reset, a late reply, a rounding thermostat or a one-frame flicker does not move the target. AC1: design agreed (notes). AC2: bench replays old vs new: old firmware ignores a thermostat change 20 -> 21.5; new adopts a synced 21.0 -> 21.5 within one extra frame and clears the preset, ignores a one-frame 19.0 flicker; default rule live (no key + push on -> follow true). Host test 32 OK (10 red on the round-0 logic). Not verified: a real thermostat (Round/iSense behaviour), the CH-enable arbitration on a real boiler, the window-close restore undoing an adoption (documented limitation).
<!-- SECTION:FINAL_SUMMARY:END -->
