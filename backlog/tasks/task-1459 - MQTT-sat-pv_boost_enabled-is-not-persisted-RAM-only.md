---
id: TASK-1459
title: MQTT sat/pv_boost_enabled is not persisted (RAM only)
status: To Do
assignee: []
created_date: '2026-10-29T07:19:00Z'
labels:
  - sat
  - mqtt
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 619000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. satHandlePvBoostEnabled (SATcontrol.ino:1949-1961) sets RAM only, while the other SAT switches go through updateSetting (MQTTstuff.ino:669+); the settings key exists (settingStuff.ino:1117).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An MQTT toggle of sat/pv_boost_enabled survives a reboot (test red on HEAD, green on fix)
<!-- AC:END -->
