---
id: TASK-1373
title: 'SAT: user MaxTSet slider bounded by MsgID 57, sent to the boiler (entry 48)'
status: To Do
assignee: []
created_date: '2026-10-25T06:31:00Z'
updated_date: '2026-10-29T19:15:00Z'
labels:
  - sat
  - webui
  - mqtt
milestone: 2.0.0
dependencies:
  - TASK-1412.1
  - TASK-1412.3
references:
  - TASK-1412
priority: medium
ordinal: 508000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 05:56/06:10 UTC (#alpha-testing 1563793351961223221, 1563796961771913227): 'Create a MaxTset slider. The limit values should be the MsgID 57 values (Lower/Higher). Then pass this value to the boiler. If the boiler doesn't report the MsgID 57, then use as default lower/upper limit values the 20/80' and 'This development should be identical to the DHW Setpoint slider.' Answers Q15 (maximum flow setpoint: a user setting). Today the cap is fixed 62/45/40 (SATcontrol.ino ~293-327, audit #29); thermo-nova lets the user set maximum_setpoint (config_flow.py:612-613). Design mirrors the DHW setpoint slider (SW=, MsgID 48 bounds): setting, REST/MQTT/HA number entity, v2 slider, sent with SH= (PIC) / MsgID 57 write (OT-Direct); used as the SAT ceiling. Plan to Sergeant D as a notice (his own design).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A MaxTSet slider in the v2 SAT page bounded by the boiler's MsgID 49 MaxTSet upper/lower bounds (fallback 20..80 when MsgID 49 is not reported; entry 100, Q41 A) sets the value; the gateway always sends it to the boiler as MsgID 57 (SH= on PIC, MsgID 57 write on OT-Direct) with no MsgID 6 gate (entry 100), and SAT caps every CS it sends at that value, ignition hold included (entries 51, 54) (host test + bench capture); HA number entity + MQTT documented
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 second reader (wf_16ada525): MsgID 57 carries no bounds; the MaxTSet upper/lower bounds are MsgID 49 MaxTSetUBMaxTSetLB (OTGW-Core.h:423 vs :431). Entry 48 quotes 'MsgID 57 values (Lower/Higher)'; our reading: bounds from MsgID 49, value written as MsgID 57 (SH= on PIC). Confirm with Sergeant D when the plan is shared. Also: the ignition hold is clamped with satGetMaxSetpoint (SATcontrol.ino:1030-1032), not satEffectiveMaxSetpoint; thermo-nova clamps the hold with the user maximum_setpoint (heating_control.py:398). Entry 51: SAT caps every CS at the user value. gruijter answered 1563816425838219335 (asked for MsgID 57 OT log).

Entry 54 (2026-10-25 07:31): with the slider at 48 SAT never sends a CS above 48, ignition hold included (thermo-nova heating_control.py:397 clamps the flame-off hold at maximum_setpoint).

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Sergeant D Q15 (entry 67): defaults 62 radiators / 55 underfloor when the user did not set the slider. Conflict: firmware 45 underfloor system cap and 50 hard ceiling; Q15b asked. Q41 expansion asked (entry 85: MsgID 49 vs 57 bounds).

2026-10-28 Q41 A (entry 100, msg 1564918647338700851): bounds from MsgID 49 (20/80 fallback), always send SH= (MsgID 57), no MsgID 6 gate, SAT caps CS itself. Defaults 62/55 (entry 67) wait on Q15b (underfloor 50 C ceiling, heat-pump default).

2026-10-28 entry 110 (Q15b B): keep the 50 C underfloor ceiling; underfloor default max flow 50 C (was 55 in entry 67); radiators 62. Heat-pump default still open.

TASK-1412.1 records review (R29): 2026-10-29. TASK-1412.1 R29 reconciliation: AC#1 aligned with entry 100 (Q41 A). Bounds from MsgID 49 (OTGW-Core.h:423), value MsgID 57 (OTGW-Core.h:431, OTDirect.ino:143), 20/80 fallback, no MsgID 6 gate. The title keeps entry 48's original wording as history; entry 100 is the dated correction. Defaults: radiators 62 (entry 67), underfloor 50 (entry 110, changes entry 67). The heat-pump default and whether an unchosen default is written to the boiler are still OPEN (entry 110 'Open', not asked yet). Do not implement a heat-pump default by assumption. The 2026-10-25 citation SATcontrol.ino:1030-1032 (ignition hold) no longer matches the current source. Re-locate it under TASK-1412.13 AC#2. Not implemented: no user MaxTSet setting exists in the current source (SATtypes.h:533 is the global 65 ceiling; SATcontrol.ino:302-309 fixed caps).
<!-- SECTION:NOTES:END -->
