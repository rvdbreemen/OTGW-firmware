v1.3.0 is a major feature release: PIC gateway settings visibility, one-click GitHub OTA, safer upgrades, optional admin protection, fuller `PS=1` integration, and significant memory optimization.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.0.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [API docs](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/README.md)

## New features

- **PIC gateway settings panel:** All 15 PIC configuration registers (setpoint override, GPIO, LEDs, tweaks, smart power, thermostat detection, voltage reference, etc.) are now exposed via REST API (`/api/v2/pic/settings`), MQTT (`otgw-pic/settings/*`), and a new "Gateway Settings" section in the Web UI. Settings are read on-demand from the PIC (one PR= every 3s, full cycle ~45s) with human-readable formatting, color-coded live/cached indicators (green/amber/gray), and browser localStorage caching per hostname for up to 7 days.
- **Single-click GitHub release OTA:** The update page now lists GitHub releases with Installed/Update/Rollback badges. One-click download and flash with semver-aware version comparison including pre-release tags and Intel HEX integrity validation for PIC firmware.
- **Optional protected admin endpoints:** Settings, maintenance, file-management, reboot, and OTA routes can now be protected with HTTP Basic Auth using the Protected Endpoints Password setting.
- **Configurable MQTT publish gating:** OpenTherm and `PS=1` summary data can now be rate-limited to reduce MQTT broker load and WiFi chatter, with automatic status republish after boot and reconnect.
- **Full `PS=1` summary integration:** `PS=1` output is now parsed into the normal data pipeline, published to MQTT, and exposed through Home Assistant discovery.
- **Monitor-page command bar:** Send one-shot OTGW PIC commands such as `TT=20.5` or `GW=R` directly from the Web UI with command/response/error feedback in the log.
- **Light/dark theme toggle:** Switch between light and dark themes with a single click; preference persists across sessions.
- **Triple-reset WiFi recovery:** Three quick hardware resets reopen the captive portal and clear stale WiFi credentials without requiring a reflash.
- **Safer OTA / LittleFS flashing:** Reboot verification via `/api/v2/health`, browser backups of `settings.ini` and `dallas_labels.ini`, Dallas labels auto-preserved via localStorage, WiFi reconnect suppressed during flash writes, full partition erase before write.
- **OTGW simulation mode:** Test the firmware and Web UI without physical hardware. SIMULATION badge shown in the monitor header.
- **Crash log endpoint:** `/api/v2/device/crashlog` exposes ESP8266 crash information for diagnostics.
- **OTGW event reporting:** PIC restart, serial overrun, and RX errors forwarded via MQTT and WebSocket.
- **Heap memory in status:** Free heap and fragmentation shown in the Web UI footer and device info API.
- **GPIO conflict detection:** Conflicting GPIO pin assignments detected and warned about at boot.
- **Gateway mode and WebSocket indicators:** Compact status indicators with descriptive tooltips in the OpenTherm Monitor header.
- **Richer settings tooltips:** Settings fields now show descriptive tooltips on hover.

## Bug fixes

- **ESP hostname reverting to ESP-XXXXXX:** Deep audit and fix of all hostname code paths — hostname now set before WiFi.begin(), before/after configTime(), and after SDK auto-connect.
- **OTA filesystem corruption:** WiFi reconnect suppressed during flash writes; full LittleFS partition erased before writing.
- **IP validation:** Only `255.255.255.255` is rejected; valid addresses with a `255` octet are no longer blocked.
- **Boot-time restart cleanup:** Prevents spurious service restarts immediately after startup.
- **Hostname normalization:** Dot-stripping now targets the correct hostname buffer.
- **Webhook payload truncation:** Buffer widened to accommodate full webhook payloads after reboot.
- **File Explorer delete:** File deletion works consistently again.
- **NTP hostname:** `startNTP()` moved after `startWiFi()` so the configured hostname is active.
- **MQTT subscription truncation:** Buffer size increased for long topic strings.
- **PIC settings buffer sizing:** `sSmartPower` and `sClockMHz` fields enlarged for PIC firmware variants returning descriptive text (e.g., `Low power` instead of `L`).
- **WiFi portal false trigger:** Stale RTC data after USB flash no longer triggers triple-reset detection.

## Internal improvements

- **ArduinoJson removed:** All JSON paths now use bounded manual handling, reducing flash and RAM usage.
- **Settings/state reorganized:** 62+ flat globals replaced with `OTGWSettings` and `OTGWState` structs.
- **String class eliminated from hot paths:** Protocol, settings, and HTTP handlers use char[] buffers, reducing heap fragmentation.
- **MQTT autodiscovery optimized:** Streaming template rendering and in-place line parsing replace bulk buffer allocation.
- **Non-blocking WiFi reconnect:** State machine replaces the blocking 30-second reconnect loop.
- **REST API v2 completed:** Dispatch table routing; all remaining v1 calls migrated. Crash log endpoint wired up.
- **Security hardening:** Centralized auth for all POST/PUT in API dispatcher. CORS wildcard replaced with dynamic origin. Webhook SSRF prevention via DNS resolution. CSRF validation rewritten without String class. XSS fix in statistics table. Boot command and MQTT payload validation.
- **Dead code removal:** ~450 lines of legacy v1 JSON functions, unused helpers, dead enums, and stale CSS removed.
- **Stack pressure reduced:** ~1,400 bytes freed via centralized `otTopic` buffer and static local buffers in hot-path functions.
- **Bug fixes:** u8 MQTT topic suffix bug, `millis()` 49-day wraparound, f8.8 negative encoding UB, OT hex parse validation, settings dispatch optimized to else-if chain.
- **WiFiManager 2.0.17:** Upgraded from 2.0.15-rc.1 to stable release.

## Upgrade notes

- **No new breaking changes vs `v1.2.0`:** No new MQTT topic renames, REST API removals, or settings-format migrations. The new auth feature is optional and disabled by default.
- **Flash both firmware and filesystem:** The Web UI and OTA changes are best taken together.
- **Hard-refresh the browser after flashing** (Ctrl+F5).
- **If upgrading from older than `v1.2.0`:** Review the earlier MQTT and API migration notes first.

## Thank you

Thank you to everyone testing, reporting edge cases, and pushing the firmware forward. The PIC settings, OTA, recovery, and memory work in this release directly reflects community feedback.

Special thanks to: @hvxl, @sjorsjuhmaniac, @DaveDavenport, @DutchessNicole, @RobR, @GeorgeZ83, @tjfsteele, @vampywiz19, @Stemplar, @proditaki, and everyone in the Discord.

If you want to support the project: [Buy me a coffee](https://www.buymeacoffee.com/rvdbreemen)
