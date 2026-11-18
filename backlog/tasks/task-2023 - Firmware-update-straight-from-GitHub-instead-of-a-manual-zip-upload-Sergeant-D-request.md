---
id: TASK-2023
title: >-
  Show 'new version available' in the gateway from a version file on a plain
  HTTP mirror (option B of the GitHub-update request, Robert 2026-11-18)
status: To Do
assignee: []
created_date: '2026-11-18T11:54:00Z'
updated_date: '2026-11-18T12:24:00Z'
labels:
  - feature
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 Maintainer decision (Robert, DM 1572585865589428318, relayed by the coordinator), verbatim: "B en neem taak C op voor een latere release 2.1.0". Scope of this task is now option B only: the gateway shows 'new version available' with a link, from a version file on a plain HTTP mirror (no HTTPS on the device, ADR-003). Option C (HTTPS self-update) is filed as its own task for 2.1.0 (see the 2.1.0 task referencing ADR-218).

OPEN QUESTION for Robert (infra choice, coordinator 2026-11-18: put to him when someone picks up B, not before): which host serves the version file over plain HTTP, and who keeps it in sync with each release (by hand, or a step in /beta-release-2x and /release-2x).
<!-- SECTION:NOTES:END -->
