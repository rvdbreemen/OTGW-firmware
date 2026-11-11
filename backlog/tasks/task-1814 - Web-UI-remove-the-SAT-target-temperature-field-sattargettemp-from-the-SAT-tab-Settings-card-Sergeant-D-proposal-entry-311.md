---
id: TASK-1814
title: >-
  Web UI: remove the SAT target temperature field (sattargettemp) from the SAT
  tab Settings card? (Sergeant D proposal, entry 311)
status: To Do
assignee: []
created_date: '2026-11-11T13:50:00Z'
updated_date: '2026-11-11T13:53:00Z'
labels:
  - webui
  - sat
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 907000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-11 13:44:46 UTC, #dev-sat-mqtt msg 1570071794612707380 (screenshot of the 'Target temperature' field, 21 C): 'In the sat tab -> Settings card we have the target temperature input field. It is not necessary to be there because the SAT climate entity already handles the room setpoint'. The field is setting sattargettemp (src/OTGW-firmware/data/v2.js:3950, label v2.js:1201). UI layout is Robert's decision; check what else reads sattargettemp (REST, MQTT, the home dial, users without Home Assistant) before removing it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's decision recorded (remove, move, or keep)
- [ ] #2 If removed or moved: every reader of sattargettemp checked (REST, MQTT, home dial, non-HA users) and the web UI change bench-tested with screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 (comms): checked: the field is not the only way to set the SAT target. The Home page dial (v2.js stepTarget, 5491-5509) posts /api/v2/sat/target, and that route writes the same setting (SATcontrol.ino:3446 updateSetting("SATtargettemp")). The setting is also in the REST settings list (restAPI.ino:4234, 4527) and the settings file (settingStuff.ino:358). Removing the field from the SAT Settings card leaves non-HA users the dial and REST. Answered Sergeant D in #dev-sat-mqtt; decision on the layout: Robert.
<!-- SECTION:NOTES:END -->
