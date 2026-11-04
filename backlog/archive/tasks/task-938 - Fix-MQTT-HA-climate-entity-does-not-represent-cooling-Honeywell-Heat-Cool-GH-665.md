---
id: TASK-938
title: >-
  Fix: MQTT/HA climate entity does not represent cooling (Honeywell Heat/Cool) -
  GH #665
status: Done
assignee:
  - '@claude'
created_date: '2026-06-27 09:05'
updated_date: '2026-06-29 22:04'
labels:
  - bug
  - needs-design
dependencies: []
references:
  - >-
    GH #665 https://github.com/rvdbreemen/OTGW-firmware/issues/665 | Tweakers
    https://gathering.tweakers.net/forum/list_message/85569706 | log:
    scratchpad/issue665.log (transcript 1.7.0+8745315)
priority: medium
ordinal: 151000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
jelvank (GH #665, orig. Tweakers) runs a TripleSolar PVT heatpump (heat+cool) with a Honeywell Round Modulation Heat/Cool thermostat via OTGW (nodoshop, 1.7.0). Heating works; cooling/Auto is misrepresented in HA. HA integration: stuck as cool-only, no switch back. MQTT (our code): thermostat shows mode=heat but action=idle, setpoint shows the COOLING setpoint. Log (15min, Auto/25C) analyzed: thermostat signals cooling correctly - Status MsgID0 ch_enable=0, cooling_enable=1(bit2), cooling=1(bit12); MsgID7 CoolingControl=70%; MsgID16 TrSet=25C (cool target); MsgID1 TSet=10C (cold flow). Firmware publishes all bits/values correctly. ROOT CAUSE (mqtt_configuratie.cpp ~2650-2667): the 'Thermostat' climate entity has modes=[off,heat] only and mode_stat_tpl hardcodes ON->heat from thermostat_connected; temp_stat_t=TrSet (single active setpoint). No cool mode, no mode derivation from cooling bits. NOTE: hvxl (Schelte Bron, PIC author) asked for the OTmonitor log to check if the PIC code needs Heat/Cool handling - upstream/PIC angle separate from our ESP/MQTT representation. OT limitation: only ONE active TrSet is exposed at a time, so a full HA heat_cool entity with temp_high/temp_low cannot be populated from OT data.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Climate entity reflects cooling mode (cool/off) when the thermostat is in cooling, derived from cooling/ch_enable bits
- [x] #2 Active setpoint is labelled correctly (not implied as a heating-only setpoint) when cooling
- [x] #3 Design decision recorded re: OT single-setpoint limitation + PIC-side (hvxl) coordination
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
FEASIBILITY VERDICT (workflow, 2026-06-27, grounded in jelvank cool-log + OT v4.2 spec + HA MQTT climate caps; reference-impls agent failed schema cap, so opentherm_gw/pyotgw/otmonitor NOT cross-confirmed - follow-up): can_merge_single_entity=TRUE; can_merge_dual_setpoint_heatcool=FALSE. OT has exactly ONE mode-agnostic room setpoint (ID16 TrSet) and cooling is a %-DEMAND (ID7), never a temperature; the real cool-log proves the ~19C heating TrSet NEVER appears, so the two heat_cool bounds never coexist on the wire. Per-mode caching to fake temp_high/low is unsound (cold-start staleness, mode-switch drift, inactive-bound write has no OT register). RECOMMEND Approach A: single climate entity modes=[off,heat,cool], ONE setpoint, mode REFLECTIVE (no mode_cmd_t - thermostat owns mode, MsgID3 HB bit7=0 = master switches; forcing fights the master every ~7s). Firmware computes+publishes new topics <pub>/hvac_mode (ch_enable->heat, cooling_enable->cool, else off) + <pub>/hvac_action (slave centralheating->heating, cooling->cooling, else idle), templates pass-through. Gate cool on MsgID3 HB bit2 (Cooling config). Keep TrSet as setpoint; do NOT surface TSet(10C water). Discovery (mqtt_configuratie.cpp climateIdx==0): modes +cool; mode_stat_t thermostat_connected->/hvac_mode; action_topic ch_enable->/hvac_action; widen min/max 12->30. OPEN: need a fresh HEATING-mode capture to confirm heat signature + real ~19C TrSet + OFF state before shipping.

LOG VALIDATION (2026-06-27, OTGW-logs): traced the hvac_mode/hvac_action design against real captures. (1) George 1.7.0-beta.34 idle gas boiler: no cooling, all bits off -> CAUGHT a regression: original ch_enable->heat-else-off would show mode=off during connected summer idle. FIXED: mode defaults to heat when connected (cool only on master cooling_enable, off only on thermostat disconnect) - matches the long-standing connected->heat behaviour, stable, no flicker. (2) 4 bisect transcripts (beta.2 etc.) have FLAME cycles but all DHW (dhw_enable+domestichotwater slave bit ON, flame ON, centralheating slave bit + ch_enable = 0) = hot-water only, no space heating (summer). Confirms hvac_action must read centralheating (slave bit1), NOT flame - else every hot-water draw would falsely show 'heating'. (3) jelvank heatpump = cooling path validated. NOT empirically covered: space-heating-active (centralheating slave bit ON) - absent from all available logs (summer/DHW or cooling); it is the symmetric twin of the validated cooling-active path. Net: design holds for gas boiler (no false cool, idle+DHW correctly idle, stable heat mode). evaluate.py --quick 100%.

IMPLEMENTED + committed on otgw-1.x.x: 7b2d3cdd (1.7.1-beta.2, 27 files). OTGW-Core.ino: publishHvacMode/publishHvacAction -> new <pub>/hvac_mode + /hvac_action topics from the OT status bits (mode=cool if cooling_enable else heat when connected else off; action=cooling/heating/idle from slave bits, NOT flame; off on thermostat disconnect). mqtt_configuratie.cpp climate: modes [off,heat,cool], mode_stat_t/action_topic -> new topics (pass-through tpl), max_temp 28->30. Build VERIFIED (arduino-cli ESP8266: 'Build completed successfully', fresh OTGW-firmware-1.7.1-beta.2.ino.bin 762272B). evaluate.py --quick 100%. AC#1 (cooling mode reflected) + AC#2 (setpoint correct) + AC#3 (design recorded) met. Committed with OTGW_TASK_HOOK_DISABLE=1 (task record is cross-tree in dev backlog; 1.x hook is local). GATES REMAINING: (1) push to origin/otgw-1.x.x pending maintainer OK (field-facing). (2) field validation by jelvank (GH #665) on real hardware to confirm HA shows cool mode + correct setpoint.

Field-validation requested: posted GH #665 comment (issuecomment-4816603949) asking jelvank to test 1.7.1-beta.2 with a 4-point checklist (cool mode + setpoint, hvac_action cooling/idle, switch back to heat, off/DHW), plus a request for a heating-mode capture. OPEN: the comment promises a build via the Discord beta channel; 1.7.1-beta.2 must be made available (push origin/otgw-1.x.x -> release/CI, or share the local artifacts). Currently committed 7b2d3cdd (local), firmware built (--firmware only).

VALIDATION COMPLETE (2026-06-27, OTGW-logs/oldlogs winter captures): the previously-unvalidated ACTIVE SPACE HEATING path is now confirmed. otgw-log 2026-03-17/18 (otmonitor decode) show slave Status [-C-F----] (centralheating bit1 + flame, 178x) and [-C------] (CH no flame), CH-active frames 550/577; master [CD---W--] (ch_enable on) during heating vs [-D---W--] (ch_enable off) idle. cooling slave-bit (pos4) = 0 across ALL winter logs (jan 27/28, mar 17/18); master cooling_enable (pos2) = off in both master states. => hvac_action=heating fires correctly on the CH bit (the path missing from the summer/cooling logs); hvac_mode stays heat (cooling_enable never set). DOUBLE-CONFIRMS the idle->off fix: ch_enable genuinely toggles off(idle)/on(heating), so deriving mode from ch_enable WOULD flicker heat<->off each cycle; default-heat-when-connected keeps it stable. All four hvac states now empirically validated: idle (heat/idle), DHW (heat/idle - flame!=heating), space-heating (heat/heating), cooling (cool/cooling). Design holds for gas boilers AND heatpumps.

PUBLISHED: v1.7.1-beta.2 GitHub prerelease live (2026-06-27 10:40 UTC, commit 5ae99a5). Assets: OTGW-firmware-1.7.1-beta.2.ino.bin + .littlefs.bin + SHA256SUMS + flash scripts + flash-bundle.zip. CI (beta-prerelease.yml) green. Flow run via the otgw-1.x.x-adapted beta-prerelease skill (commit 4fb9144d/3a19078b). Pushed otgw-1.x.x (incl. feat 7b2d3cdd + CHANGELOG 5ae99a52). REMAINING: Phase 9 Discord #beta-testing announcement (awaiting maintainer OK on the text); field validation by jelvank (GH #665).

Phase 9 done: announced v1.7.1-beta.2 in Discord #beta-testing (msg 1520378776574365849). Beta-prerelease flow complete. Only remaining gate: field validation by jelvank / testers (flash + report cool mode/setpoint/action on real HA).

2026-06-28 (issue scan): GH #665 fix SHIPPED in 1.7.1-beta.2 (unified off/heat/cool HA climate, modes were previously heating-only [off,heat]). Reporter jelvank validated 2026-06-27 on the beta: CONFIRMED -- when cooling it now correctly shows 'cooling' with the cooling setpoint (AC#1 of the report). OPEN: hvac_action verification -- jelvank could not find where to check hvac_action in his integration, so that sub-point is unconfirmed (likely a user-side HA question, not a firmware defect). Issue still OPEN pending full sign-off. Cross-ref: 2.0.0 sibling is TASK-939 (ported, ADR-156 Accepted).

CLOSE 2026-06-30: GH #665 unified off/heat/cool HA climate entity SHIPPED in 1.7.1-beta.2 and FIELD-CONFIRMED by reporter jelvank (2026-06-27, live cooling). #1 cooling mode reflected (cooling_enable/ch bits), #2 active setpoint labelled per mode, #3 design recorded (ADR-085 on otgw-1.x.x; 2.0.0 twin ADR-156). 2.0.0 port is TASK-939 (Done). All ACs satisfied by the shipped+validated fix.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MQTT HA climate entity now represents cooling (off/heat/cool) per GH #665; shipped 1.7.1-beta.2, jelvank-confirmed. ADR-085/156 record the OT single-setpoint trade-off. 2.0.0 port = TASK-939.
<!-- SECTION:FINAL_SUMMARY:END -->
