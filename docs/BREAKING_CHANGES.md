# Breaking Changes Log

This document is the cumulative log of breaking changes from **v1.0.0** onwards. Always check this file before upgrading your firmware if you skip versions, especially if you rely heavily on custom automations via MQTT or precise data structure parsing directly from the REST API endpoints.

---

## v1.6.1

### Behavior change: MQTT publish interval defaults to 60 seconds

MQTT on-change publishing is now enabled by default in the backend. Fresh settings use `MQTTonChangePublishing=true` with `MQTTinterval=60`, so changed OpenTherm values still publish immediately and unchanged values are refreshed once per minute instead of every repeated OpenTherm frame.

During the v1.6.1 settings load, an existing `MQTTinterval=0` is migrated to `60` and saved back to `settings.ini` when `MQTTonChangePublishing` is enabled or missing from the settings file.

**Who is affected:** users or custom consumers that relied on unchanged MQTT topics being republished for every repeated OpenTherm frame.

**Migration:** no action is needed for the new default. To keep legacy every-message publishing, set `MQTTonChangePublishing=false` in `settings.ini` or disable publish-on-change before saving the MQTT settings.

---

## v1.6.0

### Breaking: HA entity availability reflects MQTT link, not OT-bus liveness (ADR-074)

HA entity availability (`avty_t`) now reflects the ESP-to-MQTT link (birth/LWT) instead of OpenTherm-bus liveness. Entities like `DHW Control` and `Thermostat` will no longer go `unavailable` when the boiler stops responding on the OT bus; they only become `unavailable` when the MQTT connection itself drops.

OT-bus liveness is now reported exclusively on the dedicated `otgw_connected` binary sensor.

**Who is affected:** users with automations that trigger on HA sensor entities going `unavailable` as a proxy for "boiler is unreachable." These automations must be migrated to trigger on the `otgw_connected` sensor instead.

**Who is not affected:** users who monitor the dedicated `otgw_connected` sensor (already the recommended approach), or users who do not use OT-bus liveness as an automation trigger.

### Breaking: `resetgateway` MQTT command requires payload `"1"` (TASK-661)

The `resetgateway` MQTT command now requires payload `"1"`. Commands with any other payload are logged and ignored. The command is also rate-limited to one PIC reset per 5 seconds.

**Who is affected:** custom automations that send a `resetgateway` command with an empty payload or any value other than `"1"`. Update the automation payload to `"1"`.

**Who is not affected:** Home Assistant automations using the HA-discovery `button` entity for the PIC reset. The discovery config already specifies `payload_press: "1"` so these automations are unaffected.

---

## v1.5.0

### Breaking: MQTT source-topic shape changed to sibling-suffix (ADR-070, ADR-071)

When the `Separate Sources` setting (`bSeparateSources`) is enabled, the per-source variant topics for dual-source OpenTherm message IDs (for example `TSet`, which is written by the thermostat and echoed by the boiler) have changed shape.

**Old shape (v1.4.x, nested children):**

```
<topTopic>/value/<uniqueid>/TSet           (canonical)
<topTopic>/value/<uniqueid>/TSet/thermostat
<topTopic>/value/<uniqueid>/TSet/boiler
```

**New shape (v1.5.0, sibling-suffix):**

```
<topTopic>/value/<uniqueid>/TSet           (canonical, unchanged)
<topTopic>/value/<uniqueid>/TSet_thermostat
<topTopic>/value/<uniqueid>/TSet_boiler
```

The same suffix rule applies to the HA discovery topics:

```
homeassistant/sensor/<id>/TSet/config           (canonical, unchanged)
homeassistant/sensor/<id>/TSet_thermostat/config  (was TSet/thermostat/config)
homeassistant/sensor/<id>/TSet_boiler/config      (was TSet/boiler/config)
```

Note: the nested discovery topic shape (`TSet/thermostat/config`) was silently rejected by HA's `TOPIC_MATCHER` regex at all times, so `bSeparateSources` source-variant entities never registered in HA under v1.4.x. The sibling-suffix shape is what actually makes those entities appear in HA for the first time.

**What breaks:**

