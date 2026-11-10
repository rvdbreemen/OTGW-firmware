---
id: TASK-1786
title: 'Classic + S3 Mini: PIC no longer detected after flashing with --migrate'
status: To Do
assignee: []
created_date: '2026-11-10T18:52:00Z'
updated_date: '2026-11-10T19:38:00Z'
labels:
  - needs-maintainer-ok
  - bug
  - support
milestone: 2.0.0
dependencies: []
ordinal: 881000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report from thaghostnl, #nederlandse-ondersteuning 2026-11-10 19:44:03 UTC, msg 1569784722903662795 (https://discord.com/channels/812969634638725140/815561033036333076/1569784722903662795): "Zie ik iets over het hoofd? Op de nodo-shop Opentherm Wi-Fi heb ik nu de LOLIN S3 mini en na het flashen met de migrate optie, wordt de PIC niet meer gedetecteerd." (Nodo Shop OpenTherm Wi-Fi gateway, now with a LOLIN S3 Mini; after flashing with the migrate option the PIC is no longer detected.) No capture yet; asked for one including a reboot, plus the firmware version (bot reply msg 1569786232030888029). Related: ADR-201 (combo board detection persists only a positive PIC verdict and re-probes every boot) and TASK-1774 (rollback / settings reset after the filesystem OTA); in the capture also check whether the settings were still there after the migrate flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture from the reporter shows the boot and the PIC detection (or the reporter is asked again)
- [ ] #2 Cause found from the capture and a fix or answer agreed with Robert
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for capture.

Time correction (2026-11-10): the report time above is local CET. From the message snowflake the correct time is 2026-11-10 18:44:03 UTC (msg 1569784722903662795).

Captures received 2026-11-10 18:10 UTC (msgs 1569791410595565642, 1569791462365724854), analysed in the comms session (local copy RvdB/capture-thaghost-1110). 1.7.7 (Wemos, IP .42, host otgw): PIC 'gateway v6.8 id pic16f1847', MQTT connected to 192.168.10.2, uptime 15 min. 2.0.0-beta.170+d260e69 (S3, MAC AC:27:6E:CB:E5:90, IP .66, host OTGW2): /api/v2/device/info board 'Nodoshop OTGW Combo (ESP32-S3)', hardwaremode Degraded, otcommandinterface None, otdirectavailable false; telnet banner PIC 'no pic found', OTbus offline; 'PIC is unknown, probe pic using PR=A' at 18:59:05 UTC with no PIC answer in the 11 s left; 0 OT frames. Board Mode 0 (auto). reboot_log: 18:54:05 power on, 18:58:03 software reset 'Reboot after upgrade'; the capture started 18:58:35, 32 s after that reboot, so the boot-time detection lines are not in it. Settings on 2.0.0 look like defaults (MQTT disabled, broker homeassistant.local), unlike 1.7.7 (broker 192.168.10.2): settings were not carried over (relates to TASK-1774?), not verified how he flashed. Next: a capture that is running BEFORE a restart (restart from the web UI during the capture), plus confirm it is the same gateway board and how the S3 was seated.
<!-- SECTION:NOTES:END -->
