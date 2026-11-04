---
id: TASK-891.8
title: >-
  SAT: split heating-source (boiler/heatpump) and heating-system
  (radiators/underfloor) into separate enums
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 20:24'
updated_date: '2026-06-21 06:48'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATtypes.h
  - src/OTGW-firmware/SATcontrol.ino
parent_task_id: TASK-891
priority: high
ordinal: 115000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert directive (#dev-sat-mqtt 2026-06-20): the SATHeatingSystem enum conflates two orthogonal axes; separate them into separate enumerated classes. (1) Heating source = gas boiler | heat pump (energy source). (2) Heating system = radiators/mixed | underfloor (distribution). Structural foundation for the heat-pump MM=100% rule and the per-system COLD_SETPOINT in 891.2. Be precise with terminology (George: 'Device Type'; Robert: 'Heating source' - confirm before naming the enum).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Introduce TWO separate enums replacing the conflated SATHeatingSystem: a heating-source enum {gas boiler, heat pump} and a heating-system enum {radiators/mixed, underfloor}. Final enum naming per Robert (heating source vs device type - pending confirm).
- [x] #2 Migrate settings.sat to carry both axes (settings + UI + REST/MQTT) with backward-compatible mapping from the old SATHeatingSystem values (heat_pump value splits out).
- [ ] #3 Auto-detect heating source from ID3:LB2 (cooling-enabled bit set => heat pump; 0 => gas boiler) per George; PENDING Robert's Elga Ace validation; manual override retained.
- [x] #4 Heat-pump source: max relative modulation forced to 100% (never 0%), incl. in PWM suppression (supersedes the heat-pump MM AC noted in 891.2).
- [x] #5 Heating system drives the base offset (radiators vs underfloor) and the per-system COLD_SETPOINT (coordinate with 891.2).
- [x] #6 python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Terminology resolved (2026-06-20 20:33): George OK'd either 'heating source' or 'device type'; using Robert's term -> enum SATHeatingSource {gas_boiler, heat_pump}. NEW exploratory direction (George+Robert 20:35-20:40): HYBRID systems = heat pump + separate gas boiler monitored by TWO OTGWs; idea of a slave->master OTGW notify (possibly via the gas boiler's HA climate hvac_action) so the primary knows the boiler is firing. No decision yet -> 891.8 auto-detect (ID3:LB2) stays OPEN pending the hybrid approach; the single-device enum split + per-OTGW detect can still proceed when scheduled.

Robert 2026-06-20: HYBRID is a device type too - add a third value. Device-type/heating-source enum = SATHeatingSource {gas_boiler, heat_pump, hybrid}. Full hybrid coordination (slave->master OTGW notify) remains a separate larger topic; the enum just gains the hybrid value.

Robert 2026-06-20: hybrid scope = KISS for now -> just the manual device-type enum value {gas_boiler, heat_pump, hybrid} + manual selection. The bigger slave->master OTGW coordination is SPLIT OUT to its own task (see the new 'SAT hybrid systems' task). Hybrid MM behavior is UNDECIDED (Robert: 'I do not know about MM and hybrid') -> for now hybrid does NOT force MM=100% (only heat_pump does); revisit when decided.

IMPLEMENTATION DESIGN (mapped 42 enum sites; core getters SATcontrol.ino:211-275; auto-detect OTGW-Core.ino:2787; satAlwaysMaxModulation already exists at :259):
- New enums: SATHeatingSource {AUTO, GAS_BOILER, HEAT_PUMP, HYBRID}. Keep SATHeatingSystem values UNCHANGED {AUTO=0,RADIATORS=1,HEAT_PUMP=2(deprecated),UNDERFLOOR=3} - NO renumber, to protect persisted config.
- New settings field iHeatingSource (default AUTO). One-time migration on load: if iHeatingSystem==HEAT_PUMP -> iHeatingSource=HEAT_PUMP, iHeatingSystem=RADIATORS.
- SYSTEM axis (radiators/underfloor) drives: satGetBaseOffset + COLD_SETPOINT (28.2/21). Per George 'base offset system-only' -> drop heat-pump base-offset; a heat pump uses its system's offset.
- SOURCE axis drives: satAlwaysMaxModulation (heat_pump=>MM100, already at :259).
- Auto-detect: existing OTGW-Core:2787 derives heat-pump from MsgID3 HB bit0; George proposed ID3:LB2 (cooling bit) - DIFFERENT bit. Keep MANUAL for KISS; reconcile the detect bit on Robert's Elga validation.
- max-setpoint when source-cap (HP 40) vs system-cap (underfloor 45) differ -> use min().
- UI: sat.js/index.html add a Heating Source selector beside Heating System; restAPI exposes iHeatingSource.
- Hybrid: manual value, no MM=100% (TBD).
OPEN DESIGN QUESTION (needs George/Robert before build): the existing heat-pump min-on=1800s (:251), cycles/hr=2 (:240), max-setpoint=40 (:214). George said source affects ONLY MM. Keep these as SOURCE behaviors (recommended - heat pumps genuinely need long min-on + few cycles) or strip so source==MM only? This decides the bucketing.

DECISION (George+Robert 2026-06-20 21:26-21:31): Answer = A. Keep heat-pump timing (min-on, cycles/hr, max-setpoint 40C) as HEATING-SOURCE behaviors + force MM=100% for heat pumps (re-key the existing satAlwaysMaxModulation onto the source axis). Base-offset stays heating-system-only. PLUS a NEW user setting (user decides): heat-pump CYCLE RATE with 2 options -> 2 cycles/hour (1800s) [DEFAULT] or 1.5 cycles/hour (2400s). Implement this as part of 891.8: a heat-pump-cycle setting that drives the heat-pump cycle period (1800/2400s); note cph can be non-integer (1.5) so drive the PWM period in seconds, not an integer cph, for heat pumps.

George 2026-06-20 21:32: gas boiler also has cycle options (3 and 4 cycles/hour). These are the EXISTING settings.sat.iCyclesPerHour (default 3, range 2-6) - already wired in satGetMaxCyclesPerHour for the non-heat-pump path; no new field needed. Heat-pump path uses the new iHpCycleSeconds (1800=2/hr default, 2400=1.5/hr) via min-on. So: source=heat_pump -> iHpCycleSeconds; source=gas/hybrid -> iCyclesPerHour.

Robert 2026-06-20: SAT has NO legacy field configs -> REMOVED the migration block and the deprecated SAT_HSYS_HEAT_PUMP enum value. Clean SATHeatingSystem {auto=0, radiators=1, underfloor=2}. State field iDetectedHeatingSystem renamed -> iDetectedHeatingSource (source axis; OTGW-Core MsgID3 detect now feeds source). No migration code. UI sat.js shows system + source separately.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Split SATHeatingSource {auto,gas_boiler,heat_pump,hybrid} from SATHeatingSystem {auto,radiators,underfloor}. Source drives min-on/cycles/max-setpoint + MM=100% (heat pump, decision A); system drives base offset + COLD_SETPOINT. New settings iHeatingSource + iHpCycleSeconds (1800s=2/hr default, 2400s=1.5/hr); gas uses existing cycles-per-hour (3/4). No legacy migration (SAT is new) - removed SAT_HSYS_HEAT_PUMP. Auto-detect feeds the source axis (iDetectedHeatingSource). Fixed 2 latent radiators<->underfloor polarity bugs (SATpid Kp divisor, sat.js base-offset). REST/UI expose both axes; hybrid is a manual device type (slave->master coordination = separate task). alpha.231, esp32 SUCCESS (fw+fs), evaluate 0-fail, pushed a9047177. AC#3 left open: exact auto-detect bit (ID3:LB2 vs the current HB bit0) pending Robert's Elga Ace validation.
<!-- SECTION:FINAL_SUMMARY:END -->
