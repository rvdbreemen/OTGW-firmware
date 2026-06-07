---
id: TASK-839
title: Quiet MQTT capture tool errors
status: Done
assignee:
  - '@codex'
created_date: '2026-06-07 10:06'
updated_date: '2026-06-07 10:22'
labels:
  - tooling
  - diagnostics
dependencies: []
references:
  - scripts/capture-mqtt-debug.bat
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Update the MQTT diagnostic capture script so noisy tool stderr is captured separately, folded into the final merged transcript, and the merged artifact filename identifies the capture context.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Tool stderr is captured to a separate error.txt file instead of being printed to the console during normal capture.
- [x] #2 The merged transcript includes the captured error log content so a single artifact contains the full diagnostic context.
- [x] #3 The final transcript filename is based on date-time, hostname, and the unique device ID.
- [x] #4 Existing clean-stop behavior remains available and validation covers the script path.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the embedded PowerShell capture flow for process stderr and transcript merge behavior.
2. Add a capture-wide error.txt log and redirect noisy tool stderr into it while keeping console status output readable.
3. Rename the final merged transcript using timestamp, device host, and unique device ID metadata when available.
4. Validate help/syntax and a short noninteractive capture path, then update acceptance criteria and notes.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev
Coding agent: @codex

Implemented in scripts/capture-mqtt-debug.bat:
- Added a retained error.txt artifact assembled from script errors, mosquitto stderr, and browser stderr.
- Redirected headless browser stderr to browser.stderr.log during capture so Chromium DevTools/USB/task-manager noise no longer reaches the console.
- Merged error.txt into the final transcript under TOOL ERRORS while keeping error.txt in the run folder.
- Renamed the merged transcript to <date-time>-<hostname>-<uniqueid>.txt using telnet metadata first, then REST/debug/settings/device-info fallbacks, then prompted host/unknown-id fallback.
- Recorded resolved hostname, MQTT unique_id, optional PIC device_id, metadata sources, and metadata warnings in summary.txt before merge.

Validation:
- PowerShell parser check on embedded payload: OK.
- cmd /c scripts\capture-mqtt-debug.bat --help: OK.
- Short runtime with fake mosquitto and browser disabled: produced D:\tmp\otgw-capture-test\20260607-121645\20260607-121645-testdevice-unknown-id.txt and retained error.txt; transcript included TOOL ERRORS.
- Short runtime with browser enabled against about:blank: console stayed quiet; Chromium "DevTools listening" was captured in error.txt and merged into the transcript.
- Synthetic telnet metadata test: hostname OTGWELGA, unique_id otgw-ABCDEF123456, PIC device_id PIC-42 parsed with zero REST warnings; filename resolved to 20260607-122000-OTGWELGA-otgw-ABCDEF123456.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated the MQTT diagnostic capture script so browser/tool stderr is captured into error.txt instead of polluting the console, and error.txt is merged into the final transcript. The merged transcript now uses a date-time-hostname-uniqueid filename, with telnet metadata preferred and REST/MAC fallbacks when telnet metadata is unavailable. Validated syntax, help output, short runtime finalization, browser stderr redirection, transcript merge, and synthetic metadata naming.
<!-- SECTION:FINAL_SUMMARY:END -->
