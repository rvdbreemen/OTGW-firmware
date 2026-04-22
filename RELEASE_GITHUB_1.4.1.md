v1.4.1 is the first public release in the 1.4.x series. A 1.4.0 milestone was tracked internally but was never published as a standalone release — v1.4.1 ships the complete body of work in one go. Upgrading from v1.3.5? You get everything below. There is no v1.4.0 to install or skip.

> **CRITICAL: Flash filesystem FIRST, then firmware.** The Arduino Core 3.1.2 upgrade changed the LittleFS partition from 1 MB to 2 MB. Flashing in the correct order (filesystem first, firmware second) preserves your settings. If you mistakenly flash the firmware first, the new firmware boots against the old 1 MB layout and spends 5-10 minutes reformatting the 2 MB partition on first boot — the device is unresponsive during that time and all settings are lost. Always flash the filesystem binary before the firmware binary.

Full release notes: [RELEASE_NOTES_1.4.1.md](RELEASE_NOTES_1.4.1.md)

## Bug fixes

- **WiFi reconnect after router reboot**: removes erroneous SDK DHCP calls while the station is still associated; devices no longer require an ESP reboot after the router comes back up (fixes #525, reported by @SpandrelPanel)
- **MaxTSet and TdhwSet showing 0°C in Home Assistant**: WRITE_ACK responses now accepted as valid for OT_WRITE messages; boiler source entities populate correctly
- **OpenTherm v4.2 reserved ID range**: extended from 58-63 to 58-69, aligning with the spec
- **PROGMEM-as-RAM crash (Exception 3)**: PROGMEM pool linkage validation guard added; byte-safe helpers replace unsafe `strncmp_P`/`strstr_P` calls after Arduino Core 3.1.2 alignment enforcement
- **NTP last-sync poisoned by SDK boot value**: bogus `0xFFFFFFFF` timestamp rejected; `NtpLastSync` initialises to 0 until a real NTP sync lands

## Improvements

- **Arduino Core 3.1.2**: updated lwIP (2.2.0), improved WiFi driver stability, systematic PROGMEM safety audit across the codebase
- **SimpleTelnet**: formatted welcome banner on debug connect, structured key dispatch, clean session teardown
- **MQTT HA discovery rewrite**: streaming, bitmap-driven drip API; 309 configs across 80+ OpenTherm message IDs without the 1350-byte static staging buffer; runtime Dallas sensor discovery; comprehensive icon heuristics
- **Heap-aware discovery drip**: 2 s normal / 10 s slow-mode; fragmentation-aware publish gates; Status-burst cooldown (2 s); hold-per-interval hysteresis to prevent mode oscillation. Eliminates mid-discovery watchdog resets on loaded gateways
- **Retained-discovery self-heal** (ADR-062): daily wildcard subscribe verifies broker state, re-announces missing configs. On-demand via REST (`/api/v2/discovery`), telnet `V` key
- **Hourly heap diagnostic MQTT topic**: `<topTopic>/otgw-firmware/stats/heap` (retained, 17-field JSON: heap levels, fragmentation, tier counters, discovery state)
- **Unified time-boundary dispatcher** (ADR-064): hour/day/year triggers wall-clock aligned through a single call site
- **Configurable device manufacturer/model** in MQTT device announcement (credit: Schelte Bron)
- **Nightly restart** with configurable hour, wired to the `hourChanged` dispatcher
- **REST API additions**: `GET /api/v2/sensors/status`, `GET /api/v2/discovery`, `POST /api/v2/discovery/verify`, `POST /api/v2/discovery/republish`

## Upgrade notes

1. Download both `OTGW-firmware-*.ino.bin` and `OTGW-firmware-*.littlefs.bin` from this release.
2. Flash the **filesystem binary first** via the Web UI update page.
3. Flash the **firmware binary second**, immediately after.
4. Hard-refresh the browser (Ctrl+F5).

Flashing in this order preserves your settings. No settings migration required. The new `MQTTdiscoveryAutoVerify` setting defaults to `true`. If you run on a shared MQTT broker with tight wildcard ACLs, set it to `false`.

## Thank you

A very special shoutout to **CrashEvans** (Discord) for being the backbone of the 1.4.x beta program. CrashEvans tested every single build from the earliest boot-looping alphas through to the final stable candidate, captured multi-day telnet and MQTT logs, provided serial exception dumps, and ran back-to-back builds at all hours to help pin down the heap pressure and PROGMEM crash root causes. The heap threshold retuning in this release is literally calibrated on CrashEvans' log data. The 1.4.x branch would not have shipped without that level of commitment. Thank you.

Thanks to everyone else who contributed:
- **@SpandrelPanel** (GitHub) — reported the WiFi reconnect regression (#525) with clear reproduction steps
- **@andrebrait** (GitHub) — reported MaxTSet/TdhwSet showing 0°C in HA (#445)
- **mikdasa** (Discord) — reported the MQTT broker boot-loop with Mosquitto logs and detailed traces
- **simontemplar** (Discord) — tested 1.4.x builds on production hardware and confirmed stability

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose and verify.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
