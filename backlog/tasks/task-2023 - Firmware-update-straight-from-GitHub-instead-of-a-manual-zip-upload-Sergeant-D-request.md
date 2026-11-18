---
id: TASK-2023
title: >-
  Web UI: show 'new version available' from the GitHub releases API, checked in
  the browser (option B, Robert 2026-11-18)
status: To Do
assignee:
  - '@otgw-webui'
created_date: '2026-11-18T11:54:00Z'
updated_date: '2026-11-18T12:53:00Z'
labels:
  - feature
milestone: 2.0.0
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
- [x] #1 Robert chose an option (or declined), recorded in this task
- [ ] #2 No firmware HTTP change: the check runs client-side in the web UI against the GitHub releases API; the gateway makes no outbound request (code review + network trace in the browser)
- [ ] #3 The UI compares the latest release with the running fwversion and shows a notice with a link to the release page when it is newer; prerelease (beta) vs stable handled: a 2.x beta user is told about newer betas and stable releases, a stable user only about stable unless they opt in; tests on the real web UI files
- [ ] #4 A setting switches the check off; with it off, no request to GitHub is made (browser network trace)
- [ ] #5 Privacy note in the manual (EN and NL): the browser contacts GitHub when the check is on, and how to switch it off
- [ ] #6 The check respects the web UI's limit of at most 2 API calls in flight and fails quietly when GitHub is unreachable or rate-limited (screenshot of both states)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 Maintainer decision (Robert, DM 1572585865589428318, relayed by the coordinator), verbatim: "B en neem taak C op voor een latere release 2.1.0". Scope of this task is now option B only: the gateway shows 'new version available' with a link, from a version file on a plain HTTP mirror (no HTTPS on the device, ADR-003). Option C (HTTPS self-update) is filed as its own task for 2.1.0 (see the 2.1.0 task referencing ADR-218).

OPEN QUESTION for Robert (infra choice, coordinator 2026-11-18: put to him when someone picks up B, not before): which host serves the version file over plain HTTP, and who keeps it in sync with each release (by hand, or a step in /beta-release-2x and /release-2x).

Robert in #dev-sat-mqtt 2026-11-18 12:06 UTC (https://discord.com/channels/812969634638725140/1105556725714649128/1572583679895797861), verbatim: "For 2.0.0 we do B and for 2.1.0 a POC for updating from GitHub directly." So option B targets 2.0.0.

2026-11-18 PENDING with Robert (coordinator put it as 1/2): how the version check gets its data. (1) A plain-HTTP host we run that serves the version file; needs hosting and a sync step per release. (2) No host: the web UI in the user's browser asks the GitHub releases API itself (HTTPS is fine in the browser, the gateway stays HTTP-only per ADR-003), with a privacy note (the browser contacts GitHub) and an off-switch. Coordinator recommends 2. Not decided yet.

2026-11-18 Robert chose hosting option 2 (DM 1572594516429381716, a reply to 1572588660904296520, relayed by the coordinator), verbatim: "2". Design: no host and no firmware HTTP change; the web UI in the user's browser asks the GitHub releases API (HTTPS in the browser; the gateway itself stays HTTP-only per ADR-003), compares with the running fwversion and shows 'new version available' with a link to the release page; a privacy note in the docs (the browser contacts GitHub) and a setting to switch the check off. Owner: otgw-webui, after their BLE bug (coordinator). ACs rewritten for this design.
<!-- SECTION:NOTES:END -->
