# Breaking Changes Log

This document is the cumulative log of breaking changes from **v1.0.0** onwards. Always check this file before upgrading your firmware if you skip versions, especially if you rely heavily on custom automations via MQTT or precise data structure parsing directly from the REST API endpoints.

---

## 🛑 v1.4.2

### Breaking: heap diagnostic MQTT topic split from one JSON blob into 17 individual retained topics

In v1.4.1 the hourly heap diagnostic was published as a single retained JSON blob on:

```
<topTopic>/value/<uniqueid>/otgw-firmware/stats/heap
```

That topic is **removed** in v1.4.2. The same 17 metrics are now published as individual retained topics under:

```
<topTopic>/value/<uniqueid>/otgw-firmware/stats/<metric>
```

Each topic carries a plain ASCII decimal number. Metrics: `ws_drops`, `mqtt_drops`, `enter_low`, `enter_warning`, `enter_critical`, `drip_burst_skip`, `drip_cooldown_skip`, `drip_slowmode`, `free_heap`, `max_block`, `frag_pct`, `disc_verify_runs`, `disc_republish_triggered`, `disc_last_missing`, `disc_last_orphan`, `disc_published_topics`, `disc_last_verify_epoch`.

**Action required when upgrading from v1.4.1:**

- If your Home Assistant or Grafana setup subscribed to `<topTopic>/value/<uniqueid>/otgw-firmware/stats/heap` and used a `value_template` / JSON path to extract a field, replace that with a direct subscription to the corresponding `<topTopic>/value/<uniqueid>/otgw-firmware/stats/<metric>` topic. No JSON parsing needed.
- The old `.../stats/heap` topic is no longer published. If it still sits on your broker as a retained message, clear it manually or wait for broker expiry: the firmware will not overwrite it.
- Subscribe to `<topTopic>/value/<uniqueid>/otgw-firmware/stats/+` to receive all 17 metrics in one wildcard subscription.

### Additive: retained hostname-to-uniqueid mapping topic

A new retained topic exposes the human-readable hostname for each device:

```
<topTopic>/value/<uniqueid>/otgw-firmware/hostname
```

Published on MQTT (re)connect. This lets broker-explorers, multi-device dashboards, and troubleshooting scripts map a cryptic `<uniqueid>` (e.g. `otgw-a1b2c3`) back to the user-visible hostname (e.g. `zolder-otgw`). Additive only: no existing topic changes behavior.

---

## 🛑 v1.4.1

v1.4.1 is the first public release in the 1.4.x series (v1.4.0 was an internal development milestone that was never published).

### Breaking: LittleFS partition size changed from 1 MB to 2 MB

The upgrade to Arduino Core 3.1.2 changes the LittleFS partition size from 1 MB to 2 MB. **You must flash both the firmware binary and the filesystem binary in the same session.**

**Upgrading from v1.3.x (Arduino Core 2.7.4):**

If you flash only the firmware binary without flashing the filesystem binary, the OTGW will detect a stale 1 MB filesystem at the wrong partition offset. It will spend approximately 5 to 10 minutes reformatting the new 2 MB partition on first boot. During this time the device is unresponsive: the web UI is unreachable and MQTT stays offline. After the reformat completes, all settings are gone — MQTT broker, credentials, hostname, and every other setting resets to factory defaults. You must re-enter all settings manually after the first boot.

**Upgrading from v1.4.x (already on Arduino Core 3.1.2):**

If you skip the filesystem flash, the OTGW can still read existing settings but any setting change will silently fail to persist across reboots. Recovering from this state requires flashing the filesystem image.

**Correct upgrade procedure (applies to all upgrades):**
1. Download both `OTGW-firmware-*.ino.bin` and `OTGW-firmware-*.littlefs.bin` from the release.
2. Flash the **filesystem binary first** via the Web UI update page. Doing this before the firmware flash preserves your existing settings and avoids the first-boot reformat.
3. Flash the **firmware binary second**, immediately after, via the same update page.
4. Hard-refresh the browser (Ctrl+F5) after flashing.

**Recovery: if you already flashed in the wrong order (firmware before filesystem):**
The device spends approximately 5 to 10 minutes reformatting the 2 MB partition on first boot. It is unresponsive during that time — the web UI is unreachable and MQTT stays offline. After the reformat, settings reset to factory defaults. Flash the filesystem binary once the device becomes responsive again, hard-refresh the browser, and re-enter your settings manually via the Web UI.

### No other breaking changes

All MQTT topics, REST API endpoints, and settings format remain identical to `v1.3.5`. New additions are purely additive:

- `<topTopic>/value/<uniqueid>/otgw-firmware/stats/heap` is a new retained topic (additive). The `<uniqueid>` segment is automatically inserted by the publish namespace so multiple OTGWs on one broker cannot overwrite each other.
- Three new REST endpoints (`GET /api/v2/discovery`, `POST /api/v2/discovery/verify`, `POST /api/v2/discovery/republish`) do not replace or alter any existing endpoint.
- `MQTTdiscoveryAutoVerify` is a new setting (default `true`). On shared brokers or brokers with tight wildcard ACLs, set it to `false`.

See [RELEASE_NOTES_1.4.1.md](../RELEASE_NOTES_1.4.1.md) for the complete changelog covering all changes since `v1.3.5`.

---

## 🛑 v1.3.5

