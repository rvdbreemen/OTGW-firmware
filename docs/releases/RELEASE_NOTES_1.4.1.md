# OTGW-firmware v1.4.1 Release Notes

**Release date:** 2026-04-21
**Branch:** main (from dev)
**Compare:** [v1.3.5...v1.4.1](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.5...v1.4.1)

## Overview

v1.4.1 is the first public release in the 1.4.x series. Development on this branch started after v1.3.5 and accumulated enough scope that a 1.4.0 milestone was named internally, but it was never published as a standalone release. v1.4.1 ships the complete 1.4.x body of work in one release.

If you are upgrading from v1.3.5, you get everything below. There is no v1.4.0 to install or skip.

There are no breaking changes vs v1.3.5. All new behaviour is additive, with conservative defaults chosen so existing integrations keep working untouched.

---

## WARNING: Flash filesystem FIRST, then firmware when upgrading to v1.4.x

**If you are upgrading to v1.4.1 from any earlier version, read this before you flash anything.**

The Arduino Core 3.1.2 upgrade changed the LittleFS partition size from 1 MB to 2 MB. All versions before v1.4.x shipped with Arduino Core 2.7.4 and a 1 MB filesystem. This changes the correct upgrade procedure.

**Correct order: filesystem binary first, firmware binary second. Your settings are preserved.**

1. Download both `OTGW-firmware-1.4.1.ino.bin` and `OTGW-firmware-1.4.1.littlefs.bin` from this release.
2. Flash the **filesystem binary first** via the Web UI update page.
3. Flash the **firmware binary second**, immediately after.
4. Hard-refresh the browser (Ctrl+F5).

**What happens if you flash the firmware first (the wrong order)?**

If you mistakenly flash the firmware binary before the filesystem, the new firmware boots against the old 1 MB filesystem layout at the wrong partition offset. It then spends approximately 5 to 10 minutes reformatting the new 2 MB partition on first boot. During this time the device is completely unresponsive: the web UI is unreachable and MQTT stays offline. After the reformat, all your settings are gone and you will need to re-enter your MQTT broker, credentials, hostname, and every other setting once more.

**Flashing the filesystem binary first avoids this entirely. Your settings are preserved and no reformat is triggered.**

---

## New features and improvements since v1.3.5

### SimpleTelnet: cleaner debug console

The firmware's debug port migrated from TelnetStream/ESPTelnet to the SimpleTelnet library. The change brings a formatted welcome banner on connect, structured debug-key dispatch, and a more reliable telnet session teardown on the ESP8266. The welcome banner lists all available debug keys and their descriptions.

### Arduino Core upgrade to 3.1.2

The ESP8266 Arduino Core was upgraded from 2.7.4 to 3.1.2. This brings updated lwIP (2.2.0), improved WiFi driver stability, and correct PROGMEM pointer alignment enforcement. The upgrade required a systematic PROGMEM safety audit: `strncmp_P`/`strstr_P` calls that were safe under 2.7.4's lenient alignment are replaced with byte-safe helpers (`pgm_strncmp_PP`, `pgm_read_char`) under 3.1.2. The LittleFS image size was also corrected for the 3.x block layout.

### MQTT HA discovery: full streaming rewrite

The Home Assistant auto-discovery subsystem was rewritten from a static PROGMEM template array to a compact streaming API. Key changes:

- **309 discovery configs** across 80+ OpenTherm message IDs (climate, number, sensor, binary_sensor) emitted without the 1350-byte static staging buffer.
- Async bitmap-driven drip publisher: one config published per timer tick, with the per-ID done-bit preventing duplicates. The `F` debug key forces a full re-announce.
- PROGMEM pool linkage validation guard catches pool pointer mismatches at boot before they can cause Exception (3).
- Dallas sensor discovery at runtime from live sensor addresses, so the address list does not need to be hardcoded.
- Comprehensive icon heuristics replace the hand-maintained special-case list.

### WiFi reconnect hardening

The blocking 30-second WiFi reconnect loop introduced by a regression between v0.10 and v1.3.5 is fixed. The fix removes erroneous DHCP calls made while the station was still associated, which prevented re-acquiring an IP after a router reboot. The reconnect is non-blocking and correctly re-establishes the connection after an access-point reboot without an ESP reboot.

### Nightly restart with configurable hour

