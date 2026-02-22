# Release Notes — v1.2.0-beta

**Last updated:** 2026-02-22<br>
**Release branch:** `dev-1.2.0-stable-version`<br>
**Comparison target:** `dev` at `v1.1.0-beta` release commit `0a86aa7`<br>
**Analyzed head:** `ea69853` (local branch; `origin/dev-1.2.0-stable-version` is ahead by 1 CI `version.h` commit `fbd66df`)<br>

---

## Overview

Version `1.2.0-beta` (branch line `dev-1.2.0-stable-version`) builds on the `dev` `v1.1.0-beta` baseline and adds a broader set of changes than the earlier beta-branch notes captured:

- OpenTherm v4.2 protocol map alignment and reserved-ID compatibility handling
- Runtime safety hardening in OT map parsing and REST lookups
- MQTT/Home Assistant discovery and topic correctness fixes
- Configurable source-separated MQTT publishing + HA discovery templates
- Gateway-mode detection/API/UI reliability improvements
- Serial handling robustness and WebSocket event logging improvements
- Web UI/mobile layout and flash UX refinements
- Spec-audit tooling/CI and supporting documentation

---

## Summary of Features and Fixes (v1.2.0 branch)

### OpenTherm v4.2 alignment and protocol fixes

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
- Repository hygiene cleanup:
  - removed accidental artifact file `tmpclaude-ecc0-cwd`
  - ignore Claude local settings artifact path in `.gitignore`

---

## Breaking Changes / Migration Notes

### MQTT / Home Assistant (OpenTherm v4.2 alignment)

Manual MQTT consumers and older HA entities may need updates:

- `eletric_production` -> `electric_production`
- `solar_storage_slave_fault_incidator` -> `solar_storage_slave_fault_indicator`
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
5. Update manual MQTT automations/sensors to new topic names and payload formats (including nested source paths such as `TSet/thermostat`).
6. If you rely on legacy IDs `50-63`, confirm the system is truly pre-v4.2.
7. If custom tooling reads `/api/.../device/info`, update field names to `otgwmode` and `wifiquality_text`.

Detailed OpenTherm MQTT/HA migration guidance: `docs/fixes/opentherm-v42-mqtt-breaking-changes.md`

---

## Validation Basis

This summary was compiled from the git delta:

- baseline: `0a86aa7` (`dev` v1.1.0-beta release notes/version update commit)
- analyzed head: `ea69853` (`dev-1.2.0-stable-version` local)
- note: remote branch is ahead by one CI-only `version.h` commit (`fbd66df`)

Functional changes were derived from commit history and file diffs across firmware, Web UI, MQTT/HA config, CI workflows, and documentation.
