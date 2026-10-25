---
id: TASK-1372
title: >-
  Filesystem OTA from alpha.495 left the unit in safeboot (main firmware did not
  come back)
status: To Do
assignee: []
created_date: '2026-10-25T05:14:00Z'
labels:
  - ota
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 507000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-10-25 bench (OTGW32 OT-Direct, 4MB safeboot layout): scripts/ota_flash.py flashed the filesystem first (POST /update?cmd=100) of build alpha.499+ffff5b8 onto a unit running alpha.495+49bb211; the main firmware did not answer again ('timeout waiting for main firmware after filesystem update') and the unit came up in safeboot (rollback). Uploading the app through safeboot /update recovered it (alpha.499+ffff5b8 runs, settings kept: 177 keys). Not seen on earlier fs-first OTAs today (alpha.488-.497). Suspects: a crash of the old app with the new filesystem (v2 assets/settings keys), or the app still in 'pending verify' state when the fs restart happened (ADR-191 rollback: an image is valid only after 30 s healthy loop or a planned restart). Reproduce: fs OTA of a newer build onto an older running app; capture serial/telnet and the crashlog.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced or explained with a capture (crashlog / serial / rollback state); if a defect, fixed so a filesystem OTA never sends a unit to safeboot, or ota_flash.py orders app/fs so it cannot happen (red/green evidence)
<!-- AC:END -->
