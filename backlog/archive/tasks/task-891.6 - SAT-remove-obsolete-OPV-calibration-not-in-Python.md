---
id: TASK-891.6
title: 'SAT: remove obsolete OPV calibration (not in Python)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 19:22'
updated_date: '2026-06-21 08:48'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/SATtypes.h
  - src/OTGW-firmware/settingStuff.ino
parent_task_id: TASK-891
priority: medium
ordinal: 113000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George 2026-06-20: OPV calibration is obsolete development, not in the Python at all - remove completely. It is already inert in the control path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Remove the OPV calibration state machine and fields: SATCalibPhase/eCalibPhase, fCalibMaxTemp/fCalibStartTemp/iCalibStartMs/iCalibSamples, SAT_CALIB_* constants (SATcontrol.ino:366-470; SATtypes.h)
- [ ] #2 Remove OPV settings fOvpValue/bOvpEnabled from SATSection + their settingStuff.ino read/write + any MQTT/REST/JSON publish of OPV or calibration
- [ ] #3 Confirm no control path depended on OPV (it was inert); evaluate.py --quick clean (esp-abstraction baseline not regressed); python build.py --target esp32 exits 0; prerelease bumped
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the obsolete OPV (Overshoot Protection Value) calibration entirely (George: 'obsolete development, not in Python; remove completely'). Atomic removal across 9 files, ~100 refs: SATcontrol.ino (SAT_CALIB_* constants, satOvpCalibrate state machine, satOvpStart/StopCalibration, the control-loop call site, status-JSON je.field block, MQTT shadows + publishIfChanged); SATtypes.h (SATCalibPhase enum + 5 runtime fields + 2 settings fields); settingStuff.ino (SATovpvalue/SATovpenabled save+handlers); restAPI.ino (ovp_phase, ovp_start/stop/value/enabled dispatch, settings.sat.ovp_* fields, addNum/addBool); MQTTstuff.ino (_satOvp*Cmd + 4 dispatch-table entries); handleDebug.ino; data/sat.js + data/index.html (UI rows); data/index.js (settings field/label/help). HA discovery: removed the sat/ovp_enabled switch (case 9), renumbered cases 10-13->9-12, and fixed the caller (MQTTstuff loop <14->13, dhw_enable gate swIdx 13->12). grep-verify ZERO source refs. esp32 SUCCESS (fw+fs), evaluate.py --quick 0-fail.
<!-- SECTION:FINAL_SUMMARY:END -->
