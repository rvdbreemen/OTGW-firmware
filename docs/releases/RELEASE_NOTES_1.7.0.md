# OTGW-firmware v1.7.0 Release Notes

**Release date:** 2026-06-25
**Branch:** main (from otgw-1.x.x)
**Compare:** [v1.6.1...v1.7.0](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.6.1...v1.7.0)

## Overview

v1.7.0 is a stability and headroom release for the 1.x (ESP8266) line. It crash-proofs the heap-fragmentation failure mode that caused random reboots on long-running devices under sustained load, and follows that with a measured RAM and heap-headroom optimization pass that restores roughly the free-memory margin the firmware had back at 1.3.5. It also adds gateway-override visibility over MQTT and in the Web UI, plus a handful of correctness fixes. No feature behaviour was removed.

## New features

- **Active gateway overrides surfaced over MQTT and in the Web UI** (TASK-805, ADR-082). Override state set on the PIC (setpoint override, setback, and related gateway commands) is now published over MQTT and shown on the dashboard, so Home Assistant and the Web UI reflect what the gateway is actually doing rather than only what the thermostat requested.
- **WiFi signal quality in the telnet welcome banner** (TASK-834). The banner now shows WiFi quality as a percentage and a human-readable label alongside the raw RSSI.
- **Headless WiFi provisioning tooling** (TASK-900). `bin/provision-wifi-ap.py` and `bin/wifi-profiles.py` allow provisioning WiFi credentials without the captive-portal browser flow, useful for bench and CI setups.

## Bug fixes

- **Heap-fragmentation crash-loop on long-running devices, crash-proofed at the source.** A six-version field bisect showed the largest contiguous heap block collapsing to a few hundred bytes within a minute while total free memory still looked adequate, after which the next contiguous allocation returned NULL and was written, faulting the device. The fix spans several layers:
  - MQTT and WebSocket sends now gate on the largest contiguous block (not just total free) and skip gracefully when it is too small (TASK-837).
  - Emergency heap recovery no longer allocates a listener socket while heap is critical; it stops the OTGWstream listener and re-arms it once heap recovers (TASK-837).
  - MQTT reconnect uses exponential back-off to cut lwIP socket churn during an outage (TASK-837).
  - HTTP static-file serving backs off under fragmentation: `httpServer.handleClient()` is skipped while the largest contiguous block is below a safe floor, letting the heap coalesce while clients retry. Flash-upload handlers are never gated, so OTA and PIC updates still complete (TASK-841, TASK-843).
  - The root crash in the ESP8266 core's `streamFile` path (an unchecked per-TCP-segment `new[]` that wrote to a NULL destination, the `0x4000df64` ROM `memcpy` fault) is patched at build time so a mid-transfer low-memory moment drops and retries the connection instead of crashing (TASK-844, ADR-084).
  - The main loop was returned to its field-proven `delay(1)` pacing; a prior change to `yield()` had uncapped the loop and was a second fragmentation driver (TASK-901).
  - The `getDallasLabels` file-serve path that bypassed the streamFile guard was closed (TASK-852).
  - New diagnostic counters `mqtt_fragskips`, `ws_fragskips`, and `http_fragskips` are exposed on telnet and in MQTT stats so headroom can be watched directly.
- **MQTT broker ports above 32767 can now be saved** (TASK-903). The port field was `int16_t`, so high non-standard ports were stored as negative values and could not be saved. The field is now `uint16_t` (full 1 to 65535 range).
- **S0 pulse-counter pulses-per-kWh above 65535 no longer wrap** (TASK-903). The validator allowed 1 to 100000 into a `uint16_t` field; it now constrains to the field's 1 to 65535 range.
- **Debug Information tab shows friendly labels for the runtime WiFi rows** (TASK-904). `wifi_current_subnet`, `_gateway`, `_dns1`, `_dns2`, and the simulation flag now render readable labels, with a "(current)" qualifier distinguishing runtime values from the configured static-IP fields.

## Internal improvements

- **RAM and heap-headroom optimization pass** (TASK-902, TASK-903). A measured pass reclaimed about 6.6 KB of static RAM and lifted the idle free heap by about 6 KB. The headline change moved the OpenTherm message-name table (`OTmap[]`) into flash (PROGMEM) via inline table arrays, freeing about 4.96 KB of static RAM. Roughly 25 smaller, individually verified buffer and integer-width trims followed (OpenTherm log and topic buffers, MQTT namespace and client-id buffers, the JSON coalescing send buffer, the REST API URI tokenizer, the OT command queue, several diagnostic counters), plus two String-allocation removals (`dBmtoQuality()`, `upTime()`) that cut per-call heap churn. The largest-contiguous-block floor under sustained load rose from about 4.9 KB to about 11 KB. Each step shipped under its own prerelease tag and was bench-verified.
- **Static IP fields stored as `uint8_t` quads instead of `String`** (TASK-903), in response to the static-IP feature request (#561). Removes per-access heap churn from the WiFi settings section.
- **Build, capture, and crash-decode tooling**: per-build artifact archiving outside the repo, `.elf` preservation for `addr2line` crash decoding, a USB serial crash-dump capture script, browser-devtools (CDP) capture, crash-log REST polling in the capture tool, and assorted capture-tool robustness fixes.

## Breaking changes

No breaking changes versus v1.6.1. Several internal buffer-size constants were tightened during the RAM optimization pass (for example `OT_TOPIC_LEN` 50 to 40, `MQTT_ID_MAX_LEN` 96 to 48, `sTimezone` 65 to 48). These were sized with verified headroom above the longest real value each buffer holds, and persisted settings are read with bounded `strlcpy`, so no stored configuration is truncated on upgrade.

## Upgrade notes

- Standard upgrade. No settings migration is required.
- The ESP8266 core is patched at build time (ADR-084) for the official build artifacts; if you build locally, `build.py` reapplies the patch on every build.
- After upgrading, the Debug Information tab (Advanced menu) shows the live heap and the new fragmentation counters if you want to watch headroom directly.

## Known issues

- Under very heavy concurrent Web UI load on a fragmented heap, the UI may briefly slow or return a `503` for a request and retry, by design, instead of rebooting. This is the back-off behaviour, not a fault.

## Contributors

This release was shaped by field testers and issue reporters in the OTGW Discord and on GitHub. Thank you:

- **GeorgeZ83**, whose multi-day diagnostic captures drove the heap-fragmentation bisect and validated the optimization pass.
- **Richard_HA**, **crashevans**, **Ties**, **Remco**, and **Mattie** for field testing and feedback on the beta builds.
- **Andre Brait** (andrebrait) for the static IP feature request (#561) that informed the WiFi settings rework.
