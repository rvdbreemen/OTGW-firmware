# Release Notes — v1.2.0-beta

**Last updated:** 2026-02-28<br>
**Release branch:** `dev-1.2.0-stable-version-adding-webhook`<br>
**Comparison target:** `v1.0.0` (tag `v1.0.0`, released 2026-02-08)<br>

---

## ✨ Headline: Comprehensive Home Assistant Discovery — All OpenTherm Message Types

**v1.2.0-beta brings full Home Assistant MQTT auto-discovery for the complete OpenTherm protocol.**

Previous releases focused primarily on heating and hot-water sensors. This beta release expands HA discovery to cover **every message category in the OpenTherm spec** — 309 discovery configurations spanning 80+ message IDs.

### What is now exposed to Home Assistant automatically

| System | Example entities | OpenTherm IDs |
|---|---|---|
| 🔥 **Central Heating** | Flame on/off, CH setpoint, boiler flow/return temperature, modulation level, water pressure | 0, 1, 14, 17, 18, 25, 28 |
| 🧊 **Cooling** | Cooling active, cooling enabled, cooling control signal, cooling configuration | 0 (bits), 3 (bits), 7 |
| ☀️ **Solar / Thermal Storage** | Solar collector temperature, solar storage temperature, solar storage mode/status, solar slave fault indicator | 29, 30, 101, 113, 114 |
| 💧 **Domestic Hot Water (DHW)** | DHW temperature, DHW setpoint, flow rate, DHW 2 temperature, DHW enable, pump/burner starts and hours | 19, 26, 32, 48, 56, 57, 118, 119, 122, 123 |
| 🌡️ **Room / Thermostat** | Room temperature, room setpoint, room setpoint CH2, remote override room setpoint | 9, 16, 23, 24 |
| 💨 **Ventilation / Heat Recovery (VH)** | Ventilation enabled, relative ventilation position (ControlSetpointVH), ASF fault code, diagnostic code, VH versions/TSP | 70–91 |
| 🌬️ **Secondary Circuit (CH2)** | CH2 setpoint, CH2 flow temperature, CH2 enable | 8, 31 |
| 📊 **Sensors & Environment** | Relative humidity, outside temperature, electrical current (burner flame) | 27, 33, 36, 38 |
| ⚡ **Electric / Other** | Electric production | 0 (bit) |
| 🔧 **System & Status** | Gateway mode, thermostat connected, boiler connected, PIC version, boiler/gateway OT version | 0–5, 100, 115, 124–127, 245–246 |
| 🔢 **Operational Counters** | CH pump starts/hours, DHW pump/valve starts/hours, DHW burner starts/hours, flame starts/hours | 116–123 |
| 🌐 **Boiler / Remote Configuration** | Boiler configuration (DHW present, CH2 present, cooling config), master configuration (low-off pump control), remote boiler parameters | 3, 6, 48, 49 |
| 🏭 **Fault & Diagnostic** | Service request, lockout reset, OEM fault code, OEM diagnostic code | 5, 115 |

**Total: over 90 unique MQTT topics and 309 HA discovery configurations** across binary sensors, sensors, and climate entities.

### Why this matters

Previously, users with cooling-capable boilers, solar thermal systems, heat-recovery ventilation, or multi-circuit setups had to manually create MQTT sensors in Home Assistant. Now, once you enable MQTT auto-discovery, Home Assistant will **automatically** create entities for all those systems — the same way it creates heating entities.

A boiler supporting cooling will now expose Home Assistant entities for cooling active (binary sensor), cooling enable, cooling configuration, and the cooling control signal (percentage), all created automatically via MQTT discovery. A system with solar thermal collectors will get `Tsolarcollector` and `Tsolarstorage` temperature sensors, and solar storage mode/status sensors. A ventilation unit connected via OpenTherm will expose `vh_ventilation_enabled`, `ControlSetpointVH`, ASF fault codes, and more — all automatically, with no manual YAML needed.

---

## Breaking Changes

> **Action required for upgraders from v1.1.x**

### MQTT topic renames (Home Assistant entity cleanup needed)

Two MQTT topic names were corrected for spelling. Existing Home Assistant entities subscribed to the old topic names will stop receiving updates after upgrade and will appear as orphaned entities:

