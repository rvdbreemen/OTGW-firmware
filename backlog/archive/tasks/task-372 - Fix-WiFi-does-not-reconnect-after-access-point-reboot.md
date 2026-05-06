---
id: TASK-372
title: 'Fix: WiFi does not reconnect after access point reboot'
status: Done
assignee: []
created_date: '2026-04-21 21:32'
updated_date: '2026-04-21 21:35'
labels:
  - bug
dependencies: []
references:
  - 'GitHub #551'
  - user kroon040
  - '2026-04-21'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User kroon040 reports that after upgrading from v0.10 to v1.3.5, the OTGW no longer reconnects to WiFi when the access point is rebooted. A full power cycle of the OTGW is required to restore connectivity. In v0.10 this was not an issue. v1.3.5 changed the WiFi state machine timeout from 5s to 30s (TASK for that fix was about periodic disconnects), but this scenario — AP reboot — may have regressed. Unknown if the bug also affects 1.4.x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW reconnects to WiFi automatically after the access point reboots, without requiring a power cycle of the OTGW
- [ ] #2 Regression from v0.10 confirmed fixed: behaviour matches or exceeds v0.10 WiFi reconnect reliability
- [ ] #3 Telnet debug logs confirm the WiFi state machine re-enters the connect cycle after AP becomes available again
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: telnet debug logs from reporter showing what happens when AP reboots; confirmation whether bug also occurs on 1.4.x

2026-04-21: Fix already in dev branch (commits 6a5857b7 + 788c2982, merged 2026-04-07). Not yet in any released version (last release = v1.3.5). GitHub comment posted pointing reporter to dev branch. Task can be closed when next release ships.
<!-- SECTION:NOTES:END -->
