---
id: TASK-2919
title: Public beta v2.0.0-beta.392 (2026-11-19 20:00 slot)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T14:36:00Z'
updated_date: '2026-11-19T15:55:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1057000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta from the release commit on origin/dev (candidate beta.392 = 391 SAT TASK-2913 entries 428-430, 392 UI TASK-2914, build.py TASK-2917). Docs-only release commit: RELEASE_NOTES_2.0.0-beta.392.md, CHANGELOG rollover, README. Build esp32-combo, 60 min soak on .63 by 09, tag on the release commit, draft prerelease --latest=false, publish at 20:00, sha256 of the download, #beta-testing announcement (3 parts) + SAT field-run call, #alpha-testing note. Coordinator go 2026-11-19 15:2x; Robert's standing permission for the 2.x beta slots (2026-11-14). Gate for Sunday's stable: 2 clean SAT field runs on this beta (Robert A, DM 1572941358451265618).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit (docs only) on origin/dev with notes, CHANGELOG [2.0.0-beta.392] and README
- [x] #2 esp32-combo build of the release commit: SUCCESS lines, fresh artifacts; flash zip listing shows sat_log_to_csv.py (TASK-2917 AC1)
- [x] #3 60 min soak on .63 PASS by 09 (bootcount = flash + 2, no crashlog, heap flat)
- [x] #4 Tag v2.0.0-beta.392 on the release commit; GitHub prerelease published with --latest=false; latest stays the 1.x stable; sha256 of the downloaded zip = SHA256SUMS
- [x] #5 #beta-testing announcement (all changes since beta.382) + field-run call, #alpha-testing note; all read back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 15:41-15:48 local (comms 92): release commit 0f9d85af2 pushed (origin/dev = 0f9d85af2), docs only: RELEASE_NOTES_2.0.0-beta.392.md (146 KB; 0 TODO/placeholders, 0 relative links, 0 em dashes; anchor since-the-1x-line ok), CHANGELOG [2.0.0-beta.392] with every task id since beta.382 (2026, 2043, 2023, 2910, 2909, 2913, 2914, 2917), README links. Build esp32-combo of 0f9d85af2 (wt-comms, build.bat via Start-Process, OTGW_BUILD_JOBS=2, llama-server guard): EXIT=0 in 356 s, log 30 KB with 1x 'Successfully created ESP32S3 image' (warm, bootloader cached) + 'Build completed successfully!', 2 non-fatal 'Could not clean build directory' warnings; artifacts 15:47:42-15:48:17, githash 0f9d85a; sha256 ino.bin 9acd02c7..., littlefs.bin 0cf4cd2a..., flash.zip ee58d31e... Zip has sat_log_to_csv.py (TASK-2917 AC1). Build stamp restored. Bench soak requested from 09 on .63.

2026-11-19 16:54-16:58 local: soak .63 PASS by 09 (15:52:10, 61 min: bootcount 257 stable = flash+2, 0 crash/panic, 61/61 REST 200, PIC 6.8, heap flat avg 34258 -> 34126, MQTT 61/61; one MQTT outage 16:52:25-16:53:07 = LAN supervisor restarting a hung mosquitto, not the firmware; settings diff only the 3 updatecheck keys; flashed hashes = build). Robert's explicit yes for this one early publish: DM 1573002637585023049 'A' to 1572988030728544300 (named v2.0.0-beta.392 / 0f9d85af2 / ~17:00), read back by 92. publish392.ps1: tag check (v2.0.0-beta.392^{} = 0f9d85af2), tag pushed, draft untagged-55b7ad54f51cf013f7e1 published --latest=false: isDraft=false, isPrerelease=true, latest = v1.7.9; downloaded flash zip sha256 ee58d31e861c08b198d4bb2ed4e471cd6387c92c9bb40604c82b25b7abf7f48b = SHA256SUMS; remote tag = 0f9d85af2c28902c61f17ce99d0831982fa7b56e. Body 102,602 chars (trimmed, links the full notes at the tag). Discord (read back, identical): #beta-testing 1573003528601997312 / 1573003537246588929 / 1573003545937051679, field-run call 1573003555198205952, #alpha-testing note 1573003564056576192.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.392 published 2026-11-19 ~16:57 local (early, Robert's explicit yes DM 1573002637585023049). AC1: docs-only release commit 0f9d85af2 on origin/dev (notes 146 KB, CHANGELOG [2.0.0-beta.392] with all 8 tasks since 382, README). AC2: esp32-combo build of 0f9d85af2 EXIT=0, 'Build completed successfully!', artifacts 15:47-15:48 githash 0f9d85a; zip contains sat_log_to_csv.py (TASK-2917 AC1). AC3: 60-min soak .63 PASS by 09 (bootcount stable, 0 panic, 61/61 REST, heap flat, MQTT connected; one broker restart by the LAN supervisor). AC4: tag on 0f9d85af2, prerelease published --latest=false, latest stays v1.7.9, download sha256 ee58d31e... = SHA256SUMS. AC5: #beta-testing 3 parts + SAT field-run call + #alpha-testing note, all read back. Not verified: the SAT rule of beta.391 with a real boiler (the field runs are the test); TASK-2917 AC3 (a tester capture from the new zip).
<!-- SECTION:FINAL_SUMMARY:END -->
