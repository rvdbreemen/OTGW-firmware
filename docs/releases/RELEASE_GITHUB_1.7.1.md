Home Assistant integration release for the 1.x (ESP8266) line: cooling support in the climate entity, plus new gateway and device-health auto-discovery sensors. No breaking changes versus v1.7.0.

Full notes: [RELEASE_NOTES_1.7.1.md](https://github.com/rvdbreemen/OTGW-firmware/blob/v1.7.1/RELEASE_NOTES_1.7.1.md) . README: [README.md](https://github.com/rvdbreemen/OTGW-firmware/blob/v1.7.1/README.md) . Compare: [v1.7.0...v1.7.1](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.0...v1.7.1)

## New features

- **Unified off/heat/cool climate entity (cooling support).** The Home Assistant MQTT climate entity now models `off` / `heat` / `cool`, driven by new `hvac_mode` and `hvac_action` topics from the OpenTherm status bits. Cooling-capable systems (for example a Honeywell Round Modulation Heat/Cool on a heatpump) no longer show as heating-only. `hvac_action` reads the central-heating bit, not flame, so DHW draws do not read as heating. (GH #665)
- **HVAC Mode and HVAC Action sensors.** The climate topics are also exposed as two standalone discoverable sensors: `hvac_mode` (off/heat/cool) and `hvac_action` (off/idle/heating/cooling).
- **Gateway mode and OTGW-connected binary-sensors.** See gateway/monitor mode and the OTGW-to-PIC link state in Home Assistant, index-gated so they publish once and self-heal.
- **Device-health sensors.** Uptime, unsupported-message-id count, and the heap-fragmentation back-off counters (`http_fragskips` / `mqtt_fragskips` / `ws_fragskips`) are now auto-discovery sensors.

## Bug fixes

- Fixed a compile break (missing `HaUnit::s` seconds unit) that kept the previous betas from shipping. The new discovery entities reach devices for the first time in this release.
- Corrected a stale Mosquitto winget package ID in the `capture-mqtt-debug` diagnostic helper. (GH #666)

## Upgrade notes

Flash both firmware and filesystem (OTA via web UI, or the merged binary over USB), then let Home Assistant re-run MQTT discovery. New entities appear under the existing OTGW device. Heatpump and Heat/Cool users get the unified climate entity automatically.

## Thank you

Special shoutout to **jelvank** for the detailed Heat/Cool report and cooling captures (GH #665) that made the unified climate entity possible.

Thanks to everyone who helped through reports, testing, and feedback:
- **jelvank** (GitHub) reported the Honeywell Round Modulation Heat/Cool behaviour with logs that drove the design
- **ties7944** (Discord) flashed beta.6 and confirmed the new uptime sensor
- **Crash-Evans** (GitHub, PR #664) brought macOS/Linux capture diagnostics to parity and ran validation logs
- **Pistoletjes** (GitHub #666) reported the Mosquitto winget install failure
- **geo83_44083** (Discord) provided sustained stability testing and captures across the 1.7.x line

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
