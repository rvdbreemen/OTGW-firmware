---
id: TASK-649
title: >-
  Audit HA capability-flag binary_sensors (MsgID 2/3/6) vs HA core opentherm_gw
  semantics — non-breaking fixes only
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 08:53'
updated_date: '2026-05-21 09:08'
labels:
  - audit
  - ha-discovery
  - mqtt
  - non-breaking
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Home Assistant core's `opentherm_gw` integration surfaces 11 capability-flag binary_sensors derived from OpenTherm MsgID 3 (slave configuration), MsgID 2 (master configuration), and MsgID 6 (RBP transfer/RW flags). Cross-check confirmed (2026-05-21 in-conversation research) that OTGW-firmware already publishes all 11 equivalent flags with HA discovery, via `mqttHaBinSensors[]` in `src/OTGW-firmware/mqtt_configuratie.cpp` lines 1124-1147.

This task does a focused audit of those 11+2 entries against HA core's semantics (`device_class`, `entity_category`, payload values, retain flag) and applies ONLY non-breaking small fixes. Topic-label renames are explicitly off-limits (would break existing automations and have been deliberated against in conversation).

The off-limits list is enforced by the AC list: any change touching `ha_lbl_*` strings, MsgID handlers, gating logic, or the ADR-070 source-separated layer is out of scope and must spawn a separate task.

Reference table (HA core → firmware label):
- DATA_SLAVE_DHW_PRESENT → dhw_present (MsgID 3 bit 0)
- DATA_SLAVE_CONTROL_TYPE → control_type_modulation (MsgID 3 bit 1)
- DATA_SLAVE_COOLING_SUPPORTED → cooling_config (MsgID 3 bit 2)
- DATA_SLAVE_DHW_CONFIG → dhw_config (MsgID 3 bit 3)
- DATA_SLAVE_MASTER_LOW_OFF_PUMP → master_low_off_pump_control_function (MsgID 3 bit 4)
- DATA_SLAVE_CH2_PRESENT → ch2_present (MsgID 3 bit 5)
- DATA_SLAVE_REMOTE_RESET → remote_water_filling_function (MsgID 3 bit 6 — note: pyotgw and OT spec use different names for the same bit)
- DATA_REMOTE_TRANSFER_MAX_CH → rbp_max_ch_setpoint (MsgID 6)
- DATA_REMOTE_RW_MAX_CH → rbp_rw_max_ch_setpoint (MsgID 6)
- DATA_REMOTE_TRANSFER_DHW → rbp_dhw_setpoint (MsgID 6)
- DATA_REMOTE_RW_DHW → rbp_rw_dhw_setpoint (MsgID 6)

Firmware extras (no HA core equivalent, audit-only): master_configuration_smart_power, heat_cool_mode_control.

On zero-fix outcome, the audit-report doc at docs/audits/2026-05-21-ha-capability-flags-dev.md is the deliverable; commit it docs-only and close Done. Parallel sibling task on feature-dev-2.0.0-otgw32-esp32-sat-support tracks the same audit against MQTTHaDiscovery.cpp.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Read each of the 13 binary_sensor entries (11 HA-core-matched + 2 firmware-extras) in mqtt_configuratie.cpp and record their current entity_category, device_class (if any), icon, retain flag, and the actual ON/OFF payload values published by OTGW-Core.ino
- [ ] #2 Confirm all 13 entries have entity_category=diagnostic (HA core uses DIAGNOSTIC for all capability flags); if any are missing, set them
- [ ] #3 Confirm no entry asserts a device_class — capability flags in HA core use device_class=None; if any wrongly assert PROBLEM/RUNNING/HEAT, remove it
- [ ] #4 Confirm the discovery payload's payload_on / payload_off (or value_template) matches what OTGW-Core.ino's sendMQTTData actually publishes ("ON" / "OFF")
- [ ] #5 Confirm all 13 discovery configs are published with retain=true
- [ ] #6 Spot-check the friendly ha_name_* strings for typos and basic English correctness; fix only clear typos, not naming-style changes
- [ ] #7 Add a code comment at the MsgID 3 bit-6 decode site noting that pyotgw names this bit DATA_SLAVE_REMOTE_RESET while the OT spec and our label call it remote_water_filling_function — both refer to the same wire bit
- [ ] #8 Produce docs/audits/2026-05-21-ha-capability-flags-dev.md capturing the per-entry findings table, any fixes applied, and a one-line conclusion (parity-confirmed / fixes-applied)
- [ ] #9 OFF-LIMITS — no changes to ha_lbl_* (topic labels), no new bits decoded, no MsgID handler changes, no ADR-070 source-separated layer changes, no gating/cadence logic changes
- [ ] #10 build.py --firmware exits 0; evaluate.py --quick shows no new findings
- [ ] #11 If any firmware code changed, _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh and src/OTGW-firmware/version.h + data/version.hash staged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read mqttHaBinSensors[] entries (mqtt_configuratie.cpp:1124-1147) for MsgID 2/3/6
2. Inspect composeBinSensorPayload (mqtt_configuratie.cpp:2089-2157) and streamBinarySensorDiscovery (line 2246) to confirm JSON keys, retain flag, availability_topic
3. Inspect publisher functions in OTGW-Core.ino: print_mastermemberid, print_slavememberid, print_RBPflags/publishRBPFlagsState
4. Confirm publishMQTTOnOff emits literal "ON"/"OFF" (MQTTstuff.ino:1179)
5. Add bit-6 ambiguity comment at print_slavememberid bit-6 site (AC#7)
6. Write audit report at docs/audits/2026-05-21-ha-capability-flags-dev.md
7. Run build.py --firmware and evaluate.py --quick — confirm green
8. Bump prerelease (bin/bump-prerelease.sh) since firmware comment was touched
9. Commit, push to origin/dev, mark Done
<!-- SECTION:PLAN:END -->
