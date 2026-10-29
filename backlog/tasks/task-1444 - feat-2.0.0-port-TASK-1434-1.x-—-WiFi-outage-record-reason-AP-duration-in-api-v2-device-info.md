---
id: TASK-1444
title: >-
  feat-2.0.0: port TASK-1434 (1.x) — WiFi outage record (reason, AP, duration)
  in /api/v2/device/info
status: To Do
assignee: []
created_date: '2026-10-29T06:14:00Z'
updated_date: '2026-10-29T06:30:00Z'
labels:
  - port-from-1.x
  - wifi
  - rest-api
  - diagnostics
  - telnet
milestone: 2.0.0
dependencies:
  - TASK-1446
priority: low
ordinal: 604000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the 1.x WiFi station link record and its REST exposure to 2.0.0 (GH #682 diagnostics).

1.x source (branch otgw-1.x.x):
- Commit df9ae0d67 (1.x TASK-1278) is the recorder: src/OTGW-firmware/networkStuff.ino:65-87 (statics), :89-127 (802.11 reason-name table + wifiReasonName()), :137-166 (onWifiConnected/onWifiDisconnected), :168-173 (wifiLinkEventsBegin, called from startWiFi at :293), :177-204 (wifiLinkEventsLog, called from loopWifi at :785, including the catch-up for a boot association that completed before the handlers were registered), :206-224 (printWifiDropBanner in the telnet banner at :897).
- Commit 28efefe8a (1.x TASK-1434) is the REST exposure: networkStuff.ino:226-261 sendWifiLinkInfoJson(), called from restAPI.ino:1449 inside sendDeviceInfoV2(), plus the docs/api/openapi.yaml device/info schema for 15 keys.

Mechanism: the event handlers only record. The ESP has already closed every TCP client when the drop event fires, telnet included. One outage runs from the first disconnect event after a connect until the next connect; the SDK repeats disconnect events (e.g. 201 NO_AP_FOUND) while it retries. Per outage the record keeps: first reason, last reason, BSSID and channel left, event count, start millis, duration (set on reconnect, 0 while down), and self (the firmware's own dead-link check caused it). Global counters: outages since boot, disconnect events since boot, and the currently joined BSSID and channel. device/info emits these always-present keys: wifi_ap_bssid (string AA:BB:..), wifi_ap_channel, wifi_outages, wifi_disconnect_events, wifi_drop_bssid ('' before the first outage), wifi_drop_channel, wifi_drop_reason, wifi_drop_reason_name (e.g. BEACON_TIMEOUT), wifi_drop_last_reason, wifi_drop_last_reason_name, wifi_drop_events, wifi_drop_down_sec, wifi_drop_ago_sec (now - start), wifi_drop_self (bool). The drop fields are 0, empty or false until the first outage.

State on origin/dev: none of this exists. No WiFi event handler is registered (git grep ARDUINO_EVENT / WiFi.onEvent finds nothing in src). loopWifi() in origin/dev:src/OTGW-firmware/networkStuff.ino:327-445 detects drops only by polling WiFi.status() in WIFI_IDLE (:340-345). sendTelnetBanner() at origin/dev:src/OTGW-firmware/networkStuff.ino:496 has no Link/Drop lines. sendDeviceInfoV2() at origin/dev:src/OTGW-firmware/restAPI.ino:3113 builds its output from a frozen DeviceInfoSnap (struct at :3079-3111) and writes the network block with je.field() at :3217-3248.

Where it goes on 2.0.0:
- networkStuff.ino: the recorder (record struct, reason-name table, connected/disconnected callbacks registered once via WiFi.onEvent(cb, ARDUINO_EVENT_WIFI_STA_CONNECTED/DISCONNECTED) in startWiFi(), and a pending-log drain called from loopWifi() in every state). Also an accessor that copies the record into a caller struct.
- sendTelnetBanner(): Link/Drop lines as in 1.x printWifiDropBanner(). These come from 1.x TASK-1278 and are in scope unless a separate port of TASK-1278 has already landed.
- restAPI.ino: add the record copy to DeviceInfoSnap, fill it in sendDeviceInfoV2() with the other frozen inputs, and emit the 15 keys with je.field() right after the existing ssid/wifirssi block. Update docs/api/openapi.yaml on dev with the same key descriptions as 1.x 28efefe8a.

Dependency: TASK-1434 only exposes what 1.x TASK-1278 records. First check the dev backlog for a 'feat-2.0.0: port TASK-1278' task. If it exists and has not landed, add it as a dependency. If none exists, implement the recorder here.

Platform considerations (ESP32-S3, arduino-esp32 3.x / ESP-IDF):
- Event data: info.wifi_sta_connected.bssid/.channel and info.wifi_sta_disconnected.reason/.bssid. The channel left comes from the last connected event, as on 1.x.
- Reason codes are wifi_err_reason_t. Keep the 1.x names for codes 1-24 and 200-204, and add the IDF-only codes present in the pinned core (205 CONNECTION_FAIL, 206 AP_TSF_RESET, 207 ROAMING, and higher if defined). Check esp_wifi_types.h in the pinned framework rather than relying on memory. Unknown codes map to '?'. If arduino-esp32 offers WiFi.disconnectReasonName() on the pinned core, it may be used instead of a local table; verify first.
- Concurrency: the callbacks run in the arduino event task, while device/info is assembled on async_tcp from a snapshot and the banner runs on the telnet path. Protect the record with a portMUX_TYPE critical section, or publish it as one struct copied under that lock. Read it once into DeviceInfoSnap so a key set is never half old, half new. Keep the callbacks short (no logging, no allocation).
- Ethernet (HAS_ETH_CAPABLE): switchToEthernet() switches the radio off (WIFI_OFF). A disconnect caused by that must not count as an outage, e.g. ignore events while state.net.eMode == NET_ETHERNET or while a 'radio intentionally off' flag is set. Decide which, and document it in the code comment.
- BETA AP fallback (_VERSION_PRERELEASE): WiFi.disconnect() in WIFI_AP_RETRY (networkStuff.ino ~:434) and the retries in AP mode generate disconnect events. They belong to the same outage (no connect in between), so the 1.x semantics already give the right answer. Verify this.
- wifi_drop_self: dev has no dead-link check (1.x TASK-1248 is not ported), so nothing sets the self flag yet. Keep the key, always false, and provide the setter hook (the flag set just before an intentional esp_wifi_disconnect()/WiFi.disconnect()), so a later dead-link port only has to set it.
- On ESP32 the record may sit in RAM; no PROGMEM split is needed, but keep F()/PSTR in debug output per project style.

Risks: a race between the event task and the REST snapshot (torn reads); double-counting through the Ethernet switch or the AP fallback; the boot association finishing before handler registration (1.x catch-up at networkStuff.ino:199-203 must be mirrored, with WiFi.BSSID() and WiFi.channel()).

What NOT to port: the ESP8266 PROGMEM struct-table rationale (a switch jump table in RAM is an ESP8266 concern), strncpy_P copies, wifi_station_disconnect() calls, and the 1.x dead-link check itself (separate 1.x TASK-1248, separate decision). Do not change existing device/info keys.

## Evidence that dev lacks it

- git grep -n -i 'wifiOutages|wifi_outages|wifiDisconnectEvents|printWifiDropBanner|wifiDropBssid|wifi_drop|disconnect_events|wifiReasonName' origin/dev -- src docs/api -> no hits (origin/dev b07e3194a, 2026-10-29)
- git grep -n 'ARDUINO_EVENT|Network.onEvent|WiFi.onEvent' origin/dev -- src -> no hits: dev registers no WiFi station event handler at all, so no disconnect reason is captured anywhere
- origin/dev:src/OTGW-firmware/networkStuff.ino:340-345 loopWifi() WIFI_IDLE detects a drop only by polling WiFi.status() != WL_CONNECTED; it records no reason, BSSID, channel, duration or count
- origin/dev:src/OTGW-firmware/networkStuff.ino:496-520 sendTelnetBanner() has no Link/Drop lines (dev lacks TASK-1278 too)
- origin/dev:src/OTGW-firmware/restAPI.ino:3079-3111 struct DeviceInfoSnap and :3113 sendDeviceInfoV2(); the network block at :3217-3248 emits only wifi_current_subnet/gateway/dns1/dns2, ssid, wifirssi, wifiquality(_text)
- git grep -n -i 'dead.link|deadlink|wifiDeadLink' origin/dev -- src/OTGW-firmware -> no hits: the 1.x dead-link check (TASK-1248) that sets the self flag is not on dev
- git grep -i 'TASK-1278|TASK-1434|#682|outage record|disconnect reason' origin/dev -- backlog -> no port task; only the unrelated hits task-936 and task-1223.9

## Platform notes

1) Event API: on ESP8266 the code uses WiFi.onStationModeConnected/Disconnected with WiFiEventStation* structs and the WIFI_DISCONNECT_REASON_* constants. On arduino-esp32 3.x the equivalent is WiFi.onEvent(cb, ARDUINO_EVENT_WIFI_STA_CONNECTED / ARDUINO_EVENT_WIFI_STA_DISCONNECTED), with info.wifi_sta_connected.{bssid,channel} and info.wifi_sta_disconnected.{reason,bssid}, and the reasons are ESP-IDF wifi_err_reason_t values. The 2xx codes match (200 BEACON_TIMEOUT, 201 NO_AP_FOUND, 202 AUTH_FAIL, 203 ASSOC_FAIL, 204 HANDSHAKE_TIMEOUT). IDF adds more, such as 205 CONNECTION_FAIL, 206 AP_TSF_RESET, 207 ROAMING and 208+, so the name table needs those entries. arduino-esp32 may provide WiFi.disconnectReasonName(), but I have not verified it exists on the pinned core. 2) Concurrency: the ESP32 callback runs in the arduino/IDF event task, not cooperatively as on ESP8266. Device/info is built from a DeviceInfoSnap on another task (async_tcp), so the multi-field record needs a consistent copy into the snapshot, for example a portMUX critical section or one struct copied under a lock. 3) Ethernet (HAS_ETH_CAPABLE): switchToEthernet() sets WIFI_OFF, which raises an STA disconnect (typically ASSOC_LEAVE). It must not count as an outage, or the record must say so. 4) No PROGMEM or RAM split is needed on ESP32, but the 2.0.0 project style (F()/PSTR) still applies. 5) The self flag has no source on dev until the dead-link check (1.x TASK-1248) is ported. 6) BETA AP fallback (WIFI_AP_STA) and its WiFi.disconnect() in WIFI_AP_RETRY also generate events that the outage semantics must handle.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds. I found no evidence that dev has this under another name. The only 'disconnect reason' on dev is the MQTT client reason (MQTTstuff.ino:1489), not the WiFi station reason. The analyst's line ranges are slightly off. The ssid/wifirssi block is at origin/dev restAPI.ino:3231-3254, not :3217-3248; :3217-3224 is the wifi_current_* block. 1.x sendWifiLinkInfoJson starts at :230, which is inside the cited 226-261 range. The Ethernet-guard concern is real but partly handled already: loopWifi returns at :335 while NET_ETHERNET. The event callbacks still fire independently of loopWifi, so the guard is still needed in the callback.