| Old topic (v1.1.x) | New topic (v1.2.0) | Reason |
| --- | --- | --- |
| `…/eletric_production` | `…/electric_production` | Spelling fix |
| `…/solar_storage_slave_fault_incidator` | `…/solar_storage_slave_fault_indicator` | Spelling fix |
| `…/CumulativElectricityProduction` | `…/CumulativeElectricityProduction` | Spelling fix |
| `…/vh_free_ventlation_mode` | `…/vh_free_ventilation_mode` | Spelling fix |
| `…/vh_ventlation_mode` | `…/vh_ventilation_mode` | Spelling fix |
| `…/vh_tramfer_enble_nominal_ventlation_value` | `…/vh_transfer_enable_nominal_ventilation_value` | Spelling fix |
| `…/vh_rw_nominal_ventlation_value` | `…/vh_rw_nominal_ventilation_value` | Spelling fix |

**Migration**: Delete the old entities from Home Assistant (Settings → Devices & Services → MQTT → delete orphaned entities), then trigger MQTT discovery re-registration via the device UI or `POST /api/v2/otgw/discovery`. Manual MQTT consumers must update topic subscriptions because these typo-fix renames do not publish backward-compatibility aliases.

### MQTT separate-source topics are opt-in (default: disabled)

A new feature (`MQTTseparatesources`) publishes source-specific MQTT topics per OT message (thermostat/boiler/gateway). **This is disabled by default** in v1.2.0 for backward compatibility. Upgraders are not affected unless they opt in.

To enable: set `MQTTseparatesources = true` in settings, then run MQTT discovery to register the new per-source HA entities.

### v0 and v1 REST API removed (action required for API consumers)

All `/api/v0/` and `/api/v1/` endpoints have been **removed**. Any client calling these paths will now receive **410 Gone**.

**Migration**: Update all integrations to use the `/api/v2/` equivalents. See [docs/api/README.md](docs/api/README.md) for the complete v2 endpoint reference. The v2 API has been available since v1.1.0 — no functional changes were made to the v2 endpoints themselves.

---

## Overview

Version `1.2.0` builds on the stable `v1.0.0` baseline with two major release increments of improvements:

**v1.1.0 additions** (Dallas sensors, RESTful API v2, memory safety, 20 bug fixes):

- Dallas DS18x20 temperature sensors with custom labels, MQTT/HA publishing, and real-time graphs
- Complete RESTful API v2 with consistent JSON errors, 202 Accepted for async ops, CORS, OpenAPI spec
- Streaming file serving (95% memory reduction) — fixes the slow Web UI reported after v1.0.0
- MQTT whitespace credential fix (whitespace trimmed automatically)
- Non-blocking modal dialogs replacing `prompt()`/`alert()`
- Browser debug console (`otgwDebug`) for in-browser diagnostics
- PS mode (`PS=1`) auto-detection and UI handling
- 20 bug fixes (out-of-bounds writes, XSS, ISR race conditions, file descriptor leaks, and more)

**v1.2.0 additions** (OpenTherm v4.2 alignment, comprehensive HA discovery, source-separated MQTT, gateway reliability, webhook, v2-only API):

- **Comprehensive Home Assistant MQTT auto-discovery** — all OpenTherm message types (see section above)
- OpenTherm v4.2 protocol map alignment — new IDs, corrected types/directions/units
- Configurable source-separated MQTT topics (`MQTTseparatesources`)
- Gateway-mode detection reliability, serial robustness, WebSocket diagnostics
- Web UI mobile/responsive improvements, shared navigation shell
- **Webhook** — HTTP call on OpenTherm status bit change (configurable URL, payload, content type)
- **v0 and v1 REST API removed** — only v2 remains; ArduinoJson replaced with streaming JSON I/O
- **Bug fix** — `MQTTseparatesources` setting not persisted across reboots

---

## Summary of Features and Fixes

### New in v1.2.0

#### OpenTherm v4.2 alignment and protocol fixes

