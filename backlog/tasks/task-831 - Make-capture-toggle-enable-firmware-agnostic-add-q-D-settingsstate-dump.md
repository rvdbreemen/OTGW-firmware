---
id: TASK-831
title: Make capture toggle-enable firmware-agnostic + add q/D settings+state dump
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 08:41'
updated_date: '2026-06-06 08:44'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field run (2026-06-06) showed dev capture-mqtt-debug.bat run against a 2.0.0/OTGW32 device: all debug toggles reported not-found because the hardcoded 1.x key table (4=MQTTGate,5=Sensors,6=NTP) does not match the 2.0.0 telnet menu (g=MQTTGate, n=NTP, 4=Sensors, 5=SAT, 6=OTDirect, 7=SATBLE). Replace the static table with dynamic parsing of the banner Debug-toggles block so it works on both 1.x and 2.0.0. Also extend the settings capture to send 'q' (force read settings) and 'D' (dump full debug info: settings + state) per 2.0.0 handleDebug; both stream into telnet.log and unknown keys are ignored on 1.x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Toggle-enable parses key/label/state triplets dynamically from the banner; flips every [0] non-simulator toggle by sending its key; works on both 1.x and 2.0.0 layouts
- [x] #2 Simulator toggles (SensorSim, OTGW-Sim) are never flipped
- [x] #3 Settings capture sends 'q' then 'D' and drains both into telnet.log
- [x] #4 PowerShell payload parses clean (0 AST errors); dynamic parse verified against both 1.x and 2.0.0 banner samples
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Made capture-mqtt-debug.bat firmware-agnostic and extended the settings capture.

Toggle-enable: replaced the static 1.x key table with dynamic parsing of the banner Debug-toggles block (regex over <key> <Label> [<0|1>] triplets). Flips every [0] non-simulator toggle by sending its key; simulators (SensorSim, OTGW-Sim) excluded by label. Works on 1.x (keys 1-6) and 2.0.0/OTGW32 (1-7 + g + n incl SAT/OTDirect/SATBLE).

Settings capture: new Send-TelnetKeyAndDrain helper; Request-SettingsDump now sends q (force read settings) then D (dump settings+state on 2.0.0; full INI dump on 1.x). Unknown keys are ignored by firmware, safe across families.

Verified: AST parse clean (1474 lines). Dynamic parser tested against both 1.x and 2.0.0 banner samples (correct send/skip per toggle); status-flag lines like [-D---W--] produce no false match. Script-only change.
<!-- SECTION:FINAL_SUMMARY:END -->