1) Correct the restAPI target: the ssid/wifirssi emission on origin/dev is restAPI.ino:3231-3254, in three #if branches (BETA AP fallback :3231-3238, Ethernet :3240-3246, WiFi :3248-3253). Emit the 15 wifi_* keys AFTER the closing brace of that if/else chain (before '// --- Time, NTP & uptime ---'), not inside one branch. They must stay always-present in AP-fallback and Ethernet modes too. 2) In the Ethernet note, cite the existing guard origin/dev networkStuff.ino:335 (loopWifi returns when state.net.eMode == NET_ETHERNET). The WiFi.onEvent callback is not gated by it, so the callback must check eMode, or a radio-off flag, itself. 3) The WIFI_AP_RETRY WiFi.disconnect() reference (~:434) is correct; keep it.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a 2.0.0 build, GET /api/v2/device/info always contains the 15 keys wifi_ap_bssid, wifi_ap_channel, wifi_outages, wifi_disconnect_events, wifi_drop_bssid, wifi_drop_channel, wifi_drop_reason, wifi_drop_reason_name, wifi_drop_last_reason, wifi_drop_last_reason_name, wifi_drop_events, wifi_drop_down_sec, wifi_drop_ago_sec and wifi_drop_self, with the same names, types and before-first-outage defaults (0, '' or false) as 1.x commit 28efefe8a. A fresh boot on the bench shows wifi_outages=0 and wifi_ap_bssid/wifi_ap_channel equal to the joined AP (cross-checked against the MikroTik registration table).
- [ ] #2 Red/green on the bench (.63 Classic+S3 or .61 OTGW32, WiFi mode): force a deauth from the MikroTik router (192.168.88.1). On current origin/dev the device/info response has no wifi_drop_* keys (red). On the fix it shows wifi_outages=1, wifi_drop_reason=2 (AUTH_EXPIRE) or the IDF-reported code with its name, wifi_drop_bssid/channel equal to the AP left, and wifi_drop_down_sec>0 after reconnect.
- [ ] #3 Bench: block the gateway's MAC on the AP access list for about 90 s, then unblock. Device/info records exactly one outage, with wifi_drop_events>1, first and last reasons recorded separately, and wifi_drop_down_sec within a few seconds of the block length. wifi_disconnect_events increases by the same number of events.
- [ ] #4 The telnet banner on connect shows the Link line (AP, channel, outages, disconnect events) and the Drop line ('none since boot', or the last outage with reasons, AP, duration and age), matching the device/info values. Skip this AC only if a separate port of 1.x TASK-1278 already delivered the banner, and cite that task.
- [ ] #5 On a board with Ethernet active (esp32-combo or an Ethernet-capable target with the cable in), switching to Ethernet does not increase wifi_outages (verified on the bench, or by reading the code with the guard cited at file:line if no Ethernet bench is available; say which).
- [ ] #6 The record is read into DeviceInfoSnap under a lock (portMUX or equivalent) and the event callbacks do no logging or allocation. Verified by reading the code, with file:line cited in the Final Summary.
- [ ] #7 docs/api/openapi.yaml on dev documents the 15 keys, and the 2.0.0 CHANGELOG [Unreleased] has a user-facing entry referencing GH #682 and 1.x TASK-1434/TASK-1278.
- [ ] #8 Build for esp32, esp32-classic and esp32-combo succeeds (check each env, since build.py can mask per-env failures), and python evaluate.py --quick shows no new failures.
<!-- AC:END -->