- Added missing OT message IDs: `39`, `93`, `94`, `95`, `96`, `97`.
- Corrected OT direction flags for IDs `4`, `27`, `37`, `38`, `98`, `99`, `109`, `110`, `112`, `124`, `126`.
- Corrected v4.2 type/byte semantics for IDs `38`, `71`, `77`, `78`, `87`, `98`, `99`.
- Corrected data semantics/units:
  - `FanSpeed` (ID `35`) handled as `u8/u8` and `Hz`
  - `RelativeHumidity` (ID `38`) handled as `f8.8`
  - `DHWFlowRate` unit updated to `l/min`
- Added compatibility profile for legacy pre-v4.2 IDs `50-63`:
  - `AUTO` (default runtime behavior in code): keep legacy decoding until OT v4.x is detected, then suppress reserved IDs `50-63`
  - `V4X_STRICT`: always suppress
  - `PRE_V42_LEGACY`: always decode/publish
- Added missing `getOTGWValue()` mappings for IDs `113` and `114`.

### Runtime safety and correctness hardening

- Added OT map bounds checks before indexed lookups in parser and REST paths.
- Unknown IDs now use safe fallback metadata instead of raw map indexing.
- `sendOTGWvalue(int msgid)` now returns explicit out-of-range errors before OT map access.
- Reused global `cMsg` buffer in settings POST parsing (`restAPI.ino`) to avoid extra temporary stack buffer allocation.

### MQTT / Home Assistant fixes and improvements

- Fixed MQTT topic typos:
  - `eletric_production` -> `electric_production`
  - `solar_storage_slave_fault_incidator` -> `solar_storage_slave_fault_indicator`
- Fixed label typo: `Diagonostic_Indicator` -> `Diagnostic_Indicator`.
- Fixed HA discovery mismatches:
  - `vh_configuration_*` now keyed to message ID `74`
  - `Hcratio` HA `stat_t` now points to `Hcratio`
  - `FanSpeed` HA discovery split into `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` (`Hz`)
- v4.2 MQTT publishing alignment:
  - IDs `71`, `77`, `78`, `87` publish canonical single-byte base topics and retain legacy `_hb_u8` / `_lb_u8` aliases
  - IDs `98`, `99` publish semantic decoded topics and keep raw byte aliases
  - ID `38` (`RelativeHumidity`) publishes canonical `f8.8` payload instead of legacy split-byte topics

### New configurable source-separated MQTT publishing (Issue #143)

- Added `MQTTseparatesources` setting (REST + persisted settings support).
- Firmware can publish source-specific topics per OpenTherm source using nested paths (`<metric>/thermostat`, `<metric>/boiler`, `<metric>/gateway`) while retaining legacy unsuffixed topics for compatibility.
- HA discovery generation supports source-specific templates (`%source_suffix%`, `%source_topic_segment%`, `%source_name%`) for split entities when enabled.
- MQTT publish helpers were refactored for clearer source mapping resolution and safer reuse.

### MQTT auto-configuration robustness (HA discovery)

- `doAutoConfigure()` and `doAutoConfigureMsgid()` now share a single static buffer workspace to avoid duplicate persistent RAM reservations.
- Added a scoped re-entry lock to prevent autoconfig buffer clobbering.
- Lock scope was narrowed so Dallas sensor discovery (`configSensors()` path) can still run correctly.
- `doAutoConfigure()` skips Dallas placeholder lines in the main file loop and triggers Dallas discovery separately when needed.
- Auto-config state tracking logic was cleaned up (`getMQTTConfigDone` / `setMQTTConfigDone`) to reduce duplicate or skipped work.

### Gateway mode detection / API / UI fixes

- Gateway mode parsing fixed to handle actual `PR=M` response format (`M=G` / `M=M`) instead of assuming a single char.
- Gateway mode now tracks a known/unknown state (`bOTGWgatewaystateKnown`) instead of defaulting to a false/monitor-like value.
- MQTT gateway-mode state is only published after a successful mode read.
- Device-info API field was standardized to `otgwmode` and can return `ON`/`OFF` or `detecting`.
- Device-info key `wifiqualitytldr` was renamed to `wifiquality_text` for clearer semantics.
- Web UI gateway indicator now supports `Gateway`, `Monitor`, `Detecting...`, and `Unavailable`, while preserving the last known mode when refreshes fail.
- Gateway mode polling/refresh flow was cleaned up and moved into a 60-second task path.
- Debug output text was updated for clearer gateway mode reporting.

### PS mode detection and UI message handling improvements

