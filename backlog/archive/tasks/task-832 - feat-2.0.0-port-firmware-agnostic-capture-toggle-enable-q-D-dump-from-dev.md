---
id: TASK-832
title: 'feat-2.0.0: port firmware-agnostic capture toggle-enable + q/D dump from dev'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 08:49'
updated_date: '2026-06-20 11:10'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Forward-port the capture-mqtt-debug.bat improvements from dev (TASK-828/829/830/831) so the 2.0.0 worktree copy reaches parity: all-toggle-enable via dynamic banner parsing (works on 2.0.0 keys g/n + SAT/OTDirect/SATBLE), and settings capture sending q (read settings) + D (dump settings+state). 2.0.0 firmware already supports q (handleDebug readSettings) and D (dumpDebugInfo). dev script is a clean superset of the 2.0.0 script (browser-capture bytes identical; only the obsolete MQTT-only toggle fn differs), so the port is a whole-file copy.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 2.0.0 capture-mqtt-debug.bat has dynamic toggle-enable + Request-SettingsDump (q/D); browser-capture bytes unchanged
- [x] #2 PowerShell payload parses clean; both browser fns and toggle/dump fns present
- [x] #3 Field-verify on a real 2.0.0 device that D produces settings/state output in telnet.log (drain window sufficient under load)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Capture toggle + q/D dump ported and field-validated. AC#1/#2: committed a441ff6a. AC#3: LIVE telnet validation on the connected esp32-classic (2026-06-20, MQTT-connected = under load): 'D' produced '--- DUMP BEGIN ---' with [build]/[runtime]/[settings] sections (version/heap/settings state); 'q' triggered readSettings() with field-by-field updateSettings output. 46KB drained over 9s with the OT/MQTT log stream still flowing (no stall). All ACs met.
<!-- SECTION:FINAL_SUMMARY:END -->
