---
id: TASK-1806
title: >-
  OT-Direct: remove the 16 C flow setback on a thermostat disconnect, like the
  PIC
status: To Do
assignee: []
created_date: '2026-11-11T06:43:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - feature
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 899000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report A, A8 (observation) and report CD, D9. Robert's decision (2026-11-11 ~07:45, AskUserQuestion in the coordinator session, relayed): 'Like the PIC'. Today, after iSetbackTimeout (default 30 s) without thermostat frames and with FS set, checkThermostatTimeout puts setOverride(1, fSetbackTemp = 16 C) as the flow setpoint TSet with CH forced on (OTDirect.ino:2669-2678, otdEffectiveMasterFlags :827-830; default OTDirecttypes.h:112). The 'setback temperature' is a room value used as a flow setpoint, and no ADR records it (ADR-064/066/068 checked by the reviewer). The PIC has no such setback: on thermostat loss it runs its own standalone schedule with TSet 0 unless CS or a heat demand (Disconnect gateway.asm:1772-1787, StandAloneMsg :1796-1816, MessageID1 :2796-2820); its FS= is the watchdog fail-safe (SetFailSafety :5252-5259) and SB= is the GPIO away room setpoint (:5390-5398). Intended result: on thermostat loss the gateway applies no control of its own and the boiler follows its own OpenTherm timeout behaviour. Check the settings (FS, fSetbackTemp, iSetbackTimeout) for what stays, what goes and how a stored value migrates.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On thermostat loss OT-Direct no longer writes a setback flow setpoint or forces CH on; the boiler follows its own OpenTherm timeout
- [ ] #2 Host test against the real checkThermostatTimeout path: red on the old code (16 C written), green on the fix
- [ ] #3 The manual (EN and NL) and the release notes describe the new behaviour and what happened to the setback settings
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->
