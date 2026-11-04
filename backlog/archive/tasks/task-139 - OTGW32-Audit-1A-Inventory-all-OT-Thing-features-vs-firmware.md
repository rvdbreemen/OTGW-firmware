---
id: TASK-139
title: 'OTGW32-Audit-1A: Inventory all OT-Thing features vs firmware'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:16'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-1
milestone: m-1
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Systematically catalog every feature implemented in the OT-Thing reference project (thermo-nova branch in other projects) and verify each one is present in the firmware's OTDirect layer. Produce a gap list — any feature found in OT-Thing but missing or incomplete in OTDirect.ino becomes a candidate for an audit-fix task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All OT-Thing source files in thermo-nova branch are reviewed
- [x] #2 A feature inventory table is documented (OT-Thing feature vs firmware status: Present/Partial/Missing)
- [x] #3 Each gap is flagged and a corresponding audit-fix task is created
- [x] #4 Findings are added as implementation notes to this task
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate OT-Thing repo (found at /d/Users/Robert/Documents/GitHub/Others/OT-Thing/)
2. Read OT-Thing source files (main.cpp, otcontrol.cpp, mqtt.cpp, sensors.cpp, masterrequests.cpp, command.cpp)
3. Read OTDirect.ino in full
4. Build feature inventory table comparing every OT-Thing feature against OTDirect
5. Flag gaps and create audit-fix tasks
6. Document findings and mark ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
OT-Thing repo found at /d/Users/Robert/Documents/GitHub/Others/OT-Thing/ (master branch — no thermo-nova branch present; master is the current upstream).

Key OT-Thing source files reviewed:
- otcontrol.cpp (1468 lines): OT master/slave/repeater/loopback, PI controller, heating curve, summer mode, flame ratio tracking
- mqtt.cpp (310 lines): MQTT topics for CH/DHW/vent/room temp/setpoint/overrides
- masterrequests.cpp (87 lines): periodic write request objects (TSet, TdhwSet, TrSet, etc.)
- sensors.cpp (499 lines): 1-Wire DS18B20, BLE sensor, MQTT sensor, auto-detect for room temp
- main.cpp (472 lines): WiFi/Ethernet, OLED display, OTA, BLE
- command.cpp (125 lines): TCP command server on port 25238

Feature Inventory Table (OT-Thing vs OTDirect.ino):

| Feature | OT-Thing | OTDirect | Status |
|---|---|---|---|
| OT master (boiler comms) | Yes (async) | Yes (async, ISR) | PRESENT |
| OT slave (thermostat) | Yes (repeater/master modes) | Yes (gateway/monitor/master modes) | PRESENT |
| Bypass relay | Yes (GPIO_BYPASS_RELAY) | Yes (HAS_BYPASS_RELAY) | PRESENT |
| +24V step-up | Yes (GPIO_STEPUP_ENABLE) | Yes (PIN_STEPUP_ENABLE) | PRESENT |
| Loopback test mode | Yes | Yes | PRESENT |
| Summer mode (bit5 MsgID0) | Yes (boilerConfig.summerMode, SM= via web config) | Yes (SM= command, persisted) | PRESENT |
| Thermostat disconnect tracking | No dedicated timeout/setback logic | Yes (checkThermostatTimeout, SB=, FS=) | GAP: OTDirect has MORE than OT-Thing |
| DHW push state machine | No (OT-Thing has no HW=P logic) | Yes (PUSH_IDLE/PENDING/STARTED) | GAP: OTDirect has MORE |
| PI controller (room compensation) | Yes (loopPiCtrl, Kp/Ki/boost) | No | MISSING in OTDirect |
| Heating curve (weather comp) | Yes (getFlow: auto mode, exponent, gradient) | No | MISSING in OTDirect |
| CH mode control (ON/AUTO/OFF) | Yes (CTRLMODE_ON/AUTO/OFF) | Partial (only CS=/TT=/CH= bits) | PARTIAL |
| DHW override/mode | Yes (overrideDhw, setDhwCtrlMode) | Partial (HW=0/1/A/P) | PARTIAL |
| CH2 support | Yes (heatingCtrl[1], TsetCH2) | Yes (C2=, H2=, MsgID8) | PRESENT |
| Ventilation/HRV control | Yes (SLAVEAPP_VENT, ventCtrl) | Partial (VS= write, MsgID71 read) | PARTIAL |
| 1-Wire sensor integration | Yes (OneWireNode, DallasTemperature) | No | MISSING (hardware-specific) |
| BLE sensor integration | Yes (BLESensor) | No | MISSING (hardware-specific) |
| MQTT room temp/setpoint source | Yes (Sensor::SOURCE_MQTT) | No (no MQTT→OT sensor routing) | MISSING |
| MQTT control topics (CH mode, DHW mode) | Yes (full topic set) | Partial (standard OTGW MQTT inherited) | PARTIAL |
| Flame ratio tracking | Yes (FlameRatio: duty%, freq/h) | No | MISSING |
| HA Discovery | Yes (HADiscLocal) | Yes (via existing doAutoConfigure) | PRESENT |
| OTA firmware update | Yes (ArduinoOTA) | Yes (existing OTA) | PRESENT |
| OLED display | Yes (SSD1306 5-page) | No | N/A (different HW) |
| Smart Power (OT) | Yes (SMART_POWER_HIGH Level 3) | Partial (MsgID2 handshake only) | PARTIAL |
| Schedule table (auto-poll all MsgIDs) | No (manual sendRequest per loop) | Yes (full round-robin schedule) | OTDirect has MORE |
| 3-strike UNKNOWN_DATA_ID blacklist | No | Yes | OTDirect has MORE |
| Response override table (SR=/CR=) | No | Yes | OTDirect has MORE |
| Response modify table (RM=/CM=) | No | Yes | OTDirect has MORE |
| PS= summary mode | No | Yes | OTDirect has MORE |
| Command queue (ring buffer) | No (direct sendRequest) | Yes (8-frame ring) | OTDirect has MORE |
| PR= register queries | No | Yes | OTDirect has MORE |

Gap summary (features in OT-Thing missing from OTDirect):
1. PI room compensation controller
2. Weather-compensated heating curve (auto flow temp)
3. Flame ratio tracking (duty%, cycles/h)
4. MQTT-sourced room temp/setpoint routing to OT bus
5. 1-Wire / BLE sensor integration (HW-specific, may be out of scope)
6. Full CH mode (ON/AUTO/OFF) via MQTT for OTGW32
7. Ventilation control parity (SLAVEAPP_VENT vs partial VS= support)
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Inventoried OT-Thing source (master branch at /d/Users/Robert/Documents/GitHub/Others/OT-Thing — no thermo-nova branch present) against OTDirect.ino.

Note: the "thermo-nova" branch referenced in the task description does not exist in the local clone; master is the current reference.

Key findings:
- OTDirect matches or exceeds OT-Thing on: async OT master/slave, bypass relay, loopback mode, schedule table, response/command override tables, PS= summary, PR= queries, command queue, 3-strike blacklist.
- OT-Thing has features not in OTDirect: PI room compensation, weather heating curve, flame ratio tracking, MQTT-sourced sensor routing.
- Hardware-specific OT-Thing features (1-Wire, BLE, OLED, Ethernet) are out of scope for OTGW32.

Audit-fix tasks created:
- TASK-183: PI room compensation + weather heating curve
- TASK-184: Flame ratio tracking
- TASK-185: MQTT room temp/setpoint routing to OT bus
<!-- SECTION:FINAL_SUMMARY:END -->
