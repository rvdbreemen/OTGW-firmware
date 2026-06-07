---
id: TASK-840
title: Add transcript prefix and firmware version to MQTT capture filename
status: Done
assignee:
  - '@codex'
created_date: '2026-06-07 10:37'
updated_date: '2026-06-07 10:41'
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
Follow up the MQTT capture transcript naming so the final artifact is clearly labeled for users and includes firmware version metadata.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The final merged transcript filename starts with transcript.
- [x] #2 The final merged transcript filename includes the firmware version when discoverable.
- [x] #3 Filename generation falls back safely when firmware version metadata is unavailable.
- [x] #4 Validation covers syntax, help/runtime path, and metadata-derived filename formatting.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev
Coding agent: @codex

Implemented in scripts/capture-mqtt-debug.bat:
- Changed final transcript filename shape to transcript-<date-time>-<firmware-version>-<hostname>-<uniqueid>.txt.
- Added firmware version metadata discovery from telnet.log version:, /api/v2/debug build.version, and /api/v2/device/info fwversion.
- Added unknown-version fallback so the filename shape remains stable if metadata is unavailable.
- Added ResolvedFirmwareVersion to the merged summary metadata.

Validation:
- PowerShell parser check on embedded payload: OK.
- cmd /c scripts\capture-mqtt-debug.bat --help: OK and documents transcript-<date-time>-<firmware-version>-<hostname>-<uniqueid>.txt.
- Synthetic telnet metadata test: version 1.7.0-beta.1, hostname OTGWELGA, unique_id otgw-ABCDEF123456, PIC device_id PIC-42 produced transcript-20260607-124000-1.7.0-beta.1-OTGWELGA-otgw-ABCDEF123456.txt with zero metadata errors.
- Short runtime with fake mosquitto and no real device produced transcript-20260607-124018-unknown-version-testdevice-unknown-id.txt and retained error.txt; merged transcript included ResolvedFirmwareVersion and TOOL ERRORS.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated MQTT capture transcript naming so the user-facing artifact starts with transcript and includes firmware version metadata. Firmware version is read from telnet first, then REST debug/device-info fallbacks, with unknown-version fallback when unavailable. Validated syntax, help text, metadata formatting, runtime fallback naming, and transcript merge.
<!-- SECTION:FINAL_SUMMARY:END -->
