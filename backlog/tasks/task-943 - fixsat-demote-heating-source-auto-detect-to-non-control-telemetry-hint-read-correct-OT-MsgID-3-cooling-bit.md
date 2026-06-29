---
id: TASK-943
title: >-
  fix(sat): demote heating-source auto-detect to non-control telemetry hint;
  read correct OT MsgID 3 cooling bit
status: Done
assignee:
  - '@claude'
created_date: '2026-06-28 12:47'
updated_date: '2026-06-29 04:43'
labels: []
dependencies: []
ordinal: 156000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Beta-readiness review found the SAT heating-source auto-detect (state.sat.iDetectedHeatingSource) is (a) mis-wired — set inside print_vh_configmemberid (OTGW-Core.ino:2829), which decodes OT MsgID 74 ventilation config, keyed on ventilation bit0, NOT the intended MsgID 3 cooling bit; and (b) CONTROL-AFFECTING — satGetEffectiveHeatingSource() (SATcontrol.ino:204) resolves AUTO (the default) to iDetectedHeatingSource, which feeds boiler-command limits (maxSetpoint 40C cap, HP cycle, base offset). OpenTherm spec v4.2 has no device-class field; the only proxy is MsgID 3 HB bit2 'cooling supported', which is lossy (heating-only heat pumps do not set it; hybrid is invisible on one OT bus). Maintainer steer: source selection should be manual configuration, not unreliable auto-detection. Fix: make manual satsource authoritative for control (AUTO resolves to a safe GAS_BOILER default), move the detect read to the correct MsgID 3 HB bit2 (0x0400) but surface it only as a non-control telemetry hint (heating_source_detected). Spec evidence: OpenTherm-Protocol-Specification-v4.2.md:1606.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satGetEffectiveHeatingSource() AUTO branch returns SAT_SRC_GAS_BOILER (manual selection remains authoritative; iDetectedHeatingSource no longer drives control)
- [x] #2 Detection read removed from print_vh_configmemberid (MsgID 74) and added to the MsgID 3 slave-config handler, reading HB bit2 (& 0x0400 = cooling supported)
- [x] #3 iDetectedHeatingSource documented as a non-control telemetry hint; comments at SATtypes.h:214 and the moved code corrected to match
- [x] #4 Build green for esp32 target (per-env SUCCESS line verified, not just exit 0); evaluate.py --quick clean
- [x] #5 Field note: detected hint vs manual selection behaves sensibly on a real device (hardware-gated, deferred to field)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Build green: esp32 SUCCESS (clean solo rebuild, firmware 6:40 + filesystem; build/OTGW-firmware-esp32-otgw32-2.0.0-alpha.282 artifacts). evaluate.py --quick: 0 failures, health 98.7%. Field AC#5 deferred (hardware).

ON-DEVICE VALIDATED 2026-06-29 (OTGW32 @192.168.88.39, alpha.285): AC#5 PASS. Baseline heating_source=0 (manual=auto), heating_source_detected=1 (auto-detected gas_boiler hint). POST {name:satsource,value:2} -> 200; GET sat/status -> heating_source=2 (manual now heat_pump, AUTHORITATIVE for control) while heating_source_detected stayed 1 (UNCHANGED — the auto-detect runs independently and never overrides the manual control value). Restore satsource=0 -> heating_source back to 0, detected still 1. Confirms ADR-157: manual heating-source is authoritative, auto-detect is a non-control telemetry hint. All ACs satisfied.
<!-- SECTION:NOTES:END -->
