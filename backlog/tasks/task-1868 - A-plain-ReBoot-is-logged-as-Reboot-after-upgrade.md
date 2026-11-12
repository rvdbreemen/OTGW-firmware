---
id: TASK-1868
title: A plain /ReBoot is logged as 'Reboot after upgrade'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T12:46:00Z'
updated_date: '2026-11-12T15:35:00Z'
labels:
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 944000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on the beta.215 bench (TASK-1757 probe report, ba/09). On origin/dev, /ReBoot (FSexplorer.ino:303 -> reBootESP, :735) and the reset-wireless reboot (:744) both go through doRedirect(..., true), which always calls requestDeferredReboot("Reboot after upgrade") (FSexplorer.ino:795). So a user or API reboot, and a Wi-Fi reset, show up in the reboot reason as 'Reboot after upgrade'. Related: TASK-1447 (reboot_log.txt names why the gateway restarted). Our own finding, a small bug: no needs-maintainer-ok.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 doRedirect() takes the reboot reason from its caller: /ReBoot logs a user/API reboot, the reset-wireless path its own reason, and the upgrade path keeps 'Reboot after upgrade'
- [x] #2 A host test or bench check shows the reboot reason after a plain /ReBoot on the old code ('Reboot after upgrade') and on the fix (the user reboot reason)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Claimed by otgw-firmware-95 (2026-11-12, coordinator OK): done in one change with TASK-1746 as beta.219 (same doRedirect/reboot path).

Implementation (95, 2026-11-12, same commit as TASK-1746): doRedirect(msg, wait, URL, const char *rebootReason) replaces the bool; null = no reboot. /ReBoot passes 'User reboot (/ReBoot)', reset wireless 'Reset wireless settings' (both well under the 48-byte reboot-reason record, helperStuff.ino:248). Note on AC1's 'upgrade path keeps Reboot after upgrade': doRedirect() has only these two callers on dev; the OTA paths call requestDeferredReboot('[OTA] Rebooting...' / '[OTA] handover to safeboot') directly (OTGW-ModUpdateServer-esp32.h:248/268) and are unchanged, so the upgrade path keeps its own reason.

Evidence (host, real functions): tests/test_reboot_listener_and_reason.py: old code reports reason 'Reboot after upgrade' for /ReBoot and for reset wireless (2 FAIL); fix reports 'User reboot (/ReBoot)' and 'Reset wireless settings' (OK), the redirect page is still built. AC1 ticked. AC2 asks for 'a host test or bench check' on old and fix: the host test on the real doRedirect/reBootESP shows both, so AC2 ticked too; reboot_log.txt on the device after a /ReBoot is in 09's TASK-1746 bench as a confirmation.
<!-- SECTION:NOTES:END -->
