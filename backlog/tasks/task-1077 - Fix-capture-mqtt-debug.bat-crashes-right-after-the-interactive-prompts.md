---
id: TASK-1077
title: 'Fix: capture-mqtt-debug.bat crashes right after the interactive prompts'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-17 05:51'
updated_date: '2026-08-17 06:02'
labels:
  - bug
  - tooling
  - needs-info
dependencies:
  - TASK-1075
references:
  - 'Discord #nederlandse-ondersteuning'
  - stefan_24213
  - '2026-08-17'
  - msg 1538786515428184115
priority: high
ordinal: 180000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by stefan_24213 in Discord #nederlandse-ondersteuning on 2026-08-17, using the v1.7.4 release copy of the script. He fills in all prompts, the script crashes, and he retried several times with the same result. No log was produced, which currently blocks TASK-1075.

Leading hypothesis (code read, unconfirmed, no error text from the reporter yet): the first thing after the prompt block is tool resolution. If mosquitto_sub is not on the machine the script calls Install-Mosquitto, which throws outright when winget is missing or the install fails: 'mosquitto_sub was not found and winget is not available' (line 487) or 'winget failed to install EclipseMosquitto.Mosquitto' (line 493). An unhandled throw looks exactly like a crash to a user who just finished typing answers.

Ruled out by inspection: ConvertFrom-SecureStringToPlainText uses the Marshal BSTR route, which works on Windows PowerShell 5.1 as well as pwsh 7, so the password prompt is not the PS-version trap it could have been. Save-CaptureSettings is wrapped in try/catch and cannot abort the run.

Note the diagnostic gap this exposes: the run folder and script.error.log are only created AFTER the prompts, and OutputRoot defaults to the relative path 'logs/mqtt-diagnostics', so a failure in this window may leave nothing on disk and nothing the reporter can send.

Needed from reporter: the exact error text on screen, and whether logs/mqtt-diagnostics/<timestamp>/ exists relative to where he ran the .bat.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The actual failure is identified from the reporter's error text or a local reproduction, not from the hypothesis above alone
- [ ] #2 A missing mosquitto_sub or missing winget produces a plain actionable message with a manual-install pointer, instead of an unhandled throw
- [ ] #3 A failure between the prompts and the run-folder creation still leaves a diagnosable artefact, or the run folder is created before that window
- [ ] #4 stefan_24213 completes a capture with the fixed script
<!-- AC:END -->