- Firmware now auto-detects `PS=1` mode from summary `key=value` stream lines and auto-detects `PS=0` when raw OT frames resume.
- Web UI footer messaging was refactored to consistently show PS mode state and version warnings.
- Sensor simulation state is surfaced in the footer message but hidden from the main OT monitor table.
- PS mode footer styling (watermark emphasis) added in both light and dark themes.

### Serial robustness and diagnostics

- Increased OT serial read line buffer limit from `256` to `512` bytes (PS=1 summary lines can exceed 256 bytes).
- Improved serial overflow handling:
  - drop the current line after overflow
  - wait for line terminator before resuming (no partial/corrupted forwarding)
- Added dropped-line tracking counter for overflow-related line drops.
- Expanded WebSocket event logging for OTGW activity using prefixed event lines (`>`, `<`, `!`, `*`):
  - sent commands
  - command responses
  - OTGW error/status responses (`NG`, `SE`, `BV`, `OR`, `NS`, `NF`, `OE`, Error 01-04)
  - queue drops and reset actions (`GW=R`)
  - PS mode transitions
  - serial overflow notifications
  - PIC restart banner detection
  - ser2net-injected OTGW commands
- Time-command debug logging was clarified to avoid duplicate/noisy messages.

### Web UI / mobile / flash UX improvements

- Added shared page-navigation template shell for multiple pages (`Home`, `Settings`, `Advanced`, etc.) to reduce duplication.
- Added `index_common.css` for shared responsive/mobile behavior across light/dark themes.
- Improved mobile responsiveness (notably at `<= 768px`):
  - stacked navigation layout
  - better settings form layout
  - improved OT log controls on small screens
  - larger tap targets and more consistent spacing
- Settings UI markup improved (`label for=...` usage and input container class cleanup).
- Device-info display formatting helpers added (including gateway mode formatting and case-insensitive label translation fallback).
- OT log rendering improved:
  - frozen viewport position when auto-scroll is disabled
  - avoids unnecessary DOM rewrites when content has not changed
- OT log buffer (including persisted `localStorage` logs) is now cleared after firmware/filesystem flash completion.
- PIC/filesystem flash page UX improved with clearer backup checkbox help text and stronger submit button states/styles.

### Developer tooling / CI / documentation

- Added spec-driven OpenTherm v4.2 audit tool: `tools/opentherm_v42_spec_audit.py`.
- Added CI workflow to run the OT v4.2 spec audit and upload matrix/report artifacts.
- ADR compliance workflow GitHub Script step now uses environment variables (fixes/avoids quoting/syntax issues in PR comments).
- Added/updated supporting docs:
  - `docs/fixes/opentherm-v42-mqtt-breaking-changes.md`
  - OpenTherm v4.2 compliance review docs (`docs/reviews/2026-02-15_opentherm-v42-compliance/`)
  - Issue #143 source-separation options analysis (`docs/reviews/2026-02-20_issue-143-source-separation/ISSUE_143_OPTIONS_ANALYSIS.md`)
  - REST API documentation updated to v2-only (`docs/api/README.md`, `docs/api/openapi.yaml`)
- Repository hygiene cleanup:
  - removed accidental artifact file `tmpclaude-ecc0-cwd`
  - ignore Claude local settings artifact path in `.gitignore`

### Webhook support (new in this branch)

A new webhook feature allows the firmware to make an outbound HTTP call when a configurable OpenTherm status bit changes.

- Triggered by any configurable OpenTherm status bit (default: bit 1 — flame on/off).
- Separate URL, payload body, and content type for the "on" and "off" event.
- URLs are restricted to the local network (`isLocalUrl()` — ADR-003/ADR-032); external URLs are rejected.
- Disabled by default (`settingWebhookEnabled = false`). Settings are persisted alongside other device settings.
- New settings: `webhookEnabled`, `webhookURLon`, `webhookURLoff`, `webhookTriggerBit`, `webhookPayload`, `webhookContentType`.
- Test endpoint: `GET /api/v2/webhook/test` triggers a test call to verify the configured URL is reachable.

### v0 and v1 REST API removed (new in this branch)

The v0 and v1 REST API versions have been removed. Any request to `/api/v0/` or `/api/v1/` now returns **410 Gone**.

