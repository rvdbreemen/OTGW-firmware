---
id: TASK-484
title: >-
  Fix: WiFi setup AP mode webUI unreachable after Reset Wifi (andrebrait,
  1.5.0-beta)
status: To Do
assignee: []
created_date: '2026-04-29 23:48'
updated_date: '2026-05-04 06:23'
labels:
  - bug
  - needs-info
  - wifi
  - wifimanager
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - 2026-04-27 to 2026-04-29
  - >-
    Discord DM andrebrait 2026-05-04: bisected to between 1.2.0 and 1.3.0 —
    fresh flash 1.2.0 works, 1.3.x+ unresponsive
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter andrebrait clicked Reset Wifi in the webUI by mistake. After that the device serves the OTGW-mac_address SSID and assigns IP 192.168.4.2 to clients, but http://192.168.4.1 is unreachable. curl -v shows TCP connect succeeds, GET request sent, then 'Recv failure: Software caused connection abort'. Linux laptop can ping 192.168.4.1 but cannot open the webUI. Hard reset (mainboard button), wifi module reset, and power cycle do not help. Tester resorted to reflashing.

Setup: Wemos D1 Mini classic, NodoShop OTGW v2.x, firmware 1.5.0-beta+ (after the DHCP fix in beta.2).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on a clean device with WiFi reset triggered
- [ ] #2 Identify why the WifiManager AP-mode HTTP server accepts TCP connections but aborts the GET response
- [ ] #3 Either fix the AP-mode webserver or add diagnostics so future occurrences surface in serial output
- [ ] #4 Add a confirmation modal in the webUI before Reset Wifi (defensive UX, requested by reporter)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-04: andrebrait (Discord DM) bisected the regression: fresh flash of 1.2.0 works, 1.3.x+ does not respond. Device is unreachable after fresh flash — same TCP-accepts/HTTP-aborts symptom as the WiFi-reset case. This pins the root cause to a change introduced between v1.2.0 and v1.3.0.

Key change identified: WiFiManager library upgraded from 2.0.15-rc.1 to 2.0.17 in commit 92c521d9. The config portal call-flow in networkStuff.ino is otherwise structurally identical between both versions (same startConfigPortal path for fresh/no-credentials flash). Other WiFi-adjacent changes in 1.3.0: deprecated `setTimeout()` renamed to `setConfigPortalTimeout()` in the same commit, and `WiFi.hostname()` is now set before `WiFi.begin()` in the wifiSaved path (not relevant for fresh flash).

Hypothesis: WiFiManager 2.0.17 config portal HTTP server has a regression on ESP8266 where it opens the AP correctly (TCP layer works, ping 192.168.4.1 succeeds) but the HTTP response handler crashes or aborts the GET response. This matches the symptoms exactly: 'Recv failure: Software caused connection abort' after TCP connect.

Investigation path:
1. Downgrade WiFiManager back to 2.0.15-rc.1 in build.py and test fresh flash — if portal works, library version is the root cause.
2. Check WiFiManager 2.0.17 changelog/issues for AP HTTP server regressions on ESP8266.
3. Try `autoConnect()` instead of `startConfigPortal()` as a behavioural comparison.
4. Check if `setConfigPortalTimeout(240)` vs the old `setTimeout(240)` has different semantics that could cause the portal HTTP server to close before the user can connect.
<!-- SECTION:NOTES:END -->
