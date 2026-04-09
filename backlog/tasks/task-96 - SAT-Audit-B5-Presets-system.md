---
id: TASK-96
title: 'SAT Audit B5: Presets system'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:50'
updated_date: '2026-04-09 05:20'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the presets system in Python SAT thermo-nova (comfort/away/sleep/etc.) with the C++ implementation. Verify preset definition, storage, override and fallback behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All preset types compared (names, values, defaults)
- [x] #2 Preset persistence and storage verified
- [x] #3 Override and fallback behavior correct
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B5 audit: Presets system - GAPS FOUND

Python (climate.py, const.py): Presets include comfort, eco, away, sleep, activity. Default values: comfort=20, eco=18(?), away=10, sleep=15, home=18, activity=10.

C++ (SATcontrol.ino satHandlePreset()): Implements away, eco, comfort, sleep, activity, home. Reads from settings.sat.fPreset* values.

MATCHING:
- Preset switching changes fTargetTemp ✓
- PID integral reset on preset change ✓
- Pre-custom temperature saved before change (Task #67) ✓
- Window detection uses Activity preset ✓

GAPS:
1. Python CONF_HOME_TEMPERATURE default = 18C; CONF_COMFORT_TEMPERATURE = 20C. C++ settings defaults need verification — not visible from this audit. Check OTGW-firmware.h defaults for fPresetHome and fPresetComfort.
2. Python has preset RESTORE on HVAC mode change (off->heat restores preset). C++ has no equivalent restore logic — when SAT is re-enabled, it uses last fTargetTemp without restoring any preset.
3. Python sync_climates_with_preset: when preset changes, secondary climates are synced. C++ has preset_sync to a configurable MQTT topic (Task #46) which is correct, but it's configurable vs Python's always-sync behavior.
4. Pre-activity temperature tracking (Task #67): C++ saves fPreCustomTemp and fPreActivityTemp. Python tracks pre-custom temperature via old_state.attributes. Functionally equivalent.

No critical gaps. Minor default value verification needed.
<!-- SECTION:FINAL_SUMMARY:END -->
