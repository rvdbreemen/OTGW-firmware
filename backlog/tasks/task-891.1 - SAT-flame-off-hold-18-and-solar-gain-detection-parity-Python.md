---
id: TASK-891.1
title: 'SAT: flame-off hold +18 and solar-gain detection parity (Python)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-20 19:19'
updated_date: '2026-06-20 19:38'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/SATtypes.h
  - src/OTGW-firmware/settingStuff.ino
parent_task_id: TASK-891
priority: high
ordinal: 108000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Align SAT control constants/logic to thermo-nova Python (heating_control.py, solar_gain.py, const.py). From the SAT spec-parity audit 2026-06-20; George ruled 'Python always wins'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Flame-off hold uses return_temperature + 18.0 (Python flame_off_setpoint_offset_celsius=18.0): fFlameOffOffset default 0.0->18.0 (SATtypes.h:429) AND raise the settingStuff.ino:958 clamp from 0-5 to allow 18 (e.g. 0-30)
- [ ] #2 Flame-off hold clamped to [COLD_SETPOINT, maximum_setpoint] per Python heating_control.py:397-398 (firmware SATcontrol.ino:799)
- [ ] #3 Solar-gain detection ANDs the valves_open condition (state.sat.bValvesOpen) on top of elevation/rise/low-output (SATcontrol.ino:3881-3962; Python solar_gain.py)
- [ ] #4 Solar min-rise default 0.5->0.6 (fSolarMinRiseRate SATtypes.h:461; Python CONF_SOLAR_GAIN_MIN_RISE_PER_HOUR=0.6)
- [ ] #5 python build.py --target esp32 exits 0; evaluate.py --quick no new failures; prerelease bumped
<!-- AC:END -->
