---
id: TASK-780
title: Consolidate MQTT/telnet capture scripts into one low-friction Windows .bat
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 13:09'
updated_date: '2026-05-31 13:11'
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
- [ ] #1 Single .bat is the only file a tester needs to run (no required sibling .py/.ps1); double-click or CLI both work
- [ ] #2 Captures MQTT broker traffic to a timestamped log (subscribes to a configurable topic filter, default # )
- [ ] #3 Captures OTGW telnet debug (port 23) with auto-reconnect on drop, interleaved/parallel to the MQTT log
- [ ] #4 Prompts for broker + OTGW host with sane defaults when no args given; accepts args for non-interactive use
- [ ] #5 Bundles all logs into a single timestamped .zip for sharing
- [ ] #6 Clear failure messages when a required runtime is missing; Ctrl+C / timed stop both end cleanly and still produce the zip
- [ ] #7 Companion Python script (stdlib-only, no paho/pip) the maintainer can run directly; raw-socket MQTT subscribe so there is no third-party dependency
- [ ] #8 Both tools log MQTT subscribe-view + timestamped OTGW telnet debug to the SAME capture so the symptom (message gap) is time-correlated with the firmware side (heap dip, reconnect)
- [ ] #9 README documents that the malformed-packet bytes live in the broker log + HA MQTT debug log (a subscriber cannot see them) and instructs the tester to collect those too
- [ ] #10 Redundant scripts removed (launch_capture.bat, mqtt_telnet_capture.ps1, paho capture_mqtt_debug.py); README_capture.md points to the .bat + .py pair
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Write scripts/otgw_capture.py: stdlib-only raw-socket MQTT subscribe + telnet(:23) auto-reconnect + zip; logs gaps/timeline; no paho.
2. Write scripts/otgw_capture.bat: self-contained, extracts embedded PowerShell to %TEMP%, runs -ExecutionPolicy Bypass; raw .NET TcpClient MQTT subscribe + telnet + Compress-Archive; zero deps.
3. Rewrite README_capture.md: one .bat (testers) + one .py (maintainer); broker-log + HA-debug-log guidance for the actual malformed bytes.
4. Remove launch_capture.bat, mqtt_telnet_capture.ps1, capture_mqtt_debug.py.
5. Syntax-check py (python -m py_compile) and PS (extract block, powershell -NoProfile parse).
<!-- SECTION:PLAN:END -->