A scheduled nightly restart can be enabled via `MQTTrestartEnable` + `MQTTrestartHour` settings. The restart fires at the configured wall-clock hour (default 4:00). The check is wired to the unified `hourChanged` dispatcher (see below) so it does not drift across reboots.

### Configurable device manufacturer and model (Schelte Bron)

The MQTT device announcement now includes configurable manufacturer and model strings, editable from the MQTT Settings page. This makes the OTGW device entry in Home Assistant's device registry more descriptive. Credit to Schelte Bron for the contribution.

### NTP telemetry and debug toggle

NTP sync events and last-sync timestamps are now published over the debug telnet port when debug key 6 is active (on by default). The boot-time welcome banner notes key 6 status. A guard rejects the ESP8266 SDK's bogus initial `0xFFFFFFFF` timestamp so the NTP-last-sync field in the device info response is never populated with garbage.

### REST API additions

- `GET /api/v2/sensors/status` — returns current Dallas sensor readings and statuses.
- `GET /api/v2/discovery` — returns HA discovery state: published count, pending count, verification state.
- `POST /api/v2/discovery/verify` — starts an on-demand discovery verification window.
- `POST /api/v2/discovery/republish` — marks all configs pending and triggers a re-announce via the drip loop.
- Nightly restart settings (`MQTTrestartEnable`, `MQTTrestartHour`) exposed in the REST settings whitelist.

### WiFi SSID display and Reset WiFi button

The Settings page now shows the current WiFi SSID and includes a Reset WiFi button that clears stored credentials and reopens the captive portal, equivalent to the three-hardware-reset sequence.

### OpenTherm v4.2 alignment fixes

- IDs 58-69 (previously only 58-63) correctly treated as reserved in v4.x mode.
- `MaxTSet` and `TdhwSet` suppressed correctly in v4.x mode — they were showing as 0°C in Home Assistant.
- `WRITE_ACK` responses accepted as valid for `OT_WRITE` messages when populating the boiler source entities.
- OpenTherm Answer Thermostat messages (MsgID in the boiler-to-thermostat direction) published to the boiler MQTT source topic.

### Heap pressure reduction during HA discovery drip

Home Assistant auto-discovery publishes roughly 80 retained config messages after first boot. On ESP8266, those back-to-back publishes plus the regular Status-frame fan-out were the most common trigger for heap exhaustion and watchdog resets reported on Discord.

This release tightens the drip loop in several layers:

- **Drip interval slowed from 1 s to 2 s** under normal heap; slow-mode path from 30 s to 10 s. Full drip bounded at around 13 minutes worst-case.
- **HEAP_LOW is now the backoff trigger** — the drip backs off earlier and rarely approaches HEAP_CRITICAL.
- **Fragmentation-aware publish gates** use `getMaxFreeBlockSize()` in addition to raw free heap.
- **Heap guard thresholds lowered** based on CrashEvans' multi-day tester logs. `HEAP_CRITICAL`, `HEAP_WARNING`, and `HEAP_LOW` retuned with measured margin.
- **Status-burst cooldown window**: after an OpenTherm Status-frame fan-out (MsgID 0), the discovery drip holds off for 2 s so the two publishers do not compete for the same MQTT outbound buffer.
- **Hold-per-interval hysteresis**: drip mode can switch only after one full current-interval has elapsed, preventing oscillation between normal and slow mode under marginal heap conditions.
- **PIC quiesce during burst and drip tick**: `queryNextPICsetting()` skips a poll cycle when a Status burst is active or a drip tick is imminent, reducing mid-burst competition.

The net effect on a fresh flash is a slower but reliably-completing discovery cycle on ESP8266, measured on field devices that previously hit resets mid-discovery.

### Retained MQTT discovery verification and republish

ADR-062 introduces a self-heal mechanism for the retained HA discovery state on the broker:

- A node-scoped wildcard subscribe (`<haprefix>/+/<uniqueid>/+/config`) is opened for a short window.
- Incoming retained configs are counted against the per-source bitmap `MQTTautoConfigMap`.
- Topics the firmware believes are published but the broker does not echo back are marked pending and re-announced by the drip loop.
- The subscribe is torn down cleanly at window end.

**Daily auto-heal** via `MQTTdiscoveryAutoVerify` (default `true`). **On-demand** via REST, telnet `V` key, and MQTT telemetry.

