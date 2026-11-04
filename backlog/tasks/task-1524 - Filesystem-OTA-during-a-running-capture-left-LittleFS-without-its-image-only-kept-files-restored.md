---
id: TASK-1524
title: >-
  Filesystem OTA during a running capture left LittleFS without its image (only
  kept files restored)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T12:49:00Z'
updated_date: '2026-11-04T20:22:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no repro or fix. Needs bench repro; overlaps TASK-1372.

2026-11-04 21:07-21:13 repro attempt on .63 (Classic+PIC) beta.79: full capture running (scripts/otgw_capture.py: telnet, MQTT 2521 msgs, browser capture with /ws, snapshots every 30 s, frame recorder 319 frames); three filesystem-only OTAs (ota_flash.py --no-app --fs, beta.79 image) at 21:08:41, 21:10:08, 21:11:10: all rc 0, 'filesystem confirmed /version.hash a9dd048', '/v2.js complete 266132 bytes', settings 175 keys unchanged (cmp before/after identical). Not reproduced (0/3). The original was on .61 OT-Direct alpha.569 with SAT simulation + OT replay; repeat there with that setup after the TASK-1528 night run (.61 busy until ~06:00). Since alpha.569 the REST/file gates changed (TASK-1565 body/gate release, ADR-188 tx cap), a candidate reason it no longer happens; not proven.
<!-- SECTION:NOTES:END -->
