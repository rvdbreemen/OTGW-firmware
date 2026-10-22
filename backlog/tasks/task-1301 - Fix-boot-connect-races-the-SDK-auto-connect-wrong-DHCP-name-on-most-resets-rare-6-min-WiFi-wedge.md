---
id: TASK-1301
title: >-
  Fix: boot connect races the SDK auto-connect (wrong DHCP name on most resets,
  rare 6-min WiFi wedge)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T12:50:00Z'
updated_date: '2026-10-22T14:48:00Z'
labels:
  - bug
  - wifi
dependencies: []
references:
  - backlog task TASK-1262
priority: high
ordinal: 255000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench while chasing TASK-1262 AC#1 (boot-diag build, 35 RTS resets with the AP present).

The SDK auto-connect flag is on (persistent WiFi), so the SDK is already in STATION_CONNECTING at 382 ms, before setup() touches WiFi. startWiFi() then calls WiFi.hostname() and WiFi.begin() (wifi_station_connect + dhcpc_start) on top of that attempt. Two outcomes seen:
- ~75 % of resets: the SDK attempt wins and the router lease carries the SDK default name ESP-22B8E1 instead of the configured hostname (28 of 34 fast resets). The early WiFi.hostname() call in setup() does not prevent this, contrary to its comment.
- 1 of 20 resets (08:35:14, plus 06:17 and 08:12 earlier): the SDK stays in STATION_CONNECTING for 120 s with zero station events while the AP shows the station associated without DHCP; direct connect fails, the config portal runs 240 s, and the gateway is offline ~6 min.

Prior art: ESPHome (wifi_component_esp8266.cpp) turns SDK auto-connect off (wifi_station_set_auto_connect(0)) and runs disconnect -> config -> hostname -> wifi_station_connect on every connect; Tasmota (support_wifi.ino WifiBegin) runs persistent(false), disconnect(true), mode, hostname, begin. Neither builds on an SDK attempt already in progress.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every reset in a series of at least 60 on the bench gets its DHCP lease under the configured hostname, and none ends in the config portal or a WiFi wedge; old build vs fix shown on the same rig with RSSI recorded
- [x] #2 Saved credentials survive the change (no portal after the first and later boots), and loopWifi reconnects after an AP outage still work (deny test)
- [x] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Old-build baseline from the boot-diag reset series (lease name per reset, wedges)
2. Fix in startWiFi direct-connect path: setAutoConnect(false) once, wifi_station_disconnect() when not idle, hostname, single begin()
3. Fix build keeps the diag ring for the same timeline; 60 RTS resets with lease name + RSSI
4. Deny tests 90 s and 440 s (supervisor path)
5. Clean build without diag, evaluator, commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 bench (.68, RTS resets via COM3, AP present, scratchpad t1262/reset_series.sh, outputs fix_series_a..d.out):
- Old/diag build 2eb0fd5: lease ESP-22B8E1 on 28 of 34 fast resets (08:27 series 14/19, 14:35 series 14/15), 3 WiFi wedges in ~47 resets (06:17, 08:12, 08:35; see TASK-1262 notes for the 08:35 ring timeline). RSSI -74..-87.
- Fix (87da863 + diag ring, then + setup stage markers): 60 of 60 resets lease OTGW, web back in 4-11 s, no wedge, no portal. RSSI -73..-80 (one -95 reading). The first boot after the OTA (flag still on, SDK auto-connecting) also leased OTGW (15:45:13), and so did the OTA boot at 16:04:53. Chance of 0 wedges in 60 at the old ~6 % rate: ~2 %.
- The SDK still reads the saved credentials with auto-connect off: 60 boots without a portal (begin() without arguments uses them).
- Deny 90 s (t1278/t1301_deny.sh): block 16:17:50-16:19:20, rejoined 16:19:21 in the same boot (uptime unbroken, banner outages 1). Telnet is down during the outage, so whether the SDK reconnect policy or loopWifi's begin() won is not visible.
- Deny 440 s (t1301_deny_long.sh): block 16:38:39-16:45:59; supervisor restart ~deny+305 s ('WiFi: too many reconnect failures'); the boot ran without the AP (35 events, never linked, ADR-099 skip); rejoined 16:46:01, 2 s after the allow, lease OTGW. Rules left 0.
- Clean build without the diag (a1d756077): build.bat green, globals 53376 B (unchanged), evaluate.py --quick 0 failed; flashed to .68.
- Side observation: every RTS reset still shows 'received deauth: sending station leaving (3)' at the AP, also with the fix; a clean restart (OTA) shows a disassoc (8) instead and no reason 3.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Boot no longer races the SDK auto-connect (commit a1d756077).

Problem: with the SDK auto-connect flag on, the ESP8266 is already connecting at 382 ms, before setup() touches WiFi. startWiFi() issued hostname() + begin() on top of that attempt. Result on the bench: the router lease carried the SDK default name ESP-22B8E1 on 28 of 34 resets, and about 1 in 20 resets left the station stuck in STATION_CONNECTING with zero station events for the 120 s direct-connect window, followed by the 240 s portal (~6 min offline).

Change (networkStuff.ino startWiFi direct-connect path): turn the auto-connect flag off once (the SDK writes flash only on change), stop a connect still in progress with wifi_station_disconnect() (keeps the saved credentials), set the hostname, call begin() once. Same order as ESPHome (wifi_component_esp8266.cpp: auto_connect(0), disconnect, config, hostname, connect); Tasmota does disconnect, mode, hostname, begin. WiFiManager never touches the flag. setAutoReconnect and persistent(true) unchanged.

Evidence (old vs fix, same rig, RTS resets with the AP present): old 28/34 ESP-22B8E1 and 3 wedges in ~47; fix 60/60 OTGW, no wedge, no portal, also the first boot after the OTA. 90 s block: rejoined 1 s after the allow without a restart. 440 s block: supervisor restart, offline boot, rejoined 2 s after the allow. build.bat green (globals unchanged), evaluate.py --quick 0 failed. CHANGELOG [Unreleased] Fixed entry added.

Not verified: other routers (only the MikroTik bench); the wedge's internal SDK cause (closed source) is inferred from the event timeline, its removal is shown statistically (0/60 vs ~6 %).
<!-- SECTION:FINAL_SUMMARY:END -->
