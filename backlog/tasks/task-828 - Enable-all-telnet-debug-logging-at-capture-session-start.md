---
id: TASK-828
title: Enable all telnet debug logging at capture session start
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 05:50'
updated_date: '2026-06-06 05:52'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
capture-mqtt-debug.bat only toggles MQTT debug (key 3) on connect. For full diagnostics (heap/WS/REST/sensor investigations like George's TASK-779), the capture should enable ALL logging toggles (1 OTmsg, 2 REST API, 3 MQTT, 4 MQTTGate, 5 Sensors, 6 NTP) at session start, re-applying after each reconnect/reboot. Simulator toggles (SensorSim, OTGW-Sim) are intentionally excluded (they inject fake data).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On telnet connect, every OFF logging toggle (1-6) is flipped ON; toggles already ON are left untouched (no accidental flip-off)
- [x] #2 Simulator toggles (SensorSim, OTGW-Sim) are NOT touched
- [x] #3 Re-applied on every reconnect (post-reboot toggles reset to defaults)
- [x] #4 Banner reader captures the full toggle block before parsing (breaks on footer, not on the MQTT line)
- [x] #5 summary.txt records per-toggle action (sent/already-on/not-found)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add $script:DebugToggles table (keys 1-6 -> labels)\n2. Replace Enable-MqttTelnetDebugIfNeeded with Enable-AllTelnetDebugIfNeeded (parse each toggle state from banner, send key only when [0])\n3. Fix Read-InitialTelnetBanner to break on footer (Press 'h' / OTGW-Sim) so full toggle block is captured\n4. Update Connect-TelnetCapture call + summary line\n5. Update Show-Help text
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Extended capture-mqtt-debug.bat to enable ALL telnet logging toggles at session start instead of only MQTT (key 3).

Changes (PowerShell payload):
- New \$script:DebugToggles table mapping keys 1-6 to OTmsg/REST API/MQTT/MQTTGate/Sensors/NTP.
- Enable-MqttTelnetDebugIfNeeded replaced by Enable-AllTelnetDebugIfNeeded: parses each toggle state from the connect-time banner and sends the key only when [0], so toggles already ON are never flipped off. Simulator toggles (SensorSim, OTGW-Sim) are excluded by construction.
- Read-InitialTelnetBanner now breaks on the footer (Press 'h' / OTGW-Sim) instead of the MQTT line, so toggles 4-6 (printed after MQTT) are present before parsing.
- Connect-TelnetCapture summary line now reports per-toggle actions; runs on every (re)connect so post-reboot resets are re-applied.
- Show-Help documents the all-logging behaviour.

Verification: PowerShell AST parse clean (1011 lines, 0 errors). Functional test against George 1.7.0 banner: OTmsg/NTP (already [1]) skipped; REST API/MQTT/MQTTGate/Sensors ([0]) send keys 2/3/4/5; simulator line matched by no pattern. Script-only change (no firmware); build/eval gates N/A.
<!-- SECTION:FINAL_SUMMARY:END -->
