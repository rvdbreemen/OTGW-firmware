# OTGW-firmware v1.7.1 Release Notes

**Release date:** 2026-07-09
**Branch:** main (from otgw-1.x.x)
**Compare:** [v1.7.0...v1.7.1](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.0...v1.7.1)

## Overview

A Home Assistant integration release for the 1.x (ESP8266) line. The headline is cooling support: the MQTT climate entity now models off/heat/cool for heatpump and Heat/Cool-thermostat users. Alongside it, several new auto-discovery sensors surface gateway and device health directly in Home Assistant. No breaking changes versus v1.7.0.

## New features

- **Unified off/heat/cool Home Assistant climate entity (cooling support).** The MQTT climate ("Thermostat") entity now represents cooling. Its `modes` are `off` / `heat` / `cool`, driven by new `hvac_mode` and `hvac_action` topics computed from the OpenTherm status bits. Cooling-capable systems (for example a Honeywell Round Modulation Heat/Cool on a heatpump) previously showed as heating-only, carrying the cooling setpoint with no way to reflect cooling. Mode is reflective (the thermostat owns heat/cool switching), and `hvac_action` reads the central-heating bit rather than flame, so domestic-hot-water draws no longer show as heating. Validated against real gas-boiler (idle, DHW, active heating) and heatpump (cooling) captures. (GH #665, ADR-085)
- **HVAC Mode and HVAC Action as standalone discoverable sensors.** The climate entity's backing topics are also exposed as two standalone Home Assistant sensors: `sensor.<device>_hvac_mode` (off / heat / cool) and `sensor.<device>_hvac_action` (off / idle / heating / cooling). No new publish logic; the climate entity is unchanged. (TASK-941)
- **Gateway mode and OTGW-connected as Home Assistant binary-sensors.** Home Assistant can now see whether the gateway is in gateway or monitor mode, and whether the OTGW link to the PIC is up, without scraping the debug page. Both are gated by the discovery index so they publish once and self-heal.
- **Device-health auto-discovery sensors.** Uptime, unsupported-message-id count, and the per-path heap-fragmentation back-off counters (`http_fragskips` / `mqtt_fragskips` / `ws_fragskips`) are now Home Assistant sensors, so long-running-device health and back-off activity are visible without telnet.

## Bug fixes

- **Discovery sensors now actually reach devices.** A compile break kept the previous beta numbers from shipping: the discovery table referenced `HaUnit::s` (seconds), but that unit was never added to the `HaUnit` enum or its string mapper, so the firmware did not build. The `s` unit was added (uptime is published in seconds). The climate, gateway-mode, OTGW-connected and uptime/fragskip entities above therefore land on devices for the first time in this release.
- **Diagnostic capture tooling: Mosquitto install path.** The `capture-mqtt-debug` helper used a stale winget package ID for the Mosquitto MQTT client, so the automatic install step failed. The package ID was corrected. (GH #666, PR #671)

## Internal improvements

- macOS/Linux capture diagnostics brought to parity with the Windows helper. (PR #664)
- Stale Home Assistant discovery comments corrected (CCONOFF case, sensor count) for 1.x parity. (GH #672)

## Breaking changes

**No breaking changes versus v1.7.0.** All new entities are additive Home Assistant auto-discovery topics. No MQTT topic renames, REST API changes, or settings-format changes.

## Upgrade notes

Flash both the firmware and the filesystem (OTA via the web UI, or the merged binary over USB), then let Home Assistant re-run MQTT discovery. The new entities appear under the existing OTGW device. Heatpump and Heat/Cool-thermostat users get the unified off/heat/cool climate entity automatically once discovery re-runs.

## Thank you

Special shoutout to **jelvank** for the detailed Heat/Cool switching report and cooling captures (GH #665) that made the unified climate entity possible.

Thanks to everyone who contributed through reports, testing, and feedback:
- **jelvank** (GitHub) reported the Honeywell Round Modulation Heat/Cool behaviour with logs that drove the cooling design
- **ties7944** (Discord) flashed beta.6 and confirmed the new uptime sensor in Home Assistant
- **Crash-Evans** (GitHub, PR #664) brought the macOS/Linux capture diagnostics to parity and ran validation logs
- **Pistoletjes** (GitHub #666) reported the Mosquitto winget install failure in the capture helper
- **geo83_44083** (Discord) provided sustained stability testing and captures across the 1.7.x line

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose and verify.
