---
id: TASK-2945
title: >-
  Public beta v2.0.0-beta.401 (2026-11-20, missed 08:00 slot, Robert's explicit
  request)
status: Done
assignee: []
created_date: '2026-11-20T08:14:00Z'
updated_date: '2026-11-20T09:57:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1076000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Missed 08:00 beta (the compose reminder died with the reboot). Robert in 92's CLI 2026-11-20 ~09:1x: 'Kan je alsnog de beta release uitvoeren aub'; coordinator GO + dev freeze. Release commit (docs only) on origin/dev 9e28c7795 (beta.401: 393-401 + capture TASK-2922; no TASK-2937). Build esp32-combo, 60 min soak .63 by 09, tag on the release commit, prerelease --latest=false, sha256 of the download, #beta-testing posts + #alpha-testing note.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit (docs only) on origin/dev with notes, CHANGELOG [2.0.0-beta.401] and README
- [x] #2 esp32-combo build of the release commit: SUCCESS lines, fresh artifacts, zip listing incl. sat_log_to_csv.py
- [x] #3 Soak on .63 by 09 PASS (real length stated)
- [x] #4 Tag v2.0.0-beta.401 on the release commit; prerelease published --latest=false; latest stays 1.x; download sha256 = SHA256SUMS
- [x] #5 #beta-testing announcement (all changes since beta.392) + #alpha-testing note, read back
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.401 published 2026-11-20 ~11:05 local on Robert's explicit request (missed 08:00 slot after a PC restart). AC1: docs-only release commit 0bee30932 on origin/dev (RELEASE_NOTES_2.0.0-beta.401.md, CHANGELOG [2.0.0-beta.401] incl. TASK-2037/2928/2931/2922/2932 lines, README). AC2: esp32-combo build of 0bee30932 EXIT=0 in 611 s, 'Build completed successfully!', artifacts 09:34-09:35 githash 0bee309; flash zip 15 files incl. sat_log_to_csv.py. AC3: soak .63 by 09 PASS (coordinator decision A): device counters 72 min without restart/crash (bootcount 298 unchanged, crashlog 404, hd_min_free_heap 26100 unchanged, MQTT connected, PIC 6.8), continuous capture only 4 min because of a PC restart; stated so in the release body and the post (OTGW-field-data/bench-63/2026-11-20_soak-beta401-0bee309). AC4: tag v2.0.0-beta.401 on 0bee30932 (remote ^{} verified), prerelease published by release id 452443172 with make_latest=false, latest stays v1.7.9, download sha256 5314dfae... = SHA256SUMS. AC5: #beta-testing 1573275798587183225 / 1573275807324049439, #alpha-testing 1573275815758790768, all read back. Not verified: SAT against a real boiler (field runs); the notes file at the tag says '60 min run' (true; the GitHub body has the exact 72/4 min wording).
<!-- SECTION:FINAL_SUMMARY:END -->
