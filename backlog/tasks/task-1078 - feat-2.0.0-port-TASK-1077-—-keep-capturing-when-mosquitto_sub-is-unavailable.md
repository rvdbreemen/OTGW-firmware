---
id: TASK-1078
title: 'feat-2.0.0: port TASK-1077 — keep capturing when mosquitto_sub is unavailable'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-17 06:17'
updated_date: '2026-08-17 06:19'
labels:
  - bug
  - tooling
dependencies: []
priority: medium
ordinal: 261000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of the otgw-1.x.x fix in commits c2b3dd7cd and aeb006863.

On 1.x a field report (stefan_24213, Discord #nederlandse-ondersteuning, 2026-08-16) showed the capture script aborting the entire run when mosquitto_sub could not be resolved. Resolve-MosquittoSub throws when winget is absent, when the winget install fails, or when -SkipToolInstall is set and nothing is found, and the outer catch rethrows, so the user sees a red stacktrace. The abort happens before telnet connects, so the reporter got a transcript with both telnet.log and mqtt.log '(not present)': the log the script exists to produce was never written.

This branch carries the same structure at scripts/capture-mqtt-debug.bat:2140, unguarded, with the same downstream $mqttProcess.HasExited dereference at 2172.

Only half of the 1.x work needs porting. The wrong winget package id (EclipseMosquitto.Mosquitto, which matches no package) was already corrected here: line 453 uses EclipseFoundation.Mosquitto.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A failed auto-resolve of mosquitto_sub disables the MQTT stream and states the reason loudly, instead of aborting the run
- [x] #2 An explicit -MosquittoSubPath that does not exist stays fatal
- [x] #3 The capture completes with a populated telnet section and exit code 0 when mosquitto_sub and winget are both unavailable
- [x] #4 No console line promises MQTT capture when MQTT is disabled
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-17: ported the guard from otgw-1.x.x commit c2b3dd7cd. The sibling half of that work, the wrong winget package id, needed no port: this branch already used EclipseFoundation.Mosquitto.

Verified on this branch with mosquitto_sub and winget both hidden (patched copy using $env:OTGW_FAKE_PF and a stripped PATH), captured against a live device: exit code 0, no stacktrace, "MQTT capture disabled" recorded in both the summary and script.error.log, telnet section populated, 27505-byte transcript.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports the otgw-1.x.x capture-script fix (c2b3dd7cd) so a missing mosquitto_sub no longer destroys an entire capture run.

Resolve-MosquittoSub throws when winget is absent, when the winget install fails, or when -SkipToolInstall is set and nothing is found. The call site did not catch it and the outer handler rethrows, so the user got a red stacktrace instead of a capture. Because the abort happens before the telnet connection is made, the telnet log the script exists to produce was never written: the 1.x field report that started this shows a transcript with both telnet.log and mqtt.log '(not present)'.

The auto-resolve call site now catches the failure, disables the MQTT stream, states the reason with a manual-install pointer and a -MosquittoSubPath hint, and keeps capturing. An explicit -MosquittoSubPath that does not exist stays fatal. Also guards the $mqttProcess.HasExited dereference in the capture loop and stops the console line from promising MQTT capture when MQTT is off.

Not ported: the winget package id correction (aeb006863). This branch already had EclipseFoundation.Mosquitto.

Verified against a live device with mosquitto_sub and winget both hidden: exit 0, no stacktrace, 'MQTT capture disabled' in the summary and script.error.log, populated telnet section.
<!-- SECTION:FINAL_SUMMARY:END -->
