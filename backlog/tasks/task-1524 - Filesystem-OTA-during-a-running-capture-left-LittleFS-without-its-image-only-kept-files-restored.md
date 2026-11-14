---
id: TASK-1524
title: >-
  Filesystem OTA during a running capture left LittleFS without its image (only
  kept files restored)
status: To Do
assignee:
  - '@claude'
created_date: '2026-10-31T12:49:00Z'
updated_date: '2026-11-14T18:44:00Z'
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

Stale audit 2026-11-14 (coordinator decision): back to To Do, waiting for a reproduction. Not seen again since alpha.569; the REST/file gate changes since then are a candidate reason, not proven.

Research 2026-11-14 (read-only, origin/dev 82560d1d5): the reported end state is what the current code does by design on an interrupted filesystem upload. OTGW-ModUpdateServer-esp32.h _handleUploadAbort() (request->onDisconnect, there is no UPLOAD_FILE_ABORTED callback): Update.end(); for a filesystem upload with bytes written, _remountAfterFsWrite(true) formats the partial image and remounts, _restoreRuntimeFiles() puts settings.ini and the other kept files back from RAM/heap copies (TASK-1495), _markFsUpdateIncomplete() leaves the marker. Result: settings and runtime files intact, no web files, so the upload/helper page is served (TASK-1926 keeps that only for a filesystem without FSexplorer). Since TASK-1774 a reset during the write is also covered (RTC snapshot + boot-time format). So the user-visible effect is 'retry the filesystem upload', not data loss. What research cannot answer is AC2: why the upload connection dropped (RemoteDisconnected) under the capture load on alpha.569. That needs the bench reproduction of AC1 (filesystem OTA while telnet + /ws + MQTT + snapshots run). Proposal: AC3's 'document the limit' part can be met now (an interrupted filesystem OTA shows the upload page; settings are kept; upload again), the root-cause part stays bench-only.

AC3 documentation part done (coordinator, 2026-11-14): docs/guides/WIFI_FIRMWARE_UPDATE.md section 3: the row for an interrupted filesystem step now says what the current code does (half image cleared, settings, reboot counter and reboot log written back, upload page shown; a reset during the write covered by ADR-233; upload again, settings kept) instead of 'settings only live in memory, do not switch off', plus the advice not to run a filesystem update while a capture or another heavy client is connected. AC3 is only partly met: its 'old-vs-fix bench run' part belongs with a fix, if AC2 finds one. AC2 (why the upload dropped) is in 09's brief for the stack build: filesystem OTA on .61 while a capture runs.
<!-- SECTION:NOTES:END -->
