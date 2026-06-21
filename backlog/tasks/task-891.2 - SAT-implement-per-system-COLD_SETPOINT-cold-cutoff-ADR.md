---
id: TASK-891.2
title: 'SAT: implement per-system COLD_SETPOINT cold cutoff (+ADR)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 19:20'
updated_date: '2026-06-21 07:22'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/SATtypes.h
parent_task_id: TASK-891
priority: high
ordinal: 109000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George (SAT author) ruled COLD_SETPOINT a critical SAT function; Python value 22 was his bug. Implement the cold cutoff (firmware deliberately omitted it for the gateway role, SATcontrol.ino:1927; Robert approved implementing 2026-06-20). Python heating_control.py gates heater on/off (:72), PWM-setpoint skip (:385), enable-guard (:280), disable-guard (:317) on it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 When requested setpoint < COLD_SETPOINT: command boiler OFF as CH=0, CS=10 (SAT_MIN_SETPOINT), MM=100 (restore initial) per George 2026-06-20
- [x] #2 Gate the PWM enable/disable guards and the PWM/continuous setpoint computation on COLD_SETPOINT (mirror Python heating_control.py:72/280/317/385)
- [x] #3 ADR (via adr-kit) documenting the gateway-behaviour change: OTGW now commands the boiler OFF on low demand; record the per-system values and that it supersedes the deliberate omission
- [x] #4 python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
- [x] #5 COLD_SETPOINT is a per-HEATING-SYSTEM constant (George 2026-06-20): radiators 28.2, underfloor 21. It is NOT a gas/heat-pump axis. Selected by the radiators/underfloor heating-system setting.
- [x] #6 Heat-pump device type (George 2026-06-20): max modulation must ALWAYS stay 100%, never 0% - gate off the suppression MM=0% drop when heat-pump is selected.
- [x] #7 Base offset (satGetBaseOffset) must depend ONLY on heating system (radiators/underfloor) per George ('small mistake since the beginning'); verify SAT_HC_BASE_OFFSET_RAD/_FLOOR selection and that the gas/heat-pump distinction does not alter it.
- [x] #8 Flame-off PWM hold lower clamp uses COLD_SETPOINT (not SAT_MIN_SETPOINT): clamp(return+offset, COLD_SETPOINT, maximum_setpoint) per Python heating_control.py:397-398; deferred from TASK-891.1 (SATcontrol.ino:799-801)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
George clarified (#dev-sat-mqtt 2026-06-20 19:15-19:31) two orthogonal axes that the SATHeatingSystem enum currently conflates: HEATING SYSTEM (radiators|underfloor -> drives base offset + COLD_SETPOINT) vs DEVICE TYPE (gas|heat pump -> drives max-modulation management only; heat pump MM always 100%). heat_pump should not be an enum peer of radiators/underfloor. The axis-separation refactor may warrant its own task - pending Robert.

George design proposal (#dev-sat-mqtt 2026-06-20 19:48): Device Type axis = gas boiler | heat pump, auto-detected via ID3:LB2 (bit 1 = cooling enabled = heat pump; 0 = gas boiler) - pending Robert's validation on his Elga Ace. Heating System axis = radiators-or-mixed | underfloor. The full axis-split refactor (enum + ID3:LB2 auto-detect) is pending Robert's go and may become its own task; the heat-pump MM=100% gate is already captured in the ACs above.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Per-heating-system COLD_SETPOINT (radiators 28.2 / underfloor 21, satGetColdSetpoint). requested < COLD => boiler OFF (CH=0, CS=10, MM=100); auto-switch guards skipped below COLD; flame-off PWM hold clamped to [COLD,max] (deferred from 891.1). Heat-pump MM=100% + base-offset-system-only landed in 891.8. ADR-150 (Proposed) documents the gateway-behaviour change + reconciles the satDisable() comment. alpha.232, esp32 SUCCESS (fw+fs), evaluate 0-fail; pushed 73295292 (code) + 616e2c33 (ADR). Field-validated via the 2.0.0 alpha line.
<!-- SECTION:FINAL_SUMMARY:END -->
