The second major feature release since v1.0.0.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.2.0.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [API docs](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/README.md)

## New features

- **Full Home Assistant auto-discovery**: Every OpenTherm message type is now automatically exposed to Home Assistant — 309 discovery configurations across 80+ message IDs covering heating, cooling, solar thermal, DHW, ventilation/heat recovery, CH2, relative humidity, operational counters, and system status. No manual YAML required.
- **Webhook support**: Configurable outbound HTTP call triggered on any OpenTherm status bit change (e.g. flame on/off). Separate URL, payload, and content type for on and off events. Restricted to local network URLs; disabled by default.
- **Source-separated MQTT topics**: Optional `mqttseparatesources` setting publishes per-source topics (`<metric>/thermostat`, `<metric>/boiler`, `<metric>/gateway`) alongside the existing unsuffixed topics for backward compatibility.
- **OpenTherm v4.2 protocol alignment**: Added missing message IDs 39 and 93–97; corrected direction flags, type semantics, and units for several IDs; added legacy ID 50–63 compatibility profile (auto-suppressed on detected v4.x systems in default `AUTO` mode).
- **Runtime safety hardening**: OT map bounds checks in parser and REST paths; safe fallback metadata for unknown IDs; `sendOTGWvalue()` validates message ID range before map access.
- **Serial and WebSocket diagnostics**: Serial line buffer increased from 256 to 512 bytes with safe overflow handling; richer WebSocket event logging for commands, responses, errors, PS mode transitions, resets, and PIC restarts.

## Bug fixes

- **`MQTTseparatesources` not persisted**: Setting was written to `settings.ini` but never read back on boot, silently resetting to `false` after every reboot.
- **Gateway mode parsing**: Fixed `PR=M` response parsing (`M=G` / `M=M`); added explicit detecting/unknown state instead of defaulting to monitor-like value.
- **MQTT/HA topic typos**: `eletric_production` → `electric_production`, `solar_storage_slave_fault_incidator` → `solar_storage_slave_fault_indicator`, and several `vh_*_ventilation_*` spelling fixes.
- **HA discovery mismatches**: `vh_configuration_*` now keyed to correct message ID 74; `Hcratio` discovery `stat_t` corrected; `FanSpeed` split into `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` (Hz).
- **MQTT autoconfig re-entry**: Shared static buffer workspace and scoped re-entry lock prevent buffer clobbering during concurrent auto-configuration runs.

## Breaking changes

**REST API**: v0 and v1 endpoints have been **removed**. Any client calling `/api/v0/` or `/api/v1/` will receive **410 Gone**. Migrate to `/api/v2/` — see [API docs](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/README.md).

**MQTT topic renames** (OpenTherm v4.2 alignment — delete orphaned HA entities after upgrading):

| Old topic | New topic |
|-----------|-----------|
| `eletric_production` | `electric_production` |
| `solar_storage_slave_fault_incidator` | `solar_storage_slave_fault_indicator` |
| `CumulativElectricityProduction` | `CumulativeElectricityProduction` |
| `vh_free_ventlation_mode` | `vh_free_ventilation_mode` |
| `vh_ventlation_mode` | `vh_ventilation_mode` |
| `RelativeHumidity_hb_u8` / `_lb_u8` | `RelativeHumidity` (f8.8 value) |
| `FanSpeed` (rpm) | `FanSpeed_setpoint_hz` + `FanSpeed_actual_hz` (Hz) |

**Device info API keys** (raw API consumers only): `gatewaymode`/`mode` → `otgwmode`; `wifiqualitytldr` → `wifiquality_text`.

Full migration guide: [opentherm-v42-mqtt-breaking-changes.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/fixes/opentherm-v42-mqtt-breaking-changes.md)

## Migration

**Flash both firmware and filesystem.** Hard browser refresh (Ctrl+F5) required after flashing.

After upgrading:
1. Remove stale Home Assistant entities (especially old `FanSpeed` and typo-topic entities).
2. Trigger MQTT auto-discovery again (`POST /api/v2/otgw/discovery` or via the Web UI).
3. Update any manual MQTT automations/sensors to the renamed topics above.
4. If you parse device info JSON directly, rename `gatewaymode`/`mode` → `otgwmode` and `wifiqualitytldr` → `wifiquality_text`.

## Thank you

This release would not have been possible without the community. Thank you to everyone who tested, reported issues, and pushed for improvements.

Special thanks to: @hvxl, @sjorsjuhmaniac, @DaveDavenport, @DutchessNicole, @RobR, @GeorgeZ83, @tjfsteele, @vampywiz19, @Stemplar, @proditaki, and everyone in the Discord.

If you want to support the project: [Buy me a coffee](https://www.buymeacoffee.com/rvdbreemen)
