---
id: TASK-2023
title: >-
  Firmware update straight from GitHub instead of a manual zip upload (Sergeant
  D request)
status: To Do
assignee: []
created_date: '2026-11-18T11:54:00Z'
labels:
  - feature
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 1038000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Asked by Sergeant D in #dev-sat-mqtt 2026-11-18 11:37 UTC (https://discord.com/channels/812969634638725140/1105556725714649128/1572576499674914927): 'can we work on a new update process, like downloading the new version directly from GitHub repository instead of the user manually upload the zip file to the FW?' Constraint: the firmware is HTTP only (ADR-003, CLAUDE.md core rule 'No HTTPS or WSS'); GitHub release assets are HTTPS only; the app slot is tight on flash (TASK-1955). Options put to him (reply 1572580626995875892): A) helper on a PC or Home Assistant that downloads the zip from GitHub and pushes it to /update (no firmware change); B) gateway shows 'new version available' with a link, from a version file on a plain HTTP mirror; C) HTTPS in the firmware (reverses ADR-003, needs flash). Maintainer decision needed before any work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert chose an option (or declined), recorded in this task
- [ ] #2 If A or B: the design is checked against ADR-003 and the open-source-first rule (existing updaters researched)
<!-- AC:END -->
