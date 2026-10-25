---
id: TASK-1373
title: 'SAT: user MaxTSet slider bounded by MsgID 57, sent to the boiler (entry 48)'
status: To Do
assignee: []
created_date: '2026-10-25T06:31:00Z'
labels:
  - sat
  - webui
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 508000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 05:56/06:10 UTC (#alpha-testing 1563793351961223221, 1563796961771913227): 'Create a MaxTset slider. The limit values should be the MsgID 57 values (Lower/Higher). Then pass this value to the boiler. If the boiler doesn't report the MsgID 57, then use as default lower/upper limit values the 20/80' and 'This development should be identical to the DHW Setpoint slider.' Answers Q15 (maximum flow setpoint: a user setting). Today the cap is fixed 62/45/40 (SATcontrol.ino ~293-327, audit #29); thermo-nova lets the user set maximum_setpoint (config_flow.py:612-613). Design mirrors the DHW setpoint slider (SW=, MsgID 48 bounds): setting, REST/MQTT/HA number entity, v2 slider, sent with SH= (PIC) / MsgID 57 write (OT-Direct); used as the SAT ceiling. Plan to Sergeant D as a notice (his own design).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A MaxTSet slider in the v2 SAT page bounded by MsgID 57 (fallback 20..80) sets the value, the gateway sends it to the boiler (SH= on PIC, MsgID 57 write on OT-Direct) and SAT uses it as its ceiling (host test + bench capture); HA number entity + MQTT documented
<!-- AC:END -->