- Any manual HA YAML sensor configuration, Node-RED flow, or Grafana dashboard that subscribes to `<topTopic>/value/<uniqueid>/<msgid>/thermostat` or `<topTopic>/value/<uniqueid>/<msgid>/boiler` (the old nested state topics). These subscriptions will receive no new data after upgrading.
- Any MQTT wildcard subscription that relied on the nested child structure will need to be updated to match the new suffix pattern.

**Migration:**

1. If you use HA MQTT auto-discovery (`bSeparateSources=true`): no action required. HA's subscription logic picks up the new `state_topic` value from the updated discovery payload automatically on the next firmware boot (ADR-067 republishes discovery at boot). The source-variant entities will appear in HA for the first time as working entities.
2. If you have manually configured YAML sensors pointing at `<topTopic>/value/<uniqueid>/<msgid>/thermostat` or `<topTopic>/value/<uniqueid>/<msgid>/boiler`: update the `state_topic` values to use the underscore-suffix form (`<msgid>_thermostat`, `<msgid>_boiler`).
3. Clear zombie retained values left at the old nested state-topic paths. The firmware no longer publishes there. On mosquitto: `mosquitto_pub -t '<topTopic>/value/<uniqueid>/<msgid>/thermostat' -r -n` and the same for `/boiler`.
4. Clear zombie retained discovery configs at the old nested discovery paths. They were already invisible to HA (rejected before registration), but topic browsers display them. On mosquitto: `mosquitto_pub -t 'homeassistant/sensor/<id>/<msgid>/thermostat/config' -r -n` and the same for `/boiler/config`.

---

### Breaking: /gateway sub-topic removed (TASK-538)

The per-message-ID sub-topic `/gateway` has been removed.

**Old topic (v1.4.x):**

```
<topTopic>/value/<uniqueid>/<msgid>/gateway
```

**New equivalent (v1.5.0):**

```
<topTopic>/value/<uniqueid>/<msgid>
```

The canonical base topic now carries the value that was previously published on the `/gateway` sub-topic. There is no longer a `/gateway` child topic at all.

**What breaks:**

- Any automation, subscription, or integration that reads from `<topTopic>/value/<uniqueid>/<msgid>/gateway` will receive no new data after upgrading.

**Migration:**

Update every subscription and `state_topic` reference that ends in `/<msgid>/gateway` to use `/<msgid>` instead (drop the `/gateway` suffix). On mosquitto, clear the stale retained value: `mosquitto_pub -t '<topTopic>/value/<uniqueid>/<msgid>/gateway' -r -n`.

---

### Breaking: HA discovery entity names changed to human-readable Title Case (ADR-072, TASK-572, TASK-573)

The `name` field in all HA MQTT discovery payloads has changed format.

**Old format (v1.4.x):** underscore-separated identifier string with hostname prefix, for example:

```
OTGW_TdhwSet
OTGW_Status_Master_Memberid_Code
OTGW_ElectricalCurrentBurnerFlame
OTGW_CHPumpOperationHoursg
```

**New format (v1.5.0):** human-readable Title Case with spaces, no hostname prefix, acronyms in canonical caps, for example:

```
DHW Setpoint
Status Master MemberID Code
Electrical Current Burner Flame
CH Pump Operation Hours
```

The `unique_id` field in every discovery payload is unchanged. HA uses `unique_id` for entity identity tracking, so existing automations and entity-ID references are not broken by the name change alone.

**What breaks:**

- The user-visible entity display name in HA updates on the next discovery cycle after flashing v1.5.0. This is cosmetic: the entity_id and unique_id are stable. Dashboards built on entity_id (for example `sensor.otgw_tdhwset`) continue to work without change.
- HA derives an initial `entity_id` from the `name` on first discovery for brand-new entities. If a user has renamed an entity inside HA and pinned it to the old generated entity_id, the rename is sticky and unaffected.
- If a user has a custom dashboard card that displays the HA `friendly_name` as a label and relied on the exact old string (for example `OTGW_TdhwSet`), the displayed label will change.

**Migration:**

For most users: no action required. The entity display names update automatically on the next discovery cycle.

If you want to clean up stale HA entity entries that carry the old names: remove them from the HA entity registry (Settings, Devices and Services, MQTT, find the OTGW device, remove stale entities), then trigger a discovery republish from the OTGW web UI or wait for the next firmware boot.

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
