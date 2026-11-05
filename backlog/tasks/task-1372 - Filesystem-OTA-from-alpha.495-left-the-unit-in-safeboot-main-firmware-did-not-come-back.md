---
id: TASK-1372
title: >-
  Filesystem OTA from alpha.495 left the unit in safeboot (main firmware did not
  come back)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T05:14:00Z'
updated_date: '2026-11-05T17:09:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no repro or fix; ota_flash.py still filesystem first. Needs bench repro; overlaps TASK-1524.

2026-11-04 status: not seen again. scripts/ota_flash.py still flashes filesystem first, then the app via safeboot; since this report it gained TASK-1494 (survives a dropped response after the fs write) and TASK-1497 (checks the image arrived whole). Today about 25 filesystem-first OTAs across beta.60-beta.79 on .61 (OT-Direct) and .63 (Classic+PIC) plus 3 filesystem-only OTAs under a full capture on .63: no safeboot rollback. Not explained: the AC asks for a capture of a real occurrence; keep open until one is seen (watch for 'timeout waiting for main firmware after filesystem update').

Robert 2026-11-05 (session): close by changing scripts/ota_flash.py to flash the app first, then the filesystem (the AC's own alternative), with red/green evidence.

2026-11-05 (sonnet agent, lead verified): scripts/ota_flash.py flashes the app first (safeboot handover, upload, wait for the main firmware) and then the filesystem; --no-fs / --no-app unchanged. A newer fs can no longer land on an older app (the reported alpha.499-on-alpha.495 case). Window new app + old fs: only checklittlefshash()'s LittleFSMismatch warning (helperStuff.ino:831-857), the fs OTA handler checks size and /version.hash only (OTGW-ModUpdateServer-esp32.h:368, 458). The fs OTA reboot inside the app's 30 s pending-verify window is safe: requestDeferredReboot -> performDeferredReboot -> doRestart (helperStuff.ino:595) -> safebootMarkAppValid() (:737). tests/test_ota_flash.py 12 OK; on HEAD's script test_app_is_flashed_before_the_filesystem FAILS (['fs','safeboot','app'] != ['safeboot','app','fs']). evaluate 0 failed. Bench next: beta.99 OTA to .61 with the new order.
<!-- SECTION:NOTES:END -->
