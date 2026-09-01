---
id: TASK-1099
title: settings.ini is served without authentication and carries the admin password
status: Done
assignee:
  - '@claude'
created_date: '2026-09-01 18:31'
updated_date: '2026-09-01 19:29'
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
- [x] #1 A request for settings.ini without credentials is refused once an HTTP password is set
- [x] #2 The web UI and FSexplorer keep working with the password set, verified on hardware
- [x] #3 Any other route that serves settings.ini content is covered by the same check
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
handleFile() now gates SETTINGS_FILE with checkHttpAuth() before contentType() rewrites the path. Verified on live hardware (192.168.88.16): with an admin password set, GET /settings.ini without credentials returns 401 and with credentials returns 200; the web UI keeps working with the password set; password cleared afterward and the device restored. Before the fix the same GET returned the file with MQTTpasswd in cleartext.

Follow-up (commit 77797fa4): the initial gate compared against /settings.ini exactly, but //settings.ini resolves to the same file and bypassed it (verified live: 401 vs 200). Gate now collapses a leading // run, matching 2.0.0. Re-verified on hardware: /, // and /// settings.ini all 401 unauthenticated, 200 with credentials.
<!-- SECTION:FINAL_SUMMARY:END -->
