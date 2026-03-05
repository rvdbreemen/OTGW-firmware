# Breaking Changes Log

This document lists all the breaking changes from **v1.0.0** onwards. Always check this file before upgrading your firmware if you skip versions, especially if you rely heavily on custom automations via MQTT or precise data structure parsing directly from the REST API endpoints.

---

## 🛑 v1.3.0

There are **no breaking changes** in `v1.3.0` regarding external behaviors, topics, or endpoints.

1. **REST API Version 0 & 1 Removal finalized:** Legacy API v0 (`/api/v0`) and v1 endpoints originally deprecated and functionally hollowed out in `v1.2.0` are now completely eradicated from the system. (Return `410 Gone`). Always use `/api/v2/`.
2. **ArduinoJson completely removed:** The codebase relies on a manual JSON serialization method to dramatically improve memory efficiency. External representation of valid API queries remains identical, but custom integration parser strictly expecting loose or non-orthodox formatting might need to be adjusted.

## 🛑 v1.2.0

`v1.2.0` includes significant behavioral updates to MQTT and topics structure. It aligned the gateway firmware to **OpenTherm specification v4.2**, fixing historically incorrect data topics.

1. **MQTT Topics Renames:**
   - Typo `eletric_production` renamed to `electric_production`.
   - Typo `solar_storage_slave_fault_incidator` renamed to `solar_storage_slave_fault_indicator`.
   - `RelativeHumidity_hb_u8` & `RelativeHumidity_lb_u8` (formerly ID 38 misdecoded as `u8/u8`) is now `RelativeHumidity` publishing a v4.2 standard `f8.8` value.
2. **Legacy IDs 50-63 (Auto Suppression):** For v4.x compliant systems (most common setups), OpenTherm IDs `50-63` are now strictly defined as reserved and suppressed by default in `AUTO` mode.
3. **Advanced `FanSpeed` translation:** Standard HA discovery no longer parses `FanSpeed` natively in `rpm` — it creates the dual entities `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` (`Hz`).
4. **Device Info Payload (`GET /api/.../device/info`):** Keys in the custom JSON body have been renamed.
   - `mode` or `gatewaymode` is now explicitly **`otgwmode`** (`ON`, `OFF` or `detecting`).
   - `wifiqualitytldr` is explicitly renamed to **`wifiquality_text`**.
5. **REST API v0 and v1 API explicit deprecation warning:** Started returning `410 Gone`, moving all focus into `/api/v2/`.

> **Action required when upgrading**: Remove stale Home Assistant entities, clear retained MQTT topics in your broker for this device's specific prefix before re-triggering MQTT Discovery. Fix any custom NodeRed/HA automations relying on the old `otgwmode` map name inside generic REST sensors.

## 🛑 v1.1.0

There are **no breaking changes** in `v1.1.0`. All prior `v1.0.0` implementations strictly persisted.
1. The **`otgwmode`** logic debuted here functionally inside the REST API without fundamentally breaking `v1`.

## 🛑 v1.0.0

The `v1.0.0` milestone stabilized the core code of this custom firmware versus the historical versions. It includes fundamental architectural lock-ins.

1. **GPIO Defaults Adjustments (Dallas Sensors):** The default GPIO for the Dallas temperature sensors changed globally to GPIO 10 to officially match standard hardware specifications across devkit board revisions. Upgraders migrating from `<= v0.10.x` have to manually migrate their pin setups or allow `auto-migration` (which attempts it but explicitly suggests reviewing the setup page).
2. **WebSocket Priority:** Legacy polling for log frames via HTTP has been officially destroyed. External solutions wanting live message-frame data must attach to the pure WebSocket (`ws://<ip>:8080`) port. 

---