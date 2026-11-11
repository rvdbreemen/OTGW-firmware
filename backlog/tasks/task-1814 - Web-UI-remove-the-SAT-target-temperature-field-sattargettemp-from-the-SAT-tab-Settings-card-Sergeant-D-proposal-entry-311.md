---
id: TASK-1814
title: >-
  Web UI: remove the SAT target temperature field (sattargettemp) from the SAT
  tab Settings card? (Sergeant D proposal, entry 311)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T13:50:00Z'
updated_date: '2026-11-11T19:29:00Z'
labels:
  - webui
  - sat
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
- [x] #1 Robert's decision recorded (remove, move, or keep)
- [x] #2 If removed or moved: every reader of sattargettemp checked (REST, MQTT, home dial, non-HA users) and the web UI change bench-tested with screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 (comms): checked: the field is not the only way to set the SAT target. The Home page dial (v2.js stepTarget, 5491-5509) posts /api/v2/sat/target, and that route writes the same setting (SATcontrol.ino:3446 updateSetting("SATtargettemp")). The setting is also in the REST settings list (restAPI.ino:4234, 4527) and the settings file (settingStuff.ino:358). Removing the field from the SAT Settings card leaves non-HA users the dial and REST. Answered Sergeant D in #dev-sat-mqtt; decision on the layout: Robert.

2026-11-11: Robert decided in the comms session (AskUserQuestion), answer verbatim: "Weghalen (Aanbevolen)" = remove the field from the SAT tab Settings card; the coordinator received the same decision ('Remove it'). Label needs-maintainer-ok removed. Implementer: otgw-firmware-09 after TASK-1798.

2026-11-11 (otgw-firmware-09): implemented. v2.js: sattargettemp out of the SAT_L2 'System' group and into SAT_NOT_IN_FORMS (else the generic loop puts it under 'Other'); SAT_FIELDS/label kept for the REST settings list. Readers re-checked on rebased dev (beta.180): MQTT command sat/target (MQTTstuff.ino:620,664 satHandleTargetTemp), REST settings sattargettemp (restAPI.ino:4234, 4527), Home dial POST /api/v2/sat/target (SATcontrol.ino:3446 updateSetting), settings file (settingStuff.ino:358, 1094). None read the UI field, so all keep working; firmware untouched. Test tests/webui/sat-target-not-in-settings.test.mjs: 3/3 on the fix, 0/3 with OTGW_V2_JS pointing at the old v2.js. Before-screenshot of the System group (scratchpad t1814-before.png) shows 'Target temperature 20'.

2026-11-11 bench: beta.182+c9f37bd OTA to .61 (scripts/ota_flash.py, fs confirmed version.hash c9f37bd, settings 164 keys changed none). shot1814.mjs on the device: System group keys [satsystem, satsource, satheatingmode, satpushsetpoint, satfollowthermostat, satfireplace, satmqttfallback], targetAnywhere=false (not in SAT_L2 or SAT_L3). After-screenshot t1814-after.png (System group only) shows no Target temperature row; before t1814-before.png showed 'Target temperature 20'. Pushed 289331dba..c9f37bd8f, zip posted msg 1570158529887871110.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Robert's decision 'Weghalen' recorded in notes (comms session, AskUserQuestion). AC2: readers checked on dev beta.180 (MQTT sat/target MQTTstuff.ino:620/664, REST settings restAPI.ino:4234/4527, Home dial /api/v2/sat/target SATcontrol.ino:3446, settings file settingStuff.ino:358/1094) - none use the UI field, firmware untouched. Change: v2.js drops sattargettemp from SAT_L2 System and adds it to SAT_NOT_IN_FORMS. Test tests/webui/sat-target-not-in-settings.test.mjs 3/3 fix, 0/3 old v2.js. Bench beta.182 on .61: DOM check targetAnywhere=false; before/after screenshots of the System group. Commit c9f37bd8f (beta.182). Not verified: other UI languages render (label key unchanged, field removed so no translation impact).
<!-- SECTION:FINAL_SUMMARY:END -->
