---
id: TASK-1346
title: >-
  OT-Direct: SAT's last CH=0 must not stay in the gateway's MsgID 0 after a SAT
  disable or trip
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T10:11:00Z'
updated_date: '2026-10-24T13:03:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 469000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #3: OTDirect.ino:2755-2760 / 756-764 keep SAT's CH=0; afterwards the board's own curve and the thermostat-timeout setback run with CH disabled, the house cools silently.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After disable/trip on OT-Direct the gateway MsgID 0 CH bit returns to the thermostat's / default value (host or bench test red on HEAD, green)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause: OT-Direct applies CH= to MsgID 0 bit0 unconditionally (OTDirect.ino ~2996-3004) and keeps it after CS=0 (comment at ~2755 'CH= is the sole owner of that bit'). The PIC applies the CH= override only while a CS override is active: gateway.asm:53 ';CH=1 Override the CH enable bit when CS != 0', MessageID0 ~2702-2722 (manipulate bit0 only with SysCtrlSetpoint); second source pyotgw/pyotgw.py:602-603 'Control the CH enable status bit when overriding the control setpoint. By default the CH enable bit is set after a call to set_control_setpoint() with a value other than 0'. So after SAT's satDisable() (CS=0) a PIC board hands bit0 back, OT-Direct keeps SAT's CH=0.
Fix (OT-Direct only, PIC-faithful): separate flag otChModeOff (set by CH=, persists like PIC CHModeOff); otMasterStatusFlags bit0 stays the board's own CH (boot 1, hysteresis, setback). Effective bit0 in buildStatusRequest = CS override active ? !otChModeOff : (otMasterStatusFlags & 1). CS override active = MsgID 1 write override present (and not expired by the TASK-442 timer). Any reader that reports bit0 uses the same effective value. Gateway mode with a thermostat (TASK-1238) out of scope; note it.
Repro (HEAD alpha.488 on OTGW32 bench, master mode, SAT off): CS=40, CH=0 -> MsgID 0 = 0; CS=0 -> MsgID 0 stays 0 (PIC semantics: 256). Verify on fix: same sequence ends with 256; CS=40 alone -> 256; CS=40, CH=0 -> 0. Host test on the extracted effective-flag function + source checks. Not SAT control behaviour (gateway semantics); name to Sergeant D only as a notice.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence. HEAD repro (bench OTGW32 master mode, alpha.488+92036ef, SAT off): CS=40, CH=0 -> MsgID0 0; CS=0 -> stays 0 (CH bit stuck). Fix part 1 (OTDirect.ino, PIC-faithful): CH= sets otChModeOff, applied to bit0 only while a MsgID 1 write override is active (otdEffectiveMasterFlags in buildStatusRequest). Bench showed that in master mode the board's own heating curve re-arms MsgID 1 at once after CS=0 (TSet 45), so the flag alone keeps CH off - same as a PIC with the ESP curve. Fix part 2 (SATcontrol.ino satDisable): send CH=1 after CS=0 to restore the flag's default. Bench on fix (alpha.488+8110167 dirty build, OTA): SAT on, external CH=0 -> MsgID0 0; SAT off, then CS=40 -> 256 (CH handed back). tests/test_otd_ch_override.py 7/7 OK on fix; HEAD OTDirect FAILED 6, HEAD SATcontrol fails test_sat_disable_hands_back_ch. Other SAT tests OK, evaluate --quick 0 failed, build.bat SUCCESS 14:56.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT's last CH=0 no longer sticks after SAT stops. satDisable() now sends CH=1 after CS=0, restoring the CH override flag that outlives CS=0 on both board types. OT-Direct now applies CH= the PIC way: only while a control-setpoint override is active (gateway.asm:53; pyotgw.py:602-603), via otChModeOff + otdEffectiveMasterFlags() in buildStatusRequest.
Evidence: bench old (CS=0 leaves MsgID0 at 0) vs fix (after SAT off a new CS gives MsgID0 256); host test 7/7 on fix, failures on HEAD for both parts; build SUCCESS; evaluator 0 failed.
Not verified: PIC board (none connected; the CH=1 hand-back is harmless there because the PIC applies CH= only with a CS override); OT-Direct gateway mode with a thermostat (TASK-1238, out of scope).
<!-- SECTION:FINAL_SUMMARY:END -->
