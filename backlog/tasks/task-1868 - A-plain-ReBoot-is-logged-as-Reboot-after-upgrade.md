---
id: TASK-1868
title: A plain /ReBoot is logged as 'Reboot after upgrade'
status: To Do
assignee: []
created_date: '2026-11-12T12:46:00Z'
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
- [ ] #1 doRedirect() takes the reboot reason from its caller: /ReBoot logs a user/API reboot, the reset-wireless path its own reason, and the upgrade path keeps 'Reboot after upgrade'
- [ ] #2 A host test or bench check shows the reboot reason after a plain /ReBoot on the old code ('Reboot after upgrade') and on the fix (the user reboot reason)
<!-- AC:END -->