- Only `/api/v2/` endpoints remain — see [docs/api/README.md](docs/api/README.md) for the full reference.
- ArduinoJson dependency removed; settings I/O replaced with lightweight streaming helpers (`wStrF`, `wBoolF`, `wIntF`, `parseSettingsLine()`), reducing both flash and RAM usage.
- All in-firmware references to v1 endpoints (OTA health polling, sensor label restore) updated to v2.
- OpenAPI specification and API documentation updated to v2-only.

### Bug fixes (new in this branch)

- **`MQTTseparatesources` not persisted**: The setting was written to `settings.ini` by `writeSettings()` but was absent from `applySettingFromFile()`, causing it to silently reset to `false` on every reboot. Fixed in `settingStuff.ino`.

---

### New in v1.1.0

#### Dallas DS18x20 temperature sensor improvements

- DS18x20 sensors now support **custom labels** editable inline in the Web UI.
- Labels are stored in `/dallas_labels.ini` with zero backend RAM usage.
- Sensors automatically published to MQTT with HA auto-discovery (sensor name uses custom label when set).
- New bulk REST API: `GET /api/v2/sensors/labels` and `POST /api/v2/sensors/labels`.
- Automatic label backup/restore during filesystem flash operations.
- Real-time graph visualization for Dallas sensors with 16-color palette.

#### RESTful API v2

- 13 new v2 endpoints with consistent JSON error responses, proper HTTP status codes (202 Accepted for async), CORS/OPTIONS preflight support, and RESTful resource naming.
- New endpoints: `GET /api/v2/device/info`, `GET /api/v2/device/time`, `POST /api/v2/otgw/commands`, `POST /api/v2/otgw/discovery`, `GET /api/v2/otgw/messages/{id}`, `GET /api/v2/firmware/files`, `GET /api/v2/filesystem/files`, etc.
- Full OpenAPI specification for all v2 endpoints in `docs/api/openapi.yaml`.
- Main Web UI migrated to v2 API — a few auxiliary flows (such as OTA health/label restore) still use v1 endpoints.
- API compliance score improved from 5.4 → 8.5/10.
- See [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md).

#### Memory and performance improvements

- **Streaming file serving**: Replaced full-file-to-RAM loading with chunked streaming — 95% memory reduction for serving Web UI files. This resolves the slow UI loading reported on v1.0.0.
- Settings save reduced from 20 flash writes to 1 via deferred flush with 2-second debounce — reduces flash wear significantly.
- Heap memory 4-level health system (CRITICAL/WARNING/LOW/HEALTHY) with adaptive throttling and WebSocket backpressure control.

#### Web UI improvements

- **Non-blocking modal dialogs**: Custom HTML/CSS modals replace blocking `prompt()`/`alert()` calls, maintaining real-time WebSocket data flow during user input.
- **PS mode (`PS=1`) auto-detection**: Automatic detection from the OTGW PIC. When active, the UI hides the OT log section, disables WebSocket streaming, and suppresses time-sync commands — improving compatibility with legacy integrations (e.g. Domoticz).
- **WebUI data persistence**: Automatic log data persistence to `localStorage` with debounced 2-second saves, dynamic memory management, and auto-restoration on page load.
- **Browser debug console (`otgwDebug`)**: Full diagnostic toolkit accessible in browser console — `status()`, `info()`, `settings()`, `wsStatus()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`.

#### Bug fixes (20 findings from comprehensive codebase review)

- **Memory safety**: Out-of-bounds array write on OT message ID 255; stack buffer overflow in hex parser; year overflow in date handling.
- **Data integrity**: Wrong bitmask corrupting MQTT hours 16–23; disconnected Dallas sensor (-127°C) published to MQTT.
- **Concurrency**: ISR race conditions in S0 pulse counter causing incorrect energy readings.
- **Security**: Reflected XSS in REST API error pages; dead admin password code removed.
- **Reliability**: File descriptor leak in filesystem handler; null pointer crash on malformed MQTT topics; 750ms blocking sensor read replaced with non-blocking; HTTP client resource leak.
- **Feature fix**: GPIO outputs gated by debug flag — the feature was completely non-functional before this fix.
- **MQTT whitespace auth fix**: Automatic trimming of whitespace in MQTT credentials, fixing authentication failures when upgrading from v0.10.x.

