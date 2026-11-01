---
id: TASK-1374
title: >-
  SAT: Fireplace mode switch - heat on the heating curve only, as with a missing
  room sensor (entry 49)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T06:32:00Z'
updated_date: '2026-11-01T08:41:00Z'
labels:
  - sat
  - webui
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 509000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 06:18 UTC (#alpha-testing 1563799007157817405): 'Create a switch called Fireplace mode. When this switch is enabled SAT will send only the heating curve value to the control loop and not the PID. The behavior of SAT will be identical to the missing indoor temperature sensor we discussed earlier. Some homes have a stove or an AC heating their primary room [...] This new development will let SAT continue heating the home regardless of the main room reached the setpoint or not.' Reuse TASK-1364's curve-only path (satPidCurveOnly, P/I/D 0 per entry 46): setting SATfireplace (default off), REST/MQTT/HA switch, v2 toggle; while on, the room temperature is treated as not usable for control. Not in thermo-nova (grep fireplace/stove: nothing).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With Fireplace mode on SAT heats on the curve only with P/I/D 0 (host test red without the switch, green); switch on REST/MQTT/HA/v2 (browser test); off restores PID control
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 worktree cleanup: unmerged work of this task sits in lane wt-1374 (archived as D:\Users\Robert\Documents\GitHub\RvdB\lane-archive-20261031\wt-1374.uncommitted.patch); the lane is kept and will be merged into dev (Robert: merge lanes where possible).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: host tests with fireplace cases (test_sat_pid_cadence 31/31, test_sat_room_loss 12/12) OK; browser test v2-settings-sat-followups.test.mjs 37/37 PASS incl. 'Fireplace mode renders as a labelled switch'; bench REST /api/v2/settings has satfireplace (b); bench HA discovery homeassistant/switch/otgw-1020BA21B4F8/sat_fireplace_mode/config 'SAT Fireplace Mode'. Build: esp32-combo 2.0.0-alpha.597+d458ac3 (build/…-flash.zip 08:51, built and posted by the coordinating session); the task commit is an ancestor of d458ac3 (git merge-base --is-ancestor). evaluate.py --quick 2026-11-01: 0 FAIL, Health 100%. Bench .61 (OT-Direct) runs 2.0.0-alpha.597+d458ac3 (/api/v2/device/info), up 25 min without reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
