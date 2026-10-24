---
id: TASK-1357
title: Remove the dead SAT settings satautogains (fAutoGainsValue) and bAutoGains
status: To Do
assignee: []
created_date: '2026-10-24T19:11:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 492000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1354 (2026-10-24): no firmware code reads satautogains / settings.sat.fAutoGainsValue (only stored settingStuff.ino:387/1105, served restAPI.ino:4006-4007, published SATcontrol.ino:2746/3200 + MQTTstuff.ino:730, help settingsHelp.h:139 'the PID does not read it today'), and settings.sat.bAutoGains is read at SATpid.ino:122 but never set (default true, SATtypes.h:540), so the PID always computes gains from the heating curve. Robert approved the removal 2026-10-24 (contract change: REST/MQTT/HA). Remove end to end like TASK-1332/1341: field(s), settings file (old keys load and are ignored), REST GET/list/whitelist/POST, MQTT command + state topic, HA entity if any (retire via kRetiredSatEntities), v2.js wizard writes satautogains -> stop writing it, settingsHelp, docs/manuals/MQTT.md; keep the PID behaviour identical (gains from the curve).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No code, setting, REST/MQTT key, HA entity or UI field for satautogains/bAutoGains remains; an old settings file with the keys loads; PID gains unchanged (host test: same gains before/after); retired HA config cleared
<!-- AC:END -->
