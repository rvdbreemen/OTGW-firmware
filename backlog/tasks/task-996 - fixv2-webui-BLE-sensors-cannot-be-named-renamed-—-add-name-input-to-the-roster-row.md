---
id: TASK-996
title: >-
  fix(v2-webui): BLE sensors cannot be named/renamed — add name input to the
  roster row
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-03 22:32'
updated_date: '2026-07-03 22:41'
labels: []
dependencies: []
ordinal: 208000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The v2 BLE roster (renderBleCard) exposes Select/Forget/bindkey but NO name field, so sensors stay '(unnamed)' and cannot be renamed. Firmware already supports it: POST /api/v2/sat/ble/label {mac,label} -> satBLERosterSetLabel, persisted sBleLabel[], emitted as 'label' in discovery JSON. Add a per-row name input (pre-filled with label, Enter/Save applies, empty clears).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each roster row has a name input pre-filled with the current label
- [x] #2 Save/Enter POSTs /sat/ble/label and the name persists across reloads
- [x] #3 Clearing the field and saving clears the label
- [x] #4 Verified on-device with evidence
<!-- AC:END -->
