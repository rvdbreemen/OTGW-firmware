---
id: TASK-1920
title: >-
  The filesystem image ships a default settings.ini that differs from the
  firmware's own first-boot defaults and makes a fresh unit look like a 1.x
  upgrade
status: To Do
assignee: []
created_date: '2026-11-14T04:28:00Z'
labels:
  - bug
  - settings
  - filesystem
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 985000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1774 (code reading, not seen on a device). src/OTGW-firmware/data/settings.ini (400 B) is part of the LittleFS image: the beta.233 littlefs.bin contains it (grep -a), build.py stage_data_dir() copies data/ whole. Compared with what readSettings() writes when the file is missing (settingStuff.ino:571-576, writeSettings() from the struct defaults): (1) MQTTenable false in the file, true in the struct (MQTTstuff.h:54); (2) OTGWcommands GW=1 in the file, empty in the struct (PICtypes.h:81); (3) the file has no MQTTuseLegacyOtTopics key, so readSettings() takes the 1.x-upgrade path and turns legacy OT topics on (settingStuff.ino:654-658), where a missing file gives new topics; (4) the file has no ui_onboarded key, so the first-time-setup wizard is marked done (settingStuff.ino:671-675), against the TASK-997 comment that a fresh install has no settings file; (5) no SATbleenable key: the no-PSRAM BLE default applies either way. Hostname OTGW2 and broker homeassistant.local match the struct. Consequences: a unit flashed with an image (USB full flash, or any filesystem OTA before its restore) starts on this file, not on the firmware defaults; and every filesystem OTA briefly replaces the user's settings.ini with it (TASK-1774 / ADR-233 cover the reset case). Options for Robert: A remove data/settings.ini from the image (first boot then uses the struct defaults: MQTT on, no GW=1, new topics, wizard shown); B keep it but bring it in line with the struct defaults and add the missing keys; C keep as is (document that a fresh flash behaves like a 1.x upgrade). Not changed in TASK-1774.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Verified on a bench unit what a fresh image flash starts with (settings, MQTT topic naming, wizard state), against a unit whose settings.ini was deleted
- [ ] #2 Robert chose A, B or C (put by the coordinator with the consequences above)
- [ ] #3 The chosen option is implemented with a host or build check that fails on the old image content
<!-- AC:END -->
