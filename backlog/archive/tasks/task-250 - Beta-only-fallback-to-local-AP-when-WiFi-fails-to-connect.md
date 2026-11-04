---
id: TASK-250
title: 'Beta-only: fallback to local AP when WiFi fails to connect'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 11:22'
updated_date: '2026-04-11 11:35'
labels:
  - network
  - beta
  - wifi
  - esp8266
  - esp32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When running a beta build (_VERSION_PRERELEASE defined in version.h) and WiFi fails to connect, automatically start a local SoftAP so the device remains accessible for local testing without a working WiFi network. The feature is gated by the presence of _VERSION_PRERELEASE at compile time — production builds with no prerelease tag will never contain this code. In AP mode: Web UI, REST API, telnet and OTA updates work; MQTT does not. The device retries WiFi every 5 minutes automatically.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 AP fallback is gated at compile-time by #if defined(_VERSION_PRERELEASE) from version.h — if _VERSION_PRERELEASE is not defined (production release), all AP-fallback code compiles to zero bytes and the feature cannot start under any circumstance
- [ ] #2 AP fallback is additionally checked at runtime on startup: if strlen of the prerelease string is 0, AP fallback is silently skipped even if somehow compiled in
- [ ] #3 Trigger: AP fallback starts after 2 consecutive failed WiFi connection attempts OR after 60 seconds of trying (whichever comes first)
- [ ] #4 AP SSID is OTGW-<MAC address last 6 hex, uppercase> (e.g. OTGW-A1B2C3), password is otgw123
- [ ] #5 AP uses a fixed IP of 192.168.4.1; Web UI, REST API and telnet are all reachable on that address
- [ ] #6 MQTT is NOT started or attempted while in AP fallback mode
- [ ] #7 OTA updates work in AP fallback mode: both ESP firmware OTA and PIC firmware flashing via the Web UI remain functional
- [ ] #8 Every 5 minutes the firmware scans for the configured WiFi SSID; if it is reachable, AP is shut down and normal WiFi connection is established
- [ ] #9 The existing Retry WiFi / Reset WiFi button in the settings page triggers an immediate reconnection attempt, also from AP-fallback mode
- [ ] #10 state.net gains a bAPFallback flag (bool, default false) that is set true when AP mode is active; isNetworkUp() returns true in AP mode
- [ ] #11 Telnet debug menu (h command) shows AP mode status and current SSID when bAPFallback is true
- [ ] #12 OLED display (if present) shows AP MODE and the SSID on the network status line
- [ ] #13 Web UI settings page shows a visible AP MODE badge/indicator when bAPFallback is true (reuse or extend existing WiFi SSID display)
- [ ] #14 A prominent comment block in networkStuff.ino at the AP-fallback code section warns: BETA ONLY — guarded by _VERSION_PRERELEASE — remove prerelease tag before production release
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-firmware.h: verwijder #if HAS_ETH_CAPABLE guard om NetworkSection, maak het altijd aanwezig. Voeg bAPFallback=false toe. Voeg NET_AP_FALLBACK=2 toe aan enum. Update isNetworkUp() zodat het true geeft in AP mode.
2. networkStuff.ino: voeg WIFI_AP_FALLBACK toe aan WifiState_t enum. Schrijf startAPFallback() en stopAPFallback() (beide #if defined(_VERSION_PRERELEASE) geblokt). Pas loopWifi() aan: na 2 retries naar AP fallback i.p.v. reboot. Voeg WIFI_AP_FALLBACK state toe met 5-min retry timer die WiFi.scanNetworks() gebruikt.
3. OTGW-firmware.ino: wrap startNTP() en startMQTT() in setup() met if (!state.net.bAPFallback).
4. MQTTstuff.ino: voeg in loopMQTT() return toe als state.net.bAPFallback true is.
5. handleDebug.ino: toon AP MODE status en SSID in h-commando output.
6. OLED.ino: toon AP MODE en SSID op display als bAPFallback true is.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Beta-only SoftAP fallback implemented when WiFi is unavailable.

After 2 failed WiFi connection attempts (or 60 seconds), the firmware
starts a SoftAP named OTGW-<MAC last 6 hex> with password otgw123 and
IP 192.168.4.1. WiFi is retried every 5 minutes; AP is torn down on
reconnect.

Changes:
- OTGW-firmware.h: NetworkSection restructured (always present); added
  bAPFallback + sAPSSID under _VERSION_PRERELEASE guard; isNetworkUp()
  returns true in AP mode; new isWiFiConnected() for MQTT/NTP gating
- networkStuff.ino: startAPFallback/stopAPFallback, WIFI_AP_FALLBACK /
  WIFI_AP_RETRY states in loopWifi, timer-based 5-min WiFi retry
- OTGW-firmware.ino: NTP and MQTT startup guarded against AP mode
- MQTTstuff.ino: handleMQTT() skips processing in AP mode
- handleDebug.ino: 'h' shows AP Fallback mode with SSID
- OLED.ino: System Status page shows AP MODE + IP 192.168.4.1
- restAPI.ino: device info + devtime emit apfallback:true when active
- index.js: updateNetworkIndicator handles AP mode badge
- index.css / index_dark.css: net-ap class (orange icon + bold text)

All code under #if defined(_VERSION_PRERELEASE) — zero bytes in
production builds. Build verified: ESP8266 + ESP32 both clean.
<!-- SECTION:FINAL_SUMMARY:END -->
