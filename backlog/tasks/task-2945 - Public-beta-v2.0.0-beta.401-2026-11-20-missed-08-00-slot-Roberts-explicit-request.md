---
id: TASK-2945
title: >-
  Public beta v2.0.0-beta.401 (2026-11-20, missed 08:00 slot, Robert's explicit
  request)
status: To Do
assignee: []
created_date: '2026-11-20T08:14:00Z'
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
- [ ] #1 Release commit (docs only) on origin/dev with notes, CHANGELOG [2.0.0-beta.401] and README
- [ ] #2 esp32-combo build of the release commit: SUCCESS lines, fresh artifacts, zip listing incl. sat_log_to_csv.py
- [ ] #3 Soak on .63 by 09 PASS (real length stated)
- [ ] #4 Tag v2.0.0-beta.401 on the release commit; prerelease published --latest=false; latest stays 1.x; download sha256 = SHA256SUMS
- [ ] #5 #beta-testing announcement (all changes since beta.392) + #alpha-testing note, read back
<!-- AC:END -->
