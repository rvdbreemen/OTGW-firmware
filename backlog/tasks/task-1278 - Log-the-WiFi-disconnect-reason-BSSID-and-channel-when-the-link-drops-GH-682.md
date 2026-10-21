---
id: TASK-1278
title: >-
  Log the WiFi disconnect reason, BSSID and channel when the link drops (GH
  #682)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-21 05:30'
updated_date: '2026-10-21 18:47'
labels:
  - enhancement
  - wifi
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: medium
ordinal: 250000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Requested by mrfox7688 on GH #682 (2026-10-20): daytime WiFi losses of ~2 min with a 9-minute rhythm on an ASUS AiMesh node; NAS and gateway lose the link together. The firmware logs no disconnect reason today (no onStationModeDisconnected handler in 1.x), so deauth, roaming and silent loss cannot be told apart. Same gap blocks TASK-1262 (auth/deauth loop, reason 3 hypothesis).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Shown on the bench: a MikroTik-forced disconnect and a reconnect produce the expected lines; no heap regression; esp8266 build and evaluate.py --quick green
- [x] #2 Per outage the firmware keeps one record: station disconnect count since boot, first and last 802.11 reason (number and short name), BSSID and channel left, outage duration, and whether the firmware caused it (dead-link check); the record is shown in the telnet banner on every telnet connect
- [x] #3 The telnet banner shows the BSSID and channel of the AP currently joined (the 'WiFi: joined' log line is also written on every connect, but no telnet client is connected at that moment, because the drop closed it)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Facts (core 2.7.4, Arduino15/.../esp8266/2.7.4/libraries/ESP8266WiFi):
- WiFi.onStationModeDisconnected gives ssid, bssid[6], reason (no channel). onStationModeConnected gives ssid, bssid[6], channel.
- Handlers run from the SDK event callback (_eventCallback), not from loop(). That callback calls WiFiClient::stopAll() on every disconnect first, so the telnet session is already gone when the drop happens.
- The SDK auto-reconnects. A short drop can pass before loopWifi() polls WiFi.status(), so loopWifi misses it today.

Design (networkStuff.ino only, about 60 lines):
1. Two static WiFiEventHandler objects, registered once in startWiFi().
2. Handlers only copy into a small static record: reason, bssid, channel of the AP left (taken from the last connect event), millis(), and a drop counter. No logging, no String, no TCP in the handler.
3. Repeat events while reconnecting (201 NO_AP_FOUND every few s) only bump a counter; the first reason is kept, the last reason is updated.
4. loopWifi() prints from loop context, once per change: on reconnect one line with BSSID and channel joined, plus the drop that came before (first and last reason with short name, BSSID and channel left, duration, number of disconnect events).
5. Reason names: PROGMEM switch for the codes in ESP8266WiFiType.h (1-24, 200-204); unknown codes print the number only.

No ADR (feature inside the existing pattern). No OSS search needed: core API plus about 60 lines of glue.

Verification (AC3): bench .88.68, build + flash via OTA.
- MikroTik registration-table remove for 84:F3:EB:22:B8:E1 forces a deauth: expect a reason line plus a reconnect line.
- Access-list deny for 2 min: expect one summary with a counted repeat, no log flood.
- Heap before/after from device/info; build.bat + evaluate.py --quick green.
- Old-vs-fix: the same forced disconnect on the current build shows no reason in the log.
- Can be combined with the TASK-1262 bench test (same access-list deny).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-21 plan revision (advisor review + mrfox log analysis):
- Printing at reconnect alone is lost: stopAll() kills telnet at the drop and the reconnect line fires before the capture reconnects. The record goes into sendTelnetBanner (shown on every telnet connect), set via a pending flag checked by loopWifi in every state (short drops never leave WIFI_IDLE).
- Self-caused drops: set a flag before both wifi_station_disconnect() calls of the TASK-1248 dead-link path, so those show as self-initiated, not as an AP kick.
- Register both handlers at the top of startWiFi(), before WiFiManager autoconnect; fall back to WiFi.channel() when no connect event was seen.
- AC3 heap check: compare max free block too (core builds a String ssid per event, repeated 201/202 during an access-list deny).
- mrfox capture 19/20-10 (beta.2+403636d): 0 station disconnects in 21 h (no loopWifi lost/reconnected lines; telnet session stayed up 10:16-11:28 through 5 MQTT reconnects; bootcount 4 unchanged). So for GH #682 this task will mostly show "0 disconnects", which is itself the proof that the loss happens while associated.

2026-10-21 bench evidence (.88.68, 1.7.8-beta.5+09436d6, logs in session scratchpad t1278/):
- Old build (d92c03f): MikroTik registration-table remove at 20:12:38; telnet stream stopped at the drop, nothing about the reason anywhere.
- New build: boot banner "outages 0 / Drop: none since boot". Deauth 20:36:19 -> banner "reason 2 AUTH_EXPIRE, AP 74:4D:28:C7:07:40 ch 1, down 1 s".
- Access-list deny 90 s (rule removed, count 0) -> "down 101 s, reason 2 AUTH_EXPIRE (last 203 ASSOC_FAIL, 31 events)": repeats collapse into one record, no log flood.
- Dead-link path (MQTT off + ICMP drop at router, both restored by trap; MQTT reconnected 20:46:47): 7/8 probes logged, forced reconnect -> "reason 8 ASSOC_LEAVE ... self: dead-link check".
- RAM: first version used a switch over the sparse reason codes; GCC put a ~800 B jump table in .rodata (RAM on ESP8266): globals 53200 -> 54184. Replaced by a PROGMEM table: 53368 (+168 B vs d92c03f). Free heap after boot 16.2-16.8 KB vs 17.6-18.0 KB before, but that baseline was at 1d15h uptime, not like-for-like; the compile-time figure is the clean comparison.
- build.bat: "Build completed successfully", fresh .ino.bin + .littlefs.bin 20:34. evaluate.py --quick: 37 pass, 0 fail.
- Not verified: the "WiFi: joined"/"link lost" log lines reaching a telnet client (none is connected at that moment, by design the banner carries the data); a real roam between two APs (bench has one AP).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The telnet banner now shows the last WiFi outage: 802.11 reason (first and last, with name), BSSID and channel of the AP left, duration, event count, and whether the firmware's own dead-link check caused it, plus the AP now joined and outages since boot.

Why: the core closes all TCP clients (telnet included) on every station disconnect, so a drop was invisible in telnet captures and deauth, roam and beacon loss looked the same (GH #682, also needed for TASK-1262).

Changes (networkStuff.ino): onStationModeConnected/Disconnected handlers registered at the top of startWiFi(); they only record (they run in the SDK event task). loopWifi() logs pending events in every state; sendTelnetBanner() prints the record. Both wifi_station_disconnect() calls of the TASK-1248 dead-link path set a self flag. Reason names come from a PROGMEM table (a switch cost ~800 B RAM as a .rodata jump table).

Tests: bench deauth (reason 2), 90 s access-list deny (one record, 31 events, 101 s), dead-link forced reconnect (reason 8, self). Static RAM +168 B. build.bat green, evaluate.py --quick 37/0.

Not covered: a roam between two APs (bench has one AP).
<!-- SECTION:FINAL_SUMMARY:END -->
