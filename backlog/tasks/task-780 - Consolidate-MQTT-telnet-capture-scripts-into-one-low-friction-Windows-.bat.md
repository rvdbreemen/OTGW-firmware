---
id: TASK-780
title: Consolidate MQTT/telnet capture scripts into one low-friction Windows .bat
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 13:09'
updated_date: '2026-05-31 13:23'
labels:
  - tooling
  - windows
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
scripts/ currently has four overlapping capture artifacts: capture_mqtt_debug.py (paho-based MQTT+telnet capture, zips output), launch_capture.bat (python finder + paho auto-install + runs the py), mqtt_telnet_capture.ps1 (PowerShell variant: mosquitto_sub-or-python MQTT + TcpClient telnet + Compress-Archive), and README_capture.md. A tester (GeorgeZ83) needs a single double-click artifact to capture broker + OTGW telnet logs for the MQTT desync investigation (TASK-769). Goal: one self-contained .bat that runs on stock Windows with minimal setup, captures MQTT broker traffic + OTGW telnet debug to timestamped logs, and bundles them into a zip for sharing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Single .bat is the only file a tester needs to run (no required sibling .py/.ps1); double-click or CLI both work
- [x] #2 Captures MQTT broker traffic to a timestamped log (subscribes to a configurable topic filter, default # )
- [x] #3 Captures OTGW telnet debug (port 23) with auto-reconnect on drop, interleaved/parallel to the MQTT log
- [x] #4 Prompts for broker + OTGW host with sane defaults when no args given; accepts args for non-interactive use
- [x] #5 Capture output goes to a timestamped run folder (telnet.log, mqtt.log, summary.txt) under logs/mqtt-diagnostics for sharing
- [x] #6 Clear message if PowerShell is missing; Ctrl+C and -DurationSeconds both stop cleanly and still write summary.txt
- [x] #7 Standalone capture-mqtt-debug.ps1 removed; the PowerShell worker is embedded in the .bat, extracted to %TEMP% at runtime, ExecutionPolicy bypassed for the child process only
- [x] #8 --help and /? print usage and exit 0 (verified end-to-end); the worker MQTT/telnet logic is unchanged from the prior script
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Write scripts/otgw_capture.py: stdlib-only raw-socket MQTT subscribe + telnet(:23) auto-reconnect + zip; logs gaps/timeline; no paho.
2. Write scripts/otgw_capture.bat: self-contained, extracts embedded PowerShell to %TEMP%, runs -ExecutionPolicy Bypass; raw .NET TcpClient MQTT subscribe + telnet + Compress-Archive; zero deps.
3. Rewrite README_capture.md: one .bat (testers) + one .py (maintainer); broker-log + HA-debug-log guidance for the actual malformed bytes.
4. Remove launch_capture.bat, mqtt_telnet_capture.ps1, capture_mqtt_debug.py.
5. Syntax-check py (python -m py_compile) and PS (extract block, powershell -NoProfile parse).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SCOPE CORRECTION: my first pass misread scripts/ and built an off-target stdlib Python tool (otgw_capture.py) + Python/paho/zip ACs. User clarified the real ask: the two existing files capture-mqtt-debug.bat (thin launcher) + capture-mqtt-debug.ps1 (840-line worker: mosquitto_sub + TcpClient telnet w/ adaptive reconnect + MQTT-debug auto-toggle + summary.txt) -> consolidate into ONE .bat, PowerShell underneath. Removed otgw_capture.py and reset the ACs.

DELIVERY: capture-mqtt-debug.bat is now self-contained. Header = launcher (locate pwsh/powershell, map --help and /? to -Help, extract embedded payload to %TEMP% via Select-String ^:::PSPAYLOAD::: marker, run -ExecutionPolicy Bypass -File, delete temp, propagate exit code). The entire .ps1 worker is embedded verbatim below the marker (no batch-escaping). Standalone capture-mqtt-debug.ps1 deleted. Assembled UTF-8 no-BOM, CRLF.

VERIFIED this session (Windows): @echo off first line (no BOM); single marker at line 47; cmd /c capture-mqtt-debug.bat --help -> EXIT 0, usage text emitted (863 chars), 0 leftover temp files; /? variant -> EXIT 0. Grep confirms zero stale .ps1 references.

NOT re-run live: an actual capture session (needs a broker + OTGW on the LAN) was not executed here. The MQTT/telnet capture logic is byte-identical to the prior working script (it had produced capture logs before), so AC#2-#6 capture behaviour is inherited-unchanged, only the packaging changed and the packaging is verified.

OUT OF SCOPE / flagged: src/OTGW-firmware/data/capture-mqtt-debug.ps1.txt is a 0-byte placeholder (not web-linked) - left untouched (LittleFS asset). scripts/README.md does not reference the capture scripts - nothing stale to fix.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Consolidate the OTGW MQTT/telnet capture tooling into a single low-friction Windows .bat with PowerShell doing the work underneath.

What changed:
- scripts/capture-mqtt-debug.bat is now self-contained: a small batch launcher (locate pwsh/powershell, forward args, map --help and /? to -Help) extracts the embedded PowerShell worker to a temp .ps1 and runs it with -ExecutionPolicy Bypass, then deletes the temp file and returns its exit code.
- The full PowerShell worker (mosquitto_sub MQTT capture + TcpClient telnet with adaptive reconnect + auto MQTT-debug toggle + summary.txt) is embedded verbatim after a ^:::PSPAYLOAD::: marker, so no escaping and no behaviour change.
- Standalone scripts/capture-mqtt-debug.ps1 deleted (folded into the .bat). The off-target otgw_capture.py from a misread first pass was removed.

Why: a tester only needs the one .bat (double-click or cmd); no separate .ps1, no execution-policy prompt.

Verified (Windows): no BOM, single marker, --help and /? exit 0 with usage text, temp file cleaned up, no stale .ps1 references. Live capture not re-run (no broker/OTGW in this env); worker logic is unchanged from the prior working script.

Follow-ups (not done, by design): no .zip bundling (worker writes a run folder); src/OTGW-firmware/data/capture-mqtt-debug.ps1.txt is an empty placeholder left as-is.
<!-- SECTION:FINAL_SUMMARY:END -->
