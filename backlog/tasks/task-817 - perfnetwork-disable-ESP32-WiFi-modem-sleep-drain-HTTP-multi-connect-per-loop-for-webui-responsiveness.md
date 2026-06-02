---
id: TASK-817
title: >-
  perf(network): disable ESP32 WiFi modem-sleep + drain HTTP multi-connect per
  loop for webui responsiveness
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 22:38'
updated_date: '2026-06-02 22:55'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Initial web page serving on OTGW32 (ESP32) takes seconds, sometimes never responds. Hot-path analysis (this session) cleared all delay() calls as boot/reboot/hardware-settle (none in steady-state loop) and confirmed MQTT 5s (ADR-108) + webhook 500ms (ADR-048) + Dallas async are already dispositioned (TASK-676). Two NEW structural latency sources remain: (1) ESP32 WiFi modem-sleep (WIFI_PS_MIN_MODEM default) never disabled -> per-packet 100ms-1s latency on every request; (2) sync WebServer serves ONE socket per loop iteration -> a 6-asset page load needs 6+ loop turns. Fix both behind the platform abstraction.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 platformWifiDisableSleep() shim added to platform_esp32.h (WiFi.setSleep(false)) and platform_esp8266.h (no-op inline stub); called once after WiFi/ETH is up; no raw ifdef ESP32 outside abstraction
- [ ] #2 Steady-state HTTP drain: handleClient() serves pending connections in a bounded loop (max N iterations, watchdog-fed) so multi-asset page loads complete in 1-2 loop turns; bound documented to prevent loop starvation
- [ ] #3 Weather per-read busy-wait cap reviewed; tightened only if zero risk to fetch correctness, else left with rationale
- [ ] #4 TCP_NODELAY / Nagle on the WebServer client investigated; finding documented (implemented if a clean abstraction-safe API exists, else recorded as dead-end with reason)
- [x] #5 MQTT 5s socketTimeout left UNCHANGED (ADR-108); documented as out-of-scope not-a-finding
- [ ] #6 python build.py exits 0 for esp32 AND esp8266 envs (grep per-env SUCCESS lines)
- [ ] #7 python evaluate.py --quick reports no new failures incl ESP-abstraction boundary gate
- [ ] #8 Prerelease tag bumped (src/OTGW-firmware/** touched)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
HOT-PATH ANALYSIS RESULT (validation done, no code yet):
- All delay() calls = boot/reboot/hardware-settle. ZERO in steady-state loop. Leave as-is (KISS).
- MQTT 5s socketTimeout = ADR-108 accepted, DO NOT TOUCH. Webhook 500ms = ADR-048. Dallas = async. (all per TASK-676)
- Two NEW structural sources remain: WiFi modem-sleep + one-serve-per-loop.

IMPL (behind platform abstraction; shims in src/libraries/Platform/src/):
1. platform_esp32.h: add inline platformWifiDisableSleep() { WiFi.setSleep(false); }
   platform_esp8266.h: add inline platformWifiDisableSleep() {} no-op stub (keep stable LTS power profile; ESP32 is the reported platform). DECISION POINT for user: also disable on ESP8266? default=no.
   Call site: after initial connect in startWiFi() AND in loopWifi() WIFI_RECONNECTED (setSleep can reset across reassociation). Unguarded call from app code.
2. OTGW-firmware.ino: replace single httpServer.handleClient() at :610 with bounded drain — for(i<4){ httpServer.handleClient(); feedWatchDog(); } so a 6-asset page load completes in ~1-2 loop turns instead of 6+. Bound=4 prevents OT/MQTT starvation; watchdog fed each iter. Comment the bound rationale.
3. SATweather.ino wstreamGet/Peek 5s cap: review only; tighten to 2s ONLY if zero correctness risk, else leave with rationale note. (low value — triple-guarded fetch)
4. #5 Nagle/TCP_NODELAY: OTGWWebServer = WebServer(esp32)/ESP8266WebServer(esp8266). Investigate if a clean setNoDelay exists on the accepted client; implement if abstraction-safe, else document dead-end.

VERIFY: python build.py (both envs, grep per-env SUCCESS); evaluate.py --quick (incl ESP-abstraction gate); bump prerelease; commit refs TASK-817.
NO telemetry pre-capture (user opted to ship #1+#2 directly).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Impl: #1 platformWifiDisableSleep() added to BOTH platform_esp32.h (WiFi.setSleep(false)) and platform_esp8266.h (WiFi.setSleepMode(WIFI_NONE_SLEEP)) per user 'disable on both'. Called in startWiFi() after connect + loopWifi() WIFI_RECONNECTED. #2 bounded HTTP drain loop (bound=4, watchdog-fed) replaces single handleClient() at OTGW-firmware.ino:610. #3 weather 5s busy-wait LEFT (triple-guarded, tighter cap risks dropping slow-but-valid OWM responses; minimal-change-surface). #5 Nagle/NoDelay = DEAD-END: ESP8266WebServer(2.7.4) and ESP32 WebServer expose no public setNoDelay; only per-handler client().setNoDelay (invasive) or vendored-core patch (barred). Marginal once #1+#2 land. evaluate.py --quick: 0 failed, 97.1%.
<!-- SECTION:NOTES:END -->
