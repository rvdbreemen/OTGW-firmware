---
id: TASK-891.6
title: 'SAT: remove obsolete OPV calibration (not in Python)'
status: To Do
assignee: []
created_date: '2026-06-20 19:22'
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
