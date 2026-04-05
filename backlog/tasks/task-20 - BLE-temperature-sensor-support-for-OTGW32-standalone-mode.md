---
id: TASK-20
title: BLE temperature sensor support for OTGW32 standalone mode
status: To Do
assignee: []
created_date: '2026-04-05 11:44'
labels:
  - sat
  - feature
  - ble
  - otgw32
  - mqtt
  - rest
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
For true standalone operation without a thermostat or WiFi/HA dependency, the OTGW32 needs a local room temperature sensor. BLE (Bluetooth Low Energy) is ideal: low power, no WiFi required, works offline. Common BLE temp sensors (Xiaomi LYWSD03MMC, ATC firmware, BTHome protocol) broadcast temperature readings that the ESP32 can receive. This makes the OTGW32 a fully self-contained heating controller: boiler + OTGW32 + BLE sensor = complete system, no thermostat or smart home platform needed. The BLE sensor becomes the primary room temperature source for SAT when available, with OT bus MsgID 24 and MQTT external temp as fallbacks.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ESP32 BLE scan for compatible temperature sensors (BTHome protocol, ATC/pvvx firmware)
- [ ] #2 Setting: BLE sensor MAC address to bind to (settings.sat.sBleMAC or similar)
- [ ] #3 Setting: BLE scan interval (default 30s, range 10-300)
- [ ] #4 BLE temp reading feeds into SAT as room temperature source (highest priority)
- [ ] #5 Temperature source priority: BLE sensor > MQTT external > OT bus MsgID 24
- [ ] #6 Staleness detection: if no BLE reading for 5 min, fall back to next source
- [ ] #7 State tracking: state.sat.fBleTemp, state.sat.bBleTempValid, state.sat.iBleTempLastMs
- [ ] #8 BLE scan must not block the main loop (use async/non-blocking ESP32 BLE API)
- [ ] #9 REST API: GET /api/v2/sat/status includes ble_temp, ble_temp_valid, ble_sensor_rssi
- [ ] #10 MQTT publish: sat/ble_temp, sat/ble_sensor_rssi
- [ ] #11 WebUI: BLE sensor status in SAT dashboard (temp, RSSI, last seen)
- [ ] #12 WebUI: BLE sensor MAC configuration in settings page
- [ ] #13 HA auto-discovery: sensor entity for BLE temperature
- [ ] #14 Only compiled for ESP32 targets (#if defined(HAS_BLE) or similar guard)
<!-- AC:END -->
