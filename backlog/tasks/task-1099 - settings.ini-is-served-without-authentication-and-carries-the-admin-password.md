---
id: TASK-1099
title: settings.ini is served without authentication and carries the admin password
status: To Do
assignee: []
created_date: '2026-09-01 18:31'
labels:
  - audit
  - fsexplorer
  - security
dependencies: []
priority: high
ordinal: 196000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
handleFile() serves /settings.ini like any other static file, with no auth check. An operator who sets an HTTP password to lock the gateway down publishes that same password: an unauthenticated client on the LAN reads httppasswd and MQTTpasswd from settings.ini in cleartext and replays them. This differs from the deliberate trusted-LAN posture of /pic in that the credential meant to enforce the lockdown is the thing exposed. Found by the FSexplorer audit of 2026-09-01, finding 2 of 18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A request for settings.ini without credentials is refused once an HTTP password is set
- [ ] #2 The web UI and FSexplorer keep working with the password set, verified on hardware
- [ ] #3 Any other route that serves settings.ini content is covered by the same check
<!-- AC:END -->
