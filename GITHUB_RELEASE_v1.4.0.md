## v1.4.0 -- SAT Embedded Thermostat, ESP32 Support, WiFi Resilience

A major feature release. SAT (Smart Autotune Thermostat) turns the OTGW into a standalone smart heating controller. ESP32 support lands alongside the existing ESP8266 target through a unified platform abstraction layer. WiFi reconnect is now non-blocking and MQTT publishes uptime and firmware version. **No breaking changes** for existing ESP8266 users.

### SAT (Smart Autotune Thermostat)

An embedded heating controller that runs entirely on the ESP, independent of Home Assistant or any external automation platform.

- **Weather-compensated heating curve** with PID v3 controller and automatic gain tuning
- **Dual control modes**: continuous modulation (`CS=` setpoint override) and PWM flame cycling for boilers that don't modulate well at low demand
- **Automatic mode switching**: cycle tracker classifies flame behavior and switches between PWM and continuous based on sustained overshoot or underheat
- **Six independent safety layers**: boot safety, disable safety, stale sensor fallback, hard temperature ceiling (50C underfloor / 80C radiator), consecutive skip counter, and PIC comm failure detection
- **Radiator and underfloor heating** support with system-appropriate temperature limits
- **Full observability**: Web UI dashboard tab, REST API (`/api/v2/sat/*`), MQTT publish/subscribe, and Home Assistant auto-discovery (climate entity + 7 sensors)
- **External sensors optional**: works with OT bus data alone; MQTT/REST indoor and outdoor temperature inputs enhance accuracy when available

Documentation: [Feature guide](docs/features/smart-autotune-thermostat.md) | [ADR-062](docs/adr/ADR-062-sat-smart-autotune-thermostat-integration.md)

### WiFi and MQTT improvements

- **Non-blocking WiFi reconnect:** The blocking 30-second reconnect loop is replaced with a cooperative state machine (`loopWifi()`), preventing main-loop freezes on a heating system controller. Timeout tuned to avoid premature reboots on slow networks.
- **MQTT uptime publishing:** Uptime (seconds) is published to `otgw-firmware/uptime`, improving device observability and enabling uptime-based automations.
- **MQTT firmware version publishing:** Firmware version string is published to `otgw-firmware/version` after connect, making it easy to track which firmware is running across multiple devices.

### ESP32 and platform improvements

- **ESP32 support (experimental)** — 30+ compile-time platform shim functions abstract away ESP8266/ESP32 SDK differences. The firmware now compiles for both architectures from one source tree. Documented in [ADR-061](docs/adr/ADR-061-unified-esp8266-esp32-platform-abstraction.md).
- **PlatformIO build system** — New `platformio.ini` with `esp8266` and `esp32` environments. Build with `pio run -e esp8266` or `pio run -e esp32`.
- **Board GPIO definitions** — Auto-detected pin mappings per platform via `boards.h` (`PIN_LED1`, `PIN_BUTTON`, `PIN_I2C_SCL`, etc.).
- **ESP32 OTA update server** — Dedicated web-based firmware update implementation for ESP32.
- **Unified directory iteration** — `PlatformDir` class wraps both ESP8266 and ESP32 filesystem directory APIs behind a single interface.
- **OpenTherm enum modernization** — Binary literals updated from `B000` to `0b000` (C++14 standard) for better compiler compatibility.
- **Serial error detection** — New `platformSerialHasOverrun()` / `platformSerialHasRxError()` abstractions for cross-platform serial diagnostics.

### Platform abstraction

The abstraction layer centralizes all ESP8266/ESP32 API differences into inline wrapper functions selected at compile time:

| Function | Purpose |
|----------|---------|
| `platformFreeHeap()`, `platformMaxFreeBlock()` | Unified memory queries |
| `platformChipId()`, `platformGetMacAddress()` | Chip identity |
| `platformFSInfo()` | Filesystem statistics |
| `platformSetHostname()`, `platformGetHostname()` | Network configuration |
| `platformRtcRead/Write()` | RTC user memory (NVS-emulated on ESP32) |
| `platformResetReason()`, `platformResetCode()` | Reset diagnostics |
| `platformSerialHasOverrun()`, `platformSerialHasRxError()` | Serial error detection |

### ESP32 status

ESP32 support is functional but experimental in this release:

- Pin definitions are provisional until NodoShop ESP32 hardware is finalized.
- RTC user memory is emulated via NVS Preferences (flash-backed, limited write endurance).
- Serial overrun/error detection is not available on ESP32 (returns false).
- ESP32 provides significantly more resources: ~320KB usable RAM, dual-core 240 MHz, larger flash partitions.

### Upgrade Notes

1. Flash **both firmware and filesystem**.
2. Hard-refresh the browser after flashing (Ctrl+F5).
3. Existing ESP8266 users: no configuration changes needed. The ESP8266 build is functionally identical to v1.3.4.

Full release notes: [RELEASE_NOTES_1.4.0.md](RELEASE_NOTES_1.4.0.md)

---

### Thank you

This release would not have been possible without the community. Thank you to everyone who tested, reported issues, and pushed for improvements. The Discord community continues to be an amazing source of feedback, testing, and ideas.

Join the community on Discord: https://discord.gg/zjW3ju7vGQ

If you want to support the project: [Buy me a coffee](https://www.buymeacoffee.com/rvdbreemen)
