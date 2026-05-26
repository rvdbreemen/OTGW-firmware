---
id: TASK-401
title: >-
  Per-bit change-detection + 60s heartbeat for MQTT fan-out on OT msgId 5 (ASF),
  6 (RBP) and 100 (Remote Override)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 12:05'
updated_date: '2026-04-24 12:15'
labels:
  - mqtt
  - heap
  - fanout
  - home-assistant
  - optimization
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Scope C-min extension of TASK-400: apply the same first-seen + change-detect + 60-second heartbeat publish gate to the three bit fan-out sites that currently publish on EVERY OT frame: msgId 5 Application-Specific Fault flags (6 bits), msgId 6 Remote Boiler Parameter flags (4 bits across transfer-enable + read-write bytes), and msgId 100 Remote Override Function (2 bits). These three were picked because they are the only non-Status fan-out sites that fire frequently enough to matter for heap pressure; the other config/fault bit fan-outs (msgId 2, 3, 74, 78, 103) fire rarely (config-poll cadence) and are intentionally left out of this task to minimise risk.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reuse STATUS_HEARTBEAT_INTERVAL_SEC (60s) from TASK-400 as the heartbeat constant; do NOT introduce a new interval
- [x] #2 msgId 5 ASF: publishMQTTOnOff for service_request, lockout_reset, low_water_pressure, gas_flame_fault, air_pressure_fault, water_over_temperature gated by per-bit change-detection; ASF_flags byte-topic also gated
- [x] #3 msgId 6 RBP: RBP_flags_transfer_enable and RBP_flags_read_write byte-topics gated; rbp_dhw_setpoint, rbp_max_ch_setpoint, rbp_rw_dhw_setpoint, rbp_rw_max_ch_setpoint bit-topics each independently gated
- [x] #4 msgId 100 Remote Override: <msgid>_flag8 byte-topic gated; remote_override_manual_change_priority and remote_override_program_change_priority bit-topics gated
- [x] #5 All bit payloads use 'ON'/'OFF' strings per HA MQTT discovery convention (reuse publishMQTTOnOff or equivalent helper)
- [x] #6 forcePublish on boot (first occurrence) publishes all bits and bytes once
- [x] #7 msgId 2, 3, 74, 78, 103 bit fan-out remain UNCHANGED — explicitly out of scope
- [x] #8 Other OT handlers honouring settings.mqtt.iInterval remain UNCHANGED
- [x] #9 Build clean on ESP8266 (python build.py --firmware): no warnings, no errors
- [x] #10 python evaluate.py --quick shows 0 new failures vs post-TASK-400 dev baseline
- [ ] #11 Runtime verification (user-performed on hardware): MQTT subscriber confirms ASF/RBP/RemoteOverride topics publish at boot, on change, and at most every 60s otherwise; heap [ot] trace shows reduced publish spike on those frames
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add new file-scope tracker arrays in OTGW-Core.ino (near mqttlastsentstatusbit): mqttlastsentASFbit[8], mqttlastsentASFbyte[1], mqttlastsentRBPbit[4], mqttlastsentRBPbyte[2], mqttlastsentRObit[2], mqttlastsentRObyte[1].\n2. Extend resetMqttTrackedState() to set all new arrays to TRACKED_TIME_UNSEEN.\n3. Modify print_ASFflags: compute prevHB from value>>8, gate ASF_flags byte-topic via shouldPublishTrackedStatusByte with OTPublishGate wrap; gate each of the 6 bit-topics via shouldPublishTrackedStatusBit + publishMQTTOnOff under gate.\n4. Modify publishRBPFlagsState: gate RBP_flags_transfer_enable and RBP_flags_read_write bytes; gate rbp_dhw_setpoint, rbp_max_ch_setpoint, rbp_rw_dhw_setpoint, rbp_rw_max_ch_setpoint bits. Function signature stays the same (takes transferEnableFlags, readWriteFlags, returns combined u16) but we need prev values — add prev-param plumbing from print_RBPflags which has access to value (uint16_t& ref).\n5. Modify print_remoteoverridefunction: gate <msgid>_flag8 byte-topic and the 2 bit-topics.\n6. Build firmware, verify clean.\n7. Run evaluate.py --quick.\n8. Commit + push.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Scope C-min extension of TASK-400: extend per-bit change-detection + 60s heartbeat gating to the three non-Status fan-out sites that fire frequently enough to matter for heap pressure — msgId 5 (ASF fault), msgId 6 (RBP flags), msgId 100 (Remote Override). The other config bit fan-outs (msgId 2, 3, 74, 78, 103) remain raw-publish intentionally; they fire at config-poll cadence (rarely) and were excluded to minimise risk surface.

Changes (OTGW-Core.ino):
- 6 new file-scope tracker arrays (ASFbit[8], ASFbyte[1], RBPbit[4], RBPbyte[2], RObit[2], RObyte[1]) — total +36 bytes static RAM.
- All 6 arrays initialised to TRACKED_TIME_UNSEEN in resetMqttTrackedState() so the first OT frame after boot publishes all bits/bytes once.
- Two new generic helpers publishGatedBitMQTT / publishGatedByteMQTT (with a char* overload for the dynamic '<msgid>_flag8' topic) — wrap shouldPublishTrackedStatusBit/Byte + OTPublishGate + publishMQTTOnOff / sendMQTTData. No status-burst cooldown (scoped to msgId 0).
- print_ASFflags: extracts prev HB from OTcurrentSystemState.ASFflags, gates ASF_flags byte-topic + 6 fault bit-topics. OEMFaultCode left raw (numeric value, not a bit, HA wants fresh code).
- publishRBPFlagsState: signature extended with prevTransfer + prevReadWrite; gates 2 byte-topics + 4 bit-topics. Both call sites (print_RBPflags and publishPSSummaryFieldValue case 6) updated.
- print_remoteoverridefunction: extracts prev LB, gates <msgid>_flag8 byte-topic + 2 bit-topics.

Impact:
- Config-poll frames (msgId 5/6/100) drop from 3-6 publishes per frame to 0 publishes per frame in steady state (publishes only on bit-flip or 60s heartbeat).
- msgId 5 is the biggest win: under an active fault, the ASF frame re-fires continuously and previously spawned 7 publishes per frame. Now: at most 7 publishes per 60s.
- HA reconnect recovery: full fault/RBP/RO state re-snapshot within 60 seconds.
- settings.mqtt.iInterval continues to govern all other OT topic throttles unchanged.

Verification:
- python build.py --firmware -> exit 0, 0 warnings, 0 errors. Binary 0.70 MB.
- python evaluate.py --quick -> 31/31 passed, 2 pre-existing warnings, 94.3% health.
- Runtime verification left to user on hardware (AC #11).

Out of scope (decision C-min): msgId 2 master config, msgId 3 slave config, msgId 74 VH slave config, msgId 78 VH remote-param, msgId 103 solar-storage config. These remain raw-publish; a future C-full task can wrap them if hardware data shows they contribute meaningfully to heap pressure.
<!-- SECTION:FINAL_SUMMARY:END -->