Full details in [Codebase Review](docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md).

---

## Breaking Changes / Migration Notes

### MQTT / Home Assistant (OpenTherm v4.2 alignment)

Manual MQTT consumers and older HA entities may need updates:

- `eletric_production` -> `electric_production`
- `solar_storage_slave_fault_incidator` -> `solar_storage_slave_fault_indicator`
- `CumulativElectricityProduction` -> `CumulativeElectricityProduction`
- `vh_free_ventlation_mode` -> `vh_free_ventilation_mode`
- `vh_ventlation_mode` -> `vh_ventilation_mode`
- `vh_tramfer_enble_nominal_ventlation_value` -> `vh_transfer_enable_nominal_ventilation_value`
- `vh_rw_nominal_ventlation_value` -> `vh_rw_nominal_ventilation_value`
- `RelativeHumidity_hb_u8` / `RelativeHumidity_lb_u8` (legacy split-byte decoding) -> `RelativeHumidity` canonical `f8.8` payload
- HA discovery `FanSpeed` (`rpm`) -> `FanSpeed_setpoint_hz` + `FanSpeed_actual_hz` (`Hz`)
- Legacy IDs `50-63` now suppressed on v4.x systems in default `AUTO` compatibility mode
- Source-specific MQTT and HA discovery paths now use nested `<metric>/<source>` segments instead of `<metric>_<source>`

Example source-path migrations (when `mqttseparatesources=true`):

| Previous source-specific topic/path | New topic/path |
|-----------|-----------|
| `.../value/<node>/TSet_thermostat` | `.../value/<node>/TSet/thermostat` |
| `.../value/<node>/Tr_boiler` | `.../value/<node>/Tr/boiler` |
| `.../value/<node>/Toutside_gateway` | `.../value/<node>/Toutside/gateway` |
| `homeassistant/sensor/<node>/MaxRelModLevelSetting_thermostat/config` | `homeassistant/sensor/<node>/MaxRelModLevelSetting/thermostat/config` |

Compatibility retained:

- IDs `71`, `77`, `78`, `87` keep `_hb_u8` / `_lb_u8` alias topics alongside spec-correct base topics.
- IDs `98`, `99` keep raw byte alias topics and add semantic decoded topics.
- Legacy unsuffixed MQTT topics remain published when source separation is enabled (source-specific topics are additive).
- Source-specific `uniq_id` values remain suffix-based (for example `...-TSet_thermostat`) to reduce HA entity-registry churn after rediscovery.

### Device info API payload changes (raw consumers)

If you parse device info JSON directly (instead of the Web UI), update these keys:

- `gatewaymode` / temporary `mode` usage -> `otgwmode`
- `wifiqualitytldr` -> `wifiquality_text`

`otgwmode` can be `ON`, `OFF`, or `detecting`.

### After upgrading

1. Clear retained MQTT discovery topics (`homeassistant/.../config`) for this device/prefix if you previously used source-separated topics; older retained paths can remain visible after upgrade.
2. Optionally clear retained legacy source-specific value topics (underscore format) if you no longer need them in MQTT Explorer/history views.
3. Trigger MQTT auto-discovery again (especially if using HA entities for `FanSpeed`, source-separated entities, or v4.2-affected IDs).
4. Remove stale HA entities linked to typo topics, old `FanSpeed` discovery, or old source-specific discovery paths.
5. Update manual MQTT automations/sensors to new topic names and payload formats (including typo-fix renames like `CumulativeElectricityProduction`, `vh_*_ventilation_*`, and nested source paths such as `TSet/thermostat`).
6. If you rely on legacy IDs `50-63`, confirm the system is truly pre-v4.2.
7. If custom tooling reads `/api/.../device/info`, update field names to `otgwmode` and `wifiquality_text`.

Detailed OpenTherm MQTT/HA migration guidance: `docs/fixes/opentherm-v42-mqtt-breaking-changes.md`

---

## Validation Basis

This summary was compiled from the git delta between `v1.0.0` (tag `v1.0.0`, commit `c03a635`, released 2026-02-08) and the current `dev` branch head. Functional changes were derived from commit history and file diffs across firmware, Web UI, MQTT/HA config, CI workflows, and documentation.
