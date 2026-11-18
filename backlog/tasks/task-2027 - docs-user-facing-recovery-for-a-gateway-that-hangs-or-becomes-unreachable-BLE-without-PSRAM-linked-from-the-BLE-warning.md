---
id: TASK-2027
title: >-
  docs: user-facing recovery for a gateway that hangs or becomes unreachable
  (BLE without PSRAM), linked from the BLE warning
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T13:58:00Z'
updated_date: '2026-11-18T14:02:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1039000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator request 2026-11-18. ch07/h07 had power-cycle steps per symptom but no recovery for a hang. New 7.12a EN+NL: save /api/v2/settings; power-cycle and switch BLE off in the window (about 1-2 min, 09 bench observation); USB flash_otgw.bat --update --fs (WiFi kept, settings reset, BLE off by default on no-PSRAM, settingStuff.ino:684-690); factory flash last. Not --migrate (restores settings.ini, flash_otgw.bat:20-24).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 7.12a in ch07 and h07, every step checked against code or tool source
- [x] #2 Linked from ch05/h05 5.16 PSRAM text and from 7.12
- [x] #3 7.13 Method 1 names /settings.ini (SETTINGS_FILE, OTGW-firmware.h:138)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sources per step: BLE starts in initSAT() via satBLEInit() (SATcontrol.ino:6331, OTGW-firmware.ino:639); bleActive() = enabled && (psram || riskack) (SATble.ino:625); no SATbleenable key on no-PSRAM -> off (settingStuff.ino:684-690); UI label 'Enable BLE sensors' / 'BLE-sensoren inschakelen' under Sensors, BLE sensors (v2.js:4317, i18n/nl.json:1040); flash_otgw.bat --update --fs keeps NVS, resets settings.ini (:11-18); --migrate restores settings.ini (:20-24); flash_otgw.sh has no --update (options :145-152, filed separately); window ~1-2 min is 09's bench observation, labelled as such. Coordinator corrections taken: 'hangs or becomes unreachable', not 'restart loop' (DM 1572601402079973387). No em dashes.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
7.12a added in ch07 and h07 (AC1, per-step sources in the notes), links from 5.16 and 7.12 (AC2), 7.13 /settings.json -> /settings.ini (AC3). Docs only; not verified on a hung unit by me (the recovery steps rest on code and tool source, the 1-2 min window on 09's bench run).
<!-- SECTION:FINAL_SUMMARY:END -->
