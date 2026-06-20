---
id: TASK-891.5
title: >-
  SAT: device-status fidelity (stall ratio, modulation reliability, DHW,
  PREHEATING)
status: To Do
assignee: []
created_date: '2026-06-20 19:21'
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
- [ ] #1 Stalled-ignition threshold uses last_cycle.duration x 3.0 with floor max(600s,...) (Python BOILER_STALL_IGNITION_OFF_RATIO=3.0 / MIN_OFF=600); firmware currently 1.5x with floor120/cap900 (SATcontrol.ino:576,515-516)
- [ ] #2 Modulation reliability requires >=8 samples with delta>threshold (Python BOILER_MODULATION_RELIABILITY_MIN_SAMPLES=8, DELTA=3.0); firmware currently 3 changes (SATcontrol.ino:3227)
- [ ] #3 MODULATING_UP/DOWN consult bModulationReliable and fall back to flow-temperature gradient (Python BOILER_GRADIENT_THRESHOLD_UP=0.2/DOWN=-0.1) when modulation unreliable (SATcontrol.ino:619-628)
- [ ] #4 Add a distinct HEATING_HOT_WATER status (flame on + DHW active); firmware currently shows generic HEATING (SATtypes.h:90-96; Python device/status.py)
- [ ] #5 PREHEATING requires flow >6C below setpoint (Python BOILER_PREHEAT_DELTA=6.0), not any sub-setpoint (SATcontrol.ino:618-623)
- [ ] #6 python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
<!-- AC:END -->
