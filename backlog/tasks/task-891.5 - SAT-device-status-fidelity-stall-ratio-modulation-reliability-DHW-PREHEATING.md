---
id: TASK-891.5
title: >-
  SAT: device-status fidelity (stall ratio, modulation reliability, DHW,
  PREHEATING)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 19:21'
updated_date: '2026-06-20 20:33'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/SATtypes.h
parent_task_id: TASK-891
priority: medium
ordinal: 112000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Boiler/device status machine to thermo-nova Python (device/status.py, modulation.py, device/const.py). Audit 2026-06-20; George: 'follow Python'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Stalled-ignition threshold uses last_cycle.duration x 3.0 with floor max(600s,...) (Python BOILER_STALL_IGNITION_OFF_RATIO=3.0 / MIN_OFF=600); firmware currently 1.5x with floor120/cap900 (SATcontrol.ino:576,515-516)
- [x] #2 Modulation reliability requires >=8 samples with delta>threshold (Python BOILER_MODULATION_RELIABILITY_MIN_SAMPLES=8, DELTA=3.0); firmware currently 3 changes (SATcontrol.ino:3227)
- [x] #3 MODULATING_UP/DOWN consult bModulationReliable and fall back to flow-temperature gradient (Python BOILER_GRADIENT_THRESHOLD_UP=0.2/DOWN=-0.1) when modulation unreliable (SATcontrol.ino:619-628)
- [x] #4 Add a distinct HEATING_HOT_WATER status (flame on + DHW active); firmware currently shows generic HEATING (SATtypes.h:90-96; Python device/status.py)
- [x] #5 PREHEATING requires flow >6C below setpoint (Python BOILER_PREHEAT_DELTA=6.0), not any sub-setpoint (SATcontrol.ino:618-623)
- [x] #6 python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Device-status machine aligned to Python: HEATING_HOT_WATER DHW status added (enum+name+UI label, with the satGetBoilerStatusName bounds fix so it does not render as off); stalled-ignition threshold max(600s, last_cycle*3.0) no cap (was 1.5x +120/900); PREHEATING requires >6C below setpoint; MODULATING_UP/DOWN use a modulation_direction with flow-gradient fallback when modulation unreliable; modulation reliability rewritten to Python's 8-sample / >=max(2,40%)-of-8-above-3% rule. alpha.230, esp32 build SUCCESS (fw+fs), evaluate 0-fail, pushed origin/dev d4b8f0ee.
<!-- SECTION:FINAL_SUMMARY:END -->
