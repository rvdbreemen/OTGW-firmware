---
id: TASK-73
title: 'Climate Entity: Activity and Home presets for full preset parity'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-06 19:14'
updated_date: '2026-04-06 20:25'
labels:
  - ha-entity
  - climate
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py
    (lines 175-179, preset temperature defaults)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 604-640, preset mode handling)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 800-833, window sensor -> activity preset)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python supports 5 presets: Activity, Away, Home, Sleep, Comfort. The firmware currently has Away, Eco, Comfort, Sleep but is missing Activity and Home. Activity is used for window-open detection (temp drops to activity preset when window opens). Home is the standard 'at home' preset. Add these presets with configurable temperatures (Activity default 10C, Home default 18C) and ensure preset sync to secondary climate entities works for all presets.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Activity preset added with configurable temperature (default 10C)
- [ ] #2 Home preset added with configurable temperature (default 18C)
- [ ] #3 Window open triggers Activity preset (restore on window close)
- [ ] #4 Settings: SATpresetactivity (float), SATpresethome (float)
- [ ] #5 MQTT preset command accepts 'activity' and 'home' values
- [ ] #6 REST API preset endpoint accepts 'activity' and 'home'
- [ ] #7 Preset sync to secondary entities works for all presets including activity/home
<!-- AC:END -->