### Hourly heap diagnostic MQTT topic

A retained topic `<topTopic>/value/<uniqueid>/otgw-firmware/stats/heap` is published once per hour. The 17-field JSON payload covers current/min/max/average free heap, largest contiguous block, cumulative tier-transition counters, dropped-publish counters, and discovery state. Because the topic is retained, any fresh subscriber can read the most recent snapshot without waiting for the next hour boundary. The `<uniqueid>` segment in the path ensures multiple OTGWs on the same broker never overwrite each other's stats.

### Unified time-boundary dispatcher

ADR-064 consolidates the firmware's four time-boundary helpers under a single caller contract. Exactly one dispatcher site in `OTGW-firmware.ino` calls `minuteChanged()` and fans out the hour/day/year transitions. All minute-aligned periodic work (nightly restart, daily discovery verify, hourly heap diagnostic publish) consumes boundary flags from the dispatcher instead of polling `minute()`/`hour()` directly.

---

## Behavioural notes for users

- **First-boot HA discovery now takes roughly 2x longer under normal heap**: about 80 IDs times 2 seconds equals around 3 minutes end-to-end. This is intentional. The old 1 s cadence completed faster on paper but stalled or reset partway through on devices under heap pressure.
- **Nightly restart**: still triggers at the configured `settings.iRestartHour`. Timing is now wall-clock aligned through the unified dispatcher instead of each site polling `minute() == 0` on its own.
- **Retained discovery auto-verify**: enabled by default. On shared brokers where a per-node wildcard subscribe is undesirable, set `MQTTdiscoveryAutoVerify` to `false`. On-demand verify via REST or telnet remains available either way.

## Known limits and trade-offs

- **VH burst-quiesce**: the heap-pressure improvements apply to non-VH Status fan-out paths immediately. VH-specific burst coverage is tracked in TASK-354, pending hardware validation.
- **Verify window heap budget**: the verify path enforces a minimum free-heap threshold before starting. Under sustained heap pressure the verify may be skipped; the next cycle retries.
- **ADR-062 CI gates**: instrumentation checks planned in TASK-364 are not wired into CI yet.

## Upgrade notes

### LittleFS partition size changed — filesystem flash is mandatory

The Arduino Core 3.1.2 upgrade changed the LittleFS partition size from 1 MB to 2 MB. **You must flash both binaries in the same session, filesystem first.**

See the WARNING section at the top of these release notes for the full details on what happens if you skip or reverse the order.

Correct procedure:
1. Download both `OTGW-firmware-*.ino.bin` and `OTGW-firmware-*.littlefs.bin` from the release.
2. Flash the **filesystem binary first** via the Web UI update page.
3. Flash the **firmware binary second**, immediately after via the same update page.
4. Hard-refresh the browser (Ctrl+F5).

Flashing in this order preserves your settings. Flashing the firmware first triggers a reformat on boot and all settings are lost.

### Other upgrade notes

- No settings migration required. The new `MQTTdiscoveryAutoVerify` setting defaults to `true`.
- If you run on a shared MQTT broker with tight wildcard ACLs, set `MQTTdiscoveryAutoVerify` to `false`. On-demand verify via REST or telnet remains available either way.

## Breaking changes

**LittleFS partition size changed from 1 MB to 2 MB**: flash the filesystem binary first, firmware second to preserve your settings. Flashing firmware first triggers a 5-10 minute unresponsive boot while the partition reformats, and all settings are lost. See the WARNING section above and [docs/BREAKING_CHANGES.md](../BREAKING_CHANGES.md) for the cumulative log.

All MQTT topics, REST API endpoints, and settings format are otherwise identical to `v1.3.5`.

## Architecture Decision Records

- [ADR-062: Retained discovery verification](../adr/ADR-062-retained-discovery-verification.md) (Accepted)
- [ADR-064: Time-boundary single-caller contract](../adr/ADR-064-time-boundary-single-caller-contract.md) (Accepted)

## Thank you

Special shoutout to **CrashEvans** for the multi-day log captures that made the heap threshold retuning possible and directly drove the HEAP_LOW/HEAP_WARNING/HEAP_CRITICAL calibration in this release. Field data beats armchair tuning every time.

Thanks to everyone on Discord who reported mid-discovery resets, tested beta builds, and shared telnet and MQTT logs that helped pinpoint the root causes.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.3.5](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.5)
