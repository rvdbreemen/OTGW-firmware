---
id: TASK-1728
title: >-
  Parked: OT-Direct gateway mode detects a missing or late thermostat at runtime
  (PIC model)
status: To Do
assignee: []
created_date: '2026-11-08T10:52:00Z'
updated_date: '2026-11-20T01:18:00Z'
labels:
  - otdirect
  - parked
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 836000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parked by Robert 2026-11-08 (YAGNI) until a field report needs it. From the TASK-1716 check by otgw-firmware-54: auto-detect runs once per boot (5 s window); a thermostat slower than that is treated as absent and in master mode never relayed until reboot; gateway standalone with auto-detect off sends its own MsgID 0 with CH on and no setpoint (TASK-1684 defect in gateway form). PIC: continuous detection, standalone frames while absent, re-attach (gateway.asm:1740-1772).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 milestone 2.0.0 -> 2.1.0 (otgw-docs, coordinator's assignment). Robert DM 2026-11-20 01:15 (1573144814600462457), relayed by the coordinator: "C — field/hardware ACs outside the 2.0.0 scope, to 2.1.0". Status, assignee and ACs unchanged.
<!-- SECTION:NOTES:END -->
