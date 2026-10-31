---
id: TASK-1524
title: >-
  Filesystem OTA during a running capture left LittleFS without its image (only
  kept files restored)
status: To Do
assignee: []
created_date: '2026-10-31T12:49:00Z'
labels:
  - alpha
  - bug
  - ota
dependencies: []
priority: medium
ordinal: 680000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench 2026-10-31 13:42, OTGW32 .61 on alpha.569, a long-run capture running (telnet, /ws frames, MQTT, 5-min snapshots, SAT simulation + OT replay): scripts/ota_flash.py filesystem OTA of alpha.570 got RemoteDisconnected, /version.hash did not change, the unit then answered HTTP 000 for about a minute and afterwards served the 'upload FSexplorer.html' page: LittleFS held only the kept files (settings.ini 5518 B, sat_*.json, ot-*.json). The firmware kept running (no reboot, settings in RAM intact). A second filesystem OTA right after succeeded. A tester who flashes the filesystem while a capture or a busy web UI runs could end with a unit without web pages until a retry.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on the bench: filesystem OTA while a capture (telnet + /ws + MQTT + snapshots) runs; record what the device logs during the upload (telnet, crashlog) and whether the image write fails or the upload stalls
- [ ] #2 Root cause named with evidence (load gate, WS/TCP pressure, timeout in the upload handler, or the kept-file restore path)
- [ ] #3 Either fix it (e.g. refuse or pause other clients during filesystem OTA, or keep the old image until the new one is complete) or document the limit for testers; old-vs-fix bench run
<!-- AC:END -->
