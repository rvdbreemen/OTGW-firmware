---
id: TASK-1786
title: 'Classic + S3 Mini: PIC no longer detected after flashing with --migrate'
status: To Do
assignee: []
created_date: '2026-11-10T18:52:00Z'
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
<!-- SECTION:NOTES:END -->
