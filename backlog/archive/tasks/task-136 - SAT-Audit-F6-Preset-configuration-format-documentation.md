---
id: TASK-136
title: 'SAT Audit F6: Preset configuration format documentation'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:57'
updated_date: '2026-04-09 05:26'
labels:
  - audit
  - sat
  - phase-6
  - docs
milestone: m-0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Document the SAT preset configuration format: how presets are defined, stored and applied in the firmware. Compare with Python SAT thermo-nova preset format for compatibility.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Preset data structure documented (all fields and types)
- [x] #2 Storage format (LittleFS JSON) documented
- [x] #3 Compatibility with Python SAT preset format verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for format differences
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created backlog/docs/sat-preset-configuration.md. Presets are 6 fixed named temperatures (comfort/eco/away/sleep/home/activity) stored as individual float fields in SATSection, serialized to settings.ini as JSON. No array structure, no custom names. Active preset not persisted across reboots. Compared with Python SAT: same names, same semantics, but configuration method differs. Activity preset doubles as window-open setpoint. Preset sync feature allows broadcasting to secondary MQTT topic.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created backlog/docs/sat-preset-configuration.md documenting the preset system. Key findings: presets are 6 independent float fields (not an array), stored in the main settings.ini JSON blob (keys SATpresetcomfort etc.). Active preset (state.sat.eActivePreset) is not persisted across reboots. Python SAT compatibility: same 6 names, same temperature semantics, but configuration uses MQTT/REST instead of configuration.yaml. Window-open detection uses the activity preset as the window-open setpoint. No gaps found that warrant audit-fix tasks — the preset format is simple and correct. One notable design choice: no way to define more than 6 presets or rename them.
<!-- SECTION:FINAL_SUMMARY:END -->