There are **no breaking changes** in `v1.3.5`. This release fixes the WiFi reconnection regression from v1.3.0 and adds MQTT uptime/version publishing. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical to `v1.3.4`.

---

## 🛑 v1.3.4

There are **no breaking changes** in `v1.3.4`. This release fixes MQTT throttle slot suppression, adds Debug Info tooltips, renames "OTGW Connected" to "OpenTherm Active", and adds thermostat-only MQTT support. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical to `v1.3.3`.

---

## 🛑 v1.3.3

There are **no breaking changes** in `v1.3.3`. This release adds PIC-less OTGW support and fixes dashboard display of unsupported OT values. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical to `v1.3.2`.

---

## 🛑 v1.3.2

There are **no breaking changes** in `v1.3.2`. This is a bugfix release for the file explorer. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical to `v1.3.1`.

---

## 🛑 v1.3.1

There are **no breaking changes** in `v1.3.1`. This is a stability and bugfix release. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical to `v1.3.0`.

---

## 🛑 v1.3.0

There are **no new breaking changes** in `v1.3.0` regarding external behaviors, MQTT topics, REST endpoints, or settings format.

What changed in `v1.3.0` without breaking existing `v1.2.0` setups:

1. **Optional protected admin endpoints:** HTTP Basic Auth can now protect settings and maintenance routes, but it is disabled by default until a password is configured.
2. **Manual JSON serialization:** ArduinoJson was removed from firmware-side JSON generation to reduce RAM pressure, but the external JSON contract remains the same for supported clients.
3. **Additive `PS=1` coverage:** More `PS=1` summary data is translated into the normal publish/discovery path, but no existing topic renames or API removals were introduced in this release.

> **Important:** The significant migration items still belong to `v1.2.0`. If you are upgrading from older than `v1.2.0`, review the v1.2.0 and v1.0.0 sections below before flashing `v1.3.0`.

## 🛑 v1.2.0

`v1.2.0` includes significant behavioral updates to MQTT and topics structure. It aligned the gateway firmware to **OpenTherm specification v4.2**, fixing historically incorrect data topics.

1. **MQTT Topics Renames:**
   - Typo `eletric_production` renamed to `electric_production`.
   - Typo `solar_storage_slave_fault_incidator` renamed to `solar_storage_slave_fault_indicator`.
   - Typo `CumulativElectricityProduction` renamed to `CumulativeElectricityProduction`.
   - Typo `vh_free_ventlation_mode` renamed to `vh_free_ventilation_mode`.
   - Typo `vh_ventlation_mode` renamed to `vh_ventilation_mode`.
   - Typo `vh_tramfer_enble_nominal_ventlation_value` renamed to `vh_transfer_enable_nominal_ventilation_value`.
   - Typo `vh_rw_nominal_ventlation_value` renamed to `vh_rw_nominal_ventilation_value`.
   - `RelativeHumidity_hb_u8` & `RelativeHumidity_lb_u8` (formerly ID 38 misdecoded as `u8/u8`) is now `RelativeHumidity` publishing a v4.2 standard `f8.8` value.
2. **Legacy IDs 50-55 and 58-63 (Auto Suppression):** For v4.x compliant systems (most common setups), OpenTherm IDs `50-55` and `58-63` are now strictly defined as reserved and suppressed by default in `AUTO` mode. IDs `56` (TdhwSet) and `57` (MaxTSet) are **not** suppressed — they are valid in OpenTherm v4.2.
3. **Advanced `FanSpeed` translation:** Standard HA discovery no longer parses `FanSpeed` natively in `rpm` — it creates the dual entities `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` (`Hz`).
4. **Device Info Payload (`GET /api/.../device/info`):** Keys in the custom JSON body have been renamed.
   - `mode` or `gatewaymode` is now explicitly **`otgwmode`** (`ON`, `OFF` or `detecting`).
   - `wifiqualitytldr` is explicitly renamed to **`wifiquality_text`**.
5. **REST API v0 and v1 API explicit deprecation warning:** Started returning `410 Gone`, moving all focus into `/api/v2/`.

> **Action required when upgrading**: Remove stale Home Assistant entities, clear retained MQTT topics in your broker for this device's specific prefix before re-triggering MQTT Discovery. Fix any custom NodeRed/HA automations relying on the old `otgwmode` map name inside generic REST sensors.

## 🛑 v1.1.0

There are **no breaking changes** in `v1.1.0`. All prior `v1.0.0` integrations remain valid.

## 🛑 v1.0.0

The `v1.0.0` milestone stabilized the core code of this custom firmware versus the historical versions. It includes fundamental architectural lock-ins.

1. **GPIO Defaults Adjustments (Dallas Sensors):** The default GPIO for the Dallas temperature sensors changed globally to GPIO 10 to officially match standard hardware specifications across devkit board revisions. Upgraders migrating from `<= v0.10.x` have to manually migrate their pin setups or allow `auto-migration` (which attempts it but explicitly suggests reviewing the setup page).
2. **Live log transport changed to WebSocket:** Legacy polling for live log frames over HTTP was removed. External solutions wanting real-time message-frame data must attach to the WebSocket endpoint instead of the old polling approach.
3. **Configuration should be re-verified after upgrade:** Settings preservation and migration behavior changed significantly in the v1.0.0 milestone. Existing users upgrading from pre-1.0 builds should review their configuration after flashing, especially Dallas-sensor-related settings.

---
