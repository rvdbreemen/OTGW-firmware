---
id: TASK-1886
title: 'SAT: bring back the old hourly heating curve auto-tune (Sergeant D entry 325)'
status: To Do
assignee: []
created_date: '2026-11-13T07:23:00Z'
labels:
  - sat
  - adr
milestone: 2.0.0
dependencies: []
ordinal: 959000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D decision 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570697693968212011), entry 325 in docs/sat/sergeant-d-corrections.md: bring back the hourly coefficient auto-tune removed by TASK-1413 (02029bf75; entry 72 Q22 'Remove'). Old code: 02029bf75^:src/OTGW-firmware/SATcontrol.ino:5578-5688 (satAutoTuneUpdate, cycle-class counting, no room temperature). Conflicts to resolve in the plan: ADR-205, ADR-228, ADR-230, ADR-231 (new self-tune) and entries 302-307, 313, 315, 316 (Sergeant D AND Robert), 318, 322, 323. Open (entry 325): new self-tune removed / default off / kept beside; old tuner unchanged or changed; switch default. SAT control change: plan with citations and Sergeant D's OK before implementation (implement-next-task step 4); Robert to see first because entry 72/316 involve him; needs a superseding ADR.
<!-- SECTION:DESCRIPTION:END -->
