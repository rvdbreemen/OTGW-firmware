---
id: TASK-1077
title: 'Fix: capture-mqtt-debug.bat crashes right after the interactive prompts'
status: Done
assignee:
  - '@claude'
created_date: '2026-08-17 05:51'
updated_date: '2026-09-04 05:59'
labels:
  - bug
  - tooling
  - needs-info
dependencies: []
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
- [x] #1 The actual failure is identified from the reporter's error text or a local reproduction, not from the hypothesis above alone
- [x] #2 A missing mosquitto_sub or missing winget produces a plain actionable message with a manual-install pointer, instead of an unhandled throw
- [x] #3 A failure between the prompts and the run-folder creation still leaves a diagnosable artefact, or the run folder is created before that window
- [ ] #4 stefan_24213 completes a capture with the fixed script
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-17: root cause confirmed from Stefan's own transcript, not from the hypothesis. summary.txt line: "Error: winget failed to install EclipseMosquitto.Mosquitto (exit code -1978335212)". The exit code is recorded verbatim and deliberately not interpreted: the fix does not depend on why winget failed.

Locally reproduced the sibling branch (winget absent) by running a patched copy with $env:OTGW_FAKE_PF and a stripped PATH. Identical outcome: unhandled throw, red stacktrace, exit 1. Both branches funnel into the same rethrow at the outer catch.

Correction to the original triage: telnet had NOT connected yet when Stefan's run died. Order is browser, crashlog, Resolve-MosquittoSub, telnet writer, telnet connect, so the abort happened before any telnet data existed. That is why telnet.log was "(not present)".

AC #3 needed no work: the run folder and script.error.log are created before the failure window, and Stefan's transcript proves the artefact survived and was uploadable.

Fix: wrap the Resolve-MosquittoSub call site so a failed auto-resolve disables the MQTT stream instead of aborting the capture. An explicit -MosquittoSubPath that does not exist stays fatal. Verified against the bench device: exit 0, no stacktrace, "MQTT capture disabled" in the summary, telnet section populated (25601-byte transcript).

2026-08-17 correction: the winget failure was NOT a broken winget on the reporter's machine. The script used package id EclipseMosquitto.Mosquitto, which matches no package. Verified locally: "winget show EclipseMosquitto.Mosquitto" returns "No package found matching input criteria" with exit code -1978335212, byte for byte the code in the reporter's log, while "winget show EclipseFoundation.Mosquitto" returns Eclipse Mosquitto MQTT broker 2.1.2, publisher Eclipse Foundation. The auto-install was therefore broken for every user without mosquitto_sub already installed, not just for him.

Fixed the id. The live install path was deliberately not executed on the maintainer machine: mosquitto is already installed there, so a winget run would mutate the machine without proving anything the "winget show" resolution does not already prove.

2026-09-04 board cleanup: closed on the maintainer's call. AC #4 (stefan_24213 completes a capture with the fixed script) stays UNCHECKED: no feedback ever came back from the reporter, and the task cannot wait on a person indefinitely.

ACs #1 to #3 are met and shipped. What remains unproven is only the reporter-side confirmation, not the fix.

If the crash resurfaces, reopen from here rather than starting fresh: the diagnosis and the three landed guards are recorded in this task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
capture-mqtt-debug.bat no longer throws unhandled right after the interactive prompts.

The failure was identified rather than assumed (AC #1), and three guards landed: a missing mosquitto_sub or missing winget now produces a plain actionable message with a manual-install pointer instead of an unhandled throw (AC #2), and a failure between the prompts and the run-folder creation still leaves a diagnosable artefact (AC #3).

Closed with AC #4 unchecked. That criterion required the original reporter, stefan_24213, to complete a capture with the fixed script, and no feedback arrived. The fix is not blocked on that confirmation; only the field proof is. Reopen if the crash is reported again.
<!-- SECTION:FINAL_SUMMARY:END -->
