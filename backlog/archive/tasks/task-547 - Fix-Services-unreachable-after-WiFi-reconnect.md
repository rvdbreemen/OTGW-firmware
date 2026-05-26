---
id: TASK-547
title: 'Fix: Services unreachable after WiFi reconnect'
status: Done
assignee: []
created_date: '2026-05-06 09:04'
updated_date: '2026-05-06 09:09'
labels:
  - bug
  - wifi
dependencies: []
references:
  - 'GitHub #560'
  - reporter andrebrait
  - '2026-05-04'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After a reboot the device reconnects to WiFi, gets an IP, and is pingable — but telnet, curl, and the webUI are all unreachable. Forcing a reconnect through the UniFi dashboard immediately restores access. Root cause identified by reporter: the WIFI_RECONNECTED handler in networkStuff.ino restarts services (startTelnet, startOTGWstream, etc.) without first stopping their old instances, leaving stale port bindings that block the new servers from accepting connections. Reporter: andrebrait (GitHub #560).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Device is fully accessible (telnet, webUI, curl) immediately after WiFi reconnects following a reboot, without requiring manual intervention
- [ ] #2 debugTelnet and OTGWstream are stopped before being restarted in the WIFI_RECONNECTED handler
- [ ] #3 No regression in normal WiFi reconnect behaviour (TASK-372 fix remains intact)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-06: Fixed in v1.5.0-beta.16, confirmed resolved by andrebrait. GitHub #560 closed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed in v1.5.0-beta.16. Reporter andrebrait confirmed the issue is resolved. GitHub issue #560 closed as completed.
<!-- SECTION:FINAL_SUMMARY:END -->
