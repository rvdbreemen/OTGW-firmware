---
id: TASK-513
title: >-
  feat(debug): telnet 'w' trigger for instant Open-Meteo fetch + add '7' SATBLE
  to welcome banner
status: Done
assignee:
  - '@claude'
created_date: '2026-05-02 17:17'
updated_date: '2026-05-02 17:24'
labels:
  - debug
  - telnet
  - weather
  - open-meteo
  - esp32
  - esp8266
dependencies: []
references:
  - src/OTGW-firmware/SATweather.ino
  - src/OTGW-firmware/handleDebug.ino
  - src/OTGW-firmware/networkStuff.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Small developer-ergonomics improvement: pressing 'w' on the telnet debug console (port 23) should trigger an immediate Open-Meteo weather fetch, bypassing the 3600-second scheduler interval. Useful when debugging coordinate config, OWM-vs-Open-Meteo dispatch, or HTTP timeout behaviour. Also fixes an oversight in the telnet welcome banner: the '7' SATBLE debug toggle (added on this branch in handleDebug.ino under #if defined(ESP32)) is missing from the banner's flag list.

Implementation:
- SATweather.ino: add public wrapper `weatherFetchOpenMeteoNow()` that logs the trigger and calls the existing static `weatherFetchOpenMeteo()`. Existing preconditions (bWeatherEnable, coords) remain in effect.
- handleDebug.ino: add `case 'w':` in handleDebugChar() switch. Add help-menu entry under "--- Commands ---".
- networkStuff.ino sendTelnetBanner(): add ESP32-only '7 SAT BLE' line (behind #if defined(ESP32)) and a 'w) Open-Meteo fetch' line under a brief Commands section.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SATweather.ino exposes a public function (e.g. weatherFetchOpenMeteoNow) that triggers weatherFetchOpenMeteo() with a debug log line
- [x] #2 handleDebug.ino case 'w' calls the public Open-Meteo wrapper
- [x] #3 handleDebug.ino help menu (case 'h') documents 'w' under Commands section
- [x] #4 networkStuff.ino sendTelnetBanner adds '7 SAT BLE' line gated by #if defined(ESP32)
- [x] #5 networkStuff.ino sendTelnetBanner adds 'w) Open-Meteo fetch' line
- [x] #6 ESP8266 build clean
- [x] #7 ESP32-S3 build clean
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Telnet debug ergonomics: 'w' shortcut for instant Open-Meteo fetch with structured state dump, plus '7' SATBLE toggle line in the welcome banner.

### What changed

- **`src/OTGW-firmware/SATweather.ino`:** new public `weatherFetchOpenMeteoNow()` that logs the trigger, calls the existing static `weatherFetchOpenMeteo()` (preserving its `bWeatherEnable` + coords guards), then dumps the cached state via the new file-static `weatherDumpStateToTelnet()`. Forward declaration of `weatherFetchOpenMeteo()` added so the wrapper can compile before its definition. The dump prints valid flag, age in ms since last update, fetch error count, configured coords, day/night, weather code, temperature + apparent, humidity, cloud cover, pressure, wind speed + gusts + direction, and precipitation breakdown (rain + snow). ASCII-only units (`C`, `deg`) to avoid telnet stream encoding edge cases.

- **`src/OTGW-firmware/handleDebug.ino`:** forward declaration `void weatherFetchOpenMeteoNow();`. New `case 'w':` in `handleDebugChar()` that calls the wrapper. Help-menu entry `w) Trigger Open-Meteo weather fetch and dump state` added under the existing `--- Commands ---` section.

- **`src/OTGW-firmware/networkStuff.ino` (`sendTelnetBanner`):** `7 SAT BLE` line added to the debug-flag block, gated by `#if defined(ESP32)` to mirror the existing `case '7':` guard in `handleDebugChar`. New `Commands:` block with `w  Open-Meteo fetch + dump` line.

### Why

Manual debug-time trigger for the Open-Meteo HTTP fetch was missing; the only way to validate coords, network, or fetch-error handling was to wait for the 3600-second scheduler tick. The 'w' shortcut closes that gap. The '7' line in the welcome banner was an oversight at the time the SAT BLE toggle was added; the welcome banner went stale relative to the help menu (this commit only fixes the missing line; the broader 1-6 toggle naming inconsistency between banner and help menu is left for a separate task).

### Verification

- ESP8266 build clean: RAM 84.7% (69364 bytes), Flash 77.6% (810256 bytes). Net +1324 bytes flash and +20 bytes RAM vs pre-fix baseline — the structured dump's PSTR strings.
- ESP32-S3 build clean: RAM 31.9% (104564 bytes), Flash 96.4% (1895923 bytes). Slightly lower flash than the prior 96.8% — compiler optimised elsewhere despite the new strings.
- Forward-declaration pattern matches the existing precedent at `networkStuff.ino:402` (`void handleDebugChar(char c);`). No header file invented for a single cross-file reference.

### Risk and follow-up

- The fetch is synchronous (5s HTTP timeout); pressing 'w' blocks the telnet session for up to 5 seconds during the call. Acceptable for a debug shortcut.
- If `bWeatherEnable=false` or coords are 0,0, the fetch is a no-op and the dump shows the existing (possibly stale or invalid) cached state — clearly labelled by the `Valid: no` indicator in the output.
- Welcome banner inconsistency for toggles 4-6 (banner says "MQTT gating / Sensors / NTP" while help menu means "Sensors / SAT / OTDirect") is pre-existing and left untouched here. Worth a small follow-up task if it actually confuses users.
<!-- SECTION:FINAL_SUMMARY:END -->
