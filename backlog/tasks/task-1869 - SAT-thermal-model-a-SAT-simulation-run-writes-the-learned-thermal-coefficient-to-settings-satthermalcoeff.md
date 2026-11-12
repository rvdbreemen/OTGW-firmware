---
id: TASK-1869
title: >-
  SAT thermal model: a SAT simulation run writes the learned thermal coefficient
  to settings (satthermalcoeff)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T14:38:00Z'
updated_date: '2026-11-12T14:41:00Z'
labels:
  - sat
  - bug
dependencies: []
priority: medium
ordinal: 945000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-09 during the TASK-1817 bench on .61 (beta.216+0a10492, 2026-11-12): with satsimulation on, the persisted satthermalcoeff went from 0.0966 to 0.0998; 09 restored it. Code: the thermal learning block saves the EMA every hour through updateSetting("SATthermalcoeff") (SATcontrol.ino:6678-6683) with no satsimulation guard in that block (read 6625-6690). The self-tune already stops writing in simulation (window file not written, tuner restarts on the sim switch). A sim run should not change the real house parameters. Not decided whether thermal learning should pause in sim or learn in RAM only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: in SAT simulation the thermal learning does not call updateSetting(SATthermalcoeff); red on the current code, green on the fix
- [ ] #2 Real (non-sim) learning still saves every hour, unchanged
<!-- AC:END -->
