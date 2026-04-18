v1.4.0-beta is a minor-version release focused on a modern toolchain, a rewritten MQTT Home Assistant discovery pipeline, and ESP8266 memory / stability work. No breaking changes vs v1.3.5 for default ESP8266 builds.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.4.0.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [Compare v1.3.5...v1.4.0](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.5...v1.4.0)

## Highlights

- **ESP8266 Arduino core 3.1.2** (was 2.7.4), **AceTime 4.1.0** (was 2.0.1), **TelnetStream 1.3.0**, and migration from ESPTelnet/TelnetStream to the new **SimpleTelnet** library with callback-driven debug input.
- **Async bitmap-driven MQTT HA discovery (#547).** Synchronous inline discovery is gone. A drip publisher drains one pending ID every ~3 seconds (30 s under heap pressure), so the OT message loop is never blocked and HA restarts no longer trigger a 60+ message burst.
- **Streaming discovery API.** Legacy `MqttHaSpecialCfg` templates and PROGMEM blobs removed. Climate, number, sensor, and Dallas discovery all go through one compact streaming path. Comprehensive icon heuristics live in one place.
- **Configurable HA device manufacturer/model** (`settings.device.sManufacturer` / `sModel`, defaults `NodoShop` / `OTGW`). Hardcoded `"Schelte Bron"` / `"otgw-nodo"` constants removed; Schelte Bron is now credited on a dedicated `otgw-pic/designer` MQTT topic.
- **Opt-in scheduled nightly restart** (`settings.bNightlyRestart` + `iRestartHour`, default off, hour 4). Respects NTP sync, timezone, and a 1-hour uptime minimum.
- **REST API:** new `GET /api/v2/sensors/status` for current Dallas readings. One-line access log with HTTP status and handler timing.
- **Debug ergonomics:** MQTT debug flag split into key 3 (communication) and key 5 (gating); NTP telemetry behind key 6 (on by default). Noisy `skip`/`onNotFound`/`setMQTTConfigDone` lines suppressed.
- **Memory and stability:**
  - `sLine[1200]` and `topicBuf[200]` staging buffers eliminated (TASK-276/277).
  - `WiFiClient::setSync(true)` + `setNoDelay(true)` save ~1 KB of TCP buffer copy per connection.
  - `O(n²)` strlen scanning removed from the `AddLog` macros.
  - PROGMEM pool linkage validation guard; Exception (3) autodiscovery crash fixed.
  - Reboot-loop fix (heap guard + `bConnected` check) on early beta builds.
  - Serial-overrun suppression on MQTT publish errors.

## Bug fixes

- Exception (3) crash in MQTT autodiscovery caused by a PROGMEM-as-RAM pointer (followed up with a PROGMEM pool linkage guard).
- Reboot loop seen on the first 1.4.0-beta builds after a fresh flash.
- `F` (force discovery) key now clears the done bitmap and actually re-publishes everything.
- LittleFS image size mismatch under ESP8266 core 3.x; missing null-checks on LittleFS file opens in the web server path.
- MQTT status gate logic corrected; spammy "skip" debug lines and burst on MQTT reconnect removed.
- Serial overrun on MQTT disconnect eliminated by dropping per-publish error logs.
- Dallas sensor ID 246 removed from the static PROGMEM discovery array.

## Developer previews (not active in default builds)

- **SAT (Smart Autotune Thermostat)** source modules (PID, cycles, pressure, weather) are now in the tree behind `#if defined(ENABLE_SAT)`. `ENABLE_SAT` is **not** set in the default build, so SAT is shipped as source only. Target for the next release cycle.
- New `/flash` skill for automated build + flash during development.

## Upgrade notes

- **Flash both firmware and filesystem.** The discovery rewrite changes internal bitmaps and debug keys, and the new ESP8266 core needs the matching LittleFS image.
- Discovery is now re-published gradually on boot; stale HA entities from earlier experiments can be cleaned up once discovery settles.
- **No breaking changes vs v1.3.5**: MQTT topics, REST API, and settings JSON are preserved. New settings (`nightlyrestart`, `nightlyrestarthour`, `device.manufacturer`, `device.model`) all have defaults.
- **This is a beta.** We want a couple of weeks of field testing before promoting the tag to a stable `v1.4.0`. Please report issues on GitHub or on Discord.

## Known limitations

- SAT is not compiled into the shipped binary (`ENABLE_SAT` off).
- ESP32 and PlatformIO builds are **not** supported in this release. Any earlier documentation suggesting otherwise was aspirational; this firmware is ESP8266-only.

## Thanks

Thanks to the Discord testers who ran early builds of the discovery rewrite and the core 3.1.2 upgrade, and who caught the first-flash reboot loop quickly. Special thanks to **Schelte Bron** for the OpenTherm Gateway hardware and PIC firmware that this project builds on — now credited explicitly on the `otgw-pic/designer` MQTT topic and in the default HA device registry values.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.3.5](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.5)
