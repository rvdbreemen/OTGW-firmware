---
id: TASK-1446
title: >-
  feat-2.0.0: port TASK-1278 (1.x) — telnet banner shows last WiFi outage with
  802.11 reason, AP, channel and down time
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T06:16:00Z'
updated_date: '2026-11-14T15:40:00Z'
labels:
  - port-from-1.x
  - wifi
  - telnet
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 606000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Why: on 2.0.0 a WiFi drop is only noticed by loopWifi() polling WiFi.status() (origin/dev src/OTGW-firmware/networkStuff.ino:344-349, logs 'WiFi: connection lost'). Nothing records why (802.11 reason), which AP/channel was left, how long the link was down, or how many drops happened since boot. Field reports of 'gateway falls off the network' therefore cannot tell a deauth, a roam, a lost beacon and NO_AP_FOUND apart. 1.x solved this for GH #682.

1.x reference (commit df9ae0d67, file src/OTGW-firmware/networkStuff.ino on otgw-1.x.x):
- :65-90 file-static link record: wifiLinkUp, joined BSSID/channel, wifiDisconnectEvents (since boot), wifiOutages (since boot), first/last reason of the last outage, BSSID/channel left, events in that outage, start ms, duration ms (0 while still down), self flag, log-pending flags.
- :93-128 reason-name table + wifiReasonName().
- :137-166 onWifiConnected/onWifiDisconnected: handlers only record. An outage starts at the first disconnect event after a connect (repeated SDK retry events, e.g. 201, increment the event count and update the last reason) and ends at the next connect (duration computed there).
- :168-173 wifiLinkEventsBegin(), idempotent, called from startWiFi() (:293).
- :177-204 wifiLinkEventsLog(), called every loopWifi() pass (:785) in every state: emits 'WiFi: link lost, reason N NAME, AP xx ch N[ (self)]' and 'WiFi: joined AP xx ch N after S s, E event(s), last reason N NAME'; also seeds the joined BSSID/channel when the boot association finished before the handler was registered.
- :206-224 printWifiDropBanner(), called from sendTelnetBanner() (:897): '  Link : AP <bssid> ch <n>   outages <n>   disconnect events <n>' and either '  Drop : none since boot' or '  Drop : <age> s ago, down <d> s, reason <n> <NAME> (last <n> <NAME>, <e> events), AP <bssid> ch <n>[, self: ...]'.
- :704,:711 set wifiSelfDisconnect=true right before the firmware's own wifi_station_disconnect().
Bench evidence on 1.x (.88.68): deauth -> reason 2; 90 s router access-list block -> one outage, 31 events, 101 s, first 2 last 203; self-forced reconnect -> reason 8 marked self.

Where it goes on 2.0.0 (origin/dev, b07e3194a):
- Register handlers in startWiFi() (origin/dev networkStuff.ino:83, right after WiFi.mode(WIFI_STA)) with WiFi.onEvent(..., ARDUINO_EVENT_WIFI_STA_DISCONNECTED) and ARDUINO_EVENT_WIFI_STA_CONNECTED; read info.wifi_sta_disconnected.reason / .bssid and info.wifi_sta_connected.bssid / .channel. Register once (guard on a stored wifi_event_id_t).
- Call the log-drain function at the top of loopWifi() (origin/dev networkStuff.ino:326), before the NET_ETHERNET early return so a drop recorded before Ethernet takeover is still logged.
- Print the two lines in sendTelnetBanner() (origin/dev networkStuff.ino:496) directly after the 'Net :' line (~:519-528), only in the WiFi branch or with a clear 'WiFi' label when Ethernet is active.
- Mark self-caused disconnects: set the self flag before WiFi.disconnect() in Ethernet.ino:103 (switch to Ethernet) and networkStuff.ino:433 (BETA AP_RETRY timeout). 1.x's 'dead-link check' does not exist on dev; do not port the probe as part of this task.

Platform considerations:
- The arduino-esp32 event callback runs on the event task, concurrently with the loop task and the async_tcp task that renders the telnet banner. Protect the record with a portMUX_TYPE critical section, and have the banner/log copy a snapshot under the lock before formatting. No logging, no heap allocation, no String inside the callback.
- ESP-IDF reason codes differ from the ESP8266 SDK (200 BEACON_TIMEOUT, 201 NO_AP_FOUND, 202 AUTH_FAIL, 203 ASSOC_FAIL, 204 HANDSHAKE_TIMEOUT, 205 CONNECTION_FAIL, plus newer IDF 5.x codes). Use WiFi.disconnectReasonName((wifi_err_reason_t)r) from arduino-esp32 3.3.5 if present (verify in the framework headers); otherwise a small const table. Do NOT copy the 1.x table verbatim (ESP8266 macro names, and its jump-table RAM rationale is ESP8266-only). The reason field is uint8_t on ESP32 too; store as uint16_t if any IDF code exceeds 255 (check).
- millis() wrap: use unsigned subtraction as 1.x does.
- Placement of the drop/join log lines must go through Debug* on the loop task, never from the event callback.

Risks: data race between event task and banner (mitigated by the critical section); banner length growth on the async_tcp stack (keep each line a separate _debugPrintf_P call). 

What NOT to port: the ESP8266 WiFiEventHandler objects and onStationMode* API; the PROGMEM table and pgm_read_byte lookup; the dead-link probe self flag at 1.x :704/:711 (no probe on dev). The /api/v2/device/info exposure of this record (1.x TASK-1434, commit 28efefe8a) is a separate item and out of scope here.

## Evidence that dev lacks it

- origin/dev (b07e3194a) src/OTGW-firmware/networkStuff.ino:496-610 sendTelnetBanner: 'Net :' line prints only SSID/RSSI/IP (around :519-528); no Link/Drop line
- origin/dev src/OTGW-firmware/networkStuff.ino:83-270 startWiFi: no WiFi.onEvent/station event registration
- origin/dev src/OTGW-firmware/networkStuff.ino:326-442 loopWifi: polls WiFi.status() only ('WiFi: connection lost' at :346), no reason captured
- git grep -E 'onEvent|WiFiEvent_t|arduino_event|STA_DISCONNECTED|wifi_err_reason|BEACON_TIMEOUT' origin/dev -- src/OTGW-firmware: only otLogWs.onEvent (webSocketStuff.ino:242) and MQTT disconnect reason (MQTTstuff.ino:1487-1597), nothing for WiFi
- git grep -i '1278|outage|802.11 reason|disconnect reason' origin/dev -- backlog/tasks backlog/archive: no sibling task
- origin/dev has no dead-link probe (git grep handleWifiProbeVerdict|wifiLinkProbe|dead-link: no hits); self-initiated WiFi disconnects on dev are Ethernet.ino:103 and networkStuff.ino:433 (AP_RETRY timeout)

## Platform notes

ESP32-S3 / arduino-esp32 3.3.5 (pioarduino 55.03.35, origin/dev platformio.ini:110): use WiFi.onEvent(cb, ARDUINO_EVENT_WIFI_STA_DISCONNECTED) and ARDUINO_EVENT_WIFI_STA_CONNECTED; the callback runs on the arduino event task, not the loop task, and possibly on another core, so the shared record needs a portMUX critical section (or a copy-on-read snapshot), not plain statics. ESP-IDF reason codes differ from the ESP8266 SDK set: 802.11 codes 1..~67 plus 200 BEACON_TIMEOUT, 201 NO_AP_FOUND, 202 AUTH_FAIL, 203 ASSOC_FAIL, 204 HANDSHAKE_TIMEOUT, 205 CONNECTION_FAIL and newer ones (e.g. 206+ in IDF 5.x). Prefer arduino-esp32's WiFi.disconnectReasonName() (verify it exists in 3.3.5) over copying the 1.x PROGMEM table; the RAM-jump-table concern was ESP8266-specific. The banner already runs on async_tcp (debugTelnet is AsyncSimpleTelnet); keep the record lines short (_debugPrintf_P 256-byte buffer). When state.net.eMode == NET_ETHERNET the WiFi record should still print but the 'joined AP' line is not the active transport, and the WiFi.disconnect() in Ethernet.ino:103 must be marked self-caused, not counted as a field outage.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds. Every dev citation I opened is accurate. The banner 'Net :' content sits at origin/dev networkStuff.ino:515-525, which is within the cited range. Nothing on dev records why a WiFi link dropped. One nuance the analyst only half-covered: on the Ethernet takeover (Ethernet.ino:103-104), dev also sets WiFi.mode(WIFI_OFF). After that, a STA_DISCONNECTED event fires once and then no more events arrive. The record therefore freezes at that self-caused drop, so it must be flagged self-caused or it will read as a field outage forever. Not verified (also not by the analyst): whether arduino-esp32 3.3.5 exposes WiFi.disconnectReasonName(). The draft already says to check this.

Minor fixes:
(1) In the 'Where it goes' bullet, cite the Ethernet self-disconnect as Ethernet.ino:103-104 (disconnect + WIFI_OFF), not 103 alone. Add to the AC: after an Ethernet takeover, the banner's last drop is marked self-caused and is not counted as a field outage.
(2) Cite the banner insertion point precisely: after the WiFi/Ethernet IP block at origin/dev networkStuff.ino:515-525 and before the 'Heap :' line (~:531).
(3) Add an AC for the red/green check: build current dev, connect telnet after a forced deauth, and show that there is no Link/Drop line. Then show the lines after the change, on the same bench procedure.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On 2.0.0, each telnet connect prints a 'Link :' line (joined AP BSSID, channel, outages since boot, disconnect events since boot) and either 'Drop : none since boot' or a 'Drop :' line with age, down duration, first and last reason code plus name, event count, AP BSSID and channel left, and a self-caused marker when applicable
- [ ] #2 Disconnect/connect events are captured via WiFi.onEvent in arduino-esp32 3.x; the callback only records into a lock-protected record (portMUX or equivalent), and logging happens from loopWifi() on the loop task
- [x] #3 Repeated disconnect events during one outage (SDK retries) count as one outage with an event count and a last reason, matching 1.x semantics
- [ ] #4 WiFi.disconnect() calls initiated by the firmware (Ethernet.ino switch-over, BETA AP_RETRY timeout) are marked self-caused in the record
- [x] #5 The boot association that completes before the handler is registered still fills the joined BSSID/channel shown in the banner
- [ ] #6 Bench check on .63 (Classic+S3) or .61 (OTGW32): router-forced deauth via MikroTik shows reason 2 (or the IDF equivalent) in the 'WiFi: link lost' log line and in the next telnet banner; a ~90 s access-list block shows one outage with duration about 90-110 s and an event count above 1; before the change (current dev) the banner shows no such information. Capture both transcripts as evidence
- [ ] #7 python build.py builds esp32, esp32-classic and esp32-combo targets green and python evaluate.py --quick shows no new failures
- [ ] #8 After an Ethernet takeover (Ethernet.ino:103-104 disconnect plus WIFI_OFF), the banner's last drop is marked self-caused and is not counted as a field outage.
- [ ] #9 Red/green on the same bench procedure: build current dev, connect telnet after a forced deauth and show there is no Link/Drop line; then show the lines after the change.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no event handlers/wifiReasonName in src. Agent-doable; prerequisite of TASK-1444.

2026-11-04 Robert: MikroTik fault injection allowed: one rule per test, scoped to the bench boards' MAC/IP (.61/.63), comment-tagged, scheduler auto-removal within 30 min, removed right after; config export before and after recorded in the notes.

2026-11-04 (opus agent; lead reran in wt-int4): one WiFi link/outage record from WiFi.onEvent (STA_CONNECTED/DISCONNECTED; callbacks only write under portMUX g_wifiLinkMux, no log/alloc), retries in one outage = 1 outage with event count + first/last reason (WiFi.disconnectReasonName), self-caused drops (Ethernet takeover, AP-retry timeout, TASK-1450 dead-link reconnect) marked and not counted (mark only while the link is up, cleared by every event), boot catch-up of BSSID/channel; telnet banner Link/Drop lines; device/info 14 wifi_* keys (the AC's '15' lists 14 names; the 15th in the 1.x commit was cmd_dropped of another task); openapi documented. tests/test_wifi_link_record.py 15 OK (old: 15 FAIL; 2 mutants red each), deadlink 28, rest_contract 16 OK, evaluate 0 failed. Tag beta.84; firmware build + MikroTik deauth/access-list bench pending.

2026-11-14 (09) bench .63 (2.0.0-beta.248+5e8c416, CAPsMAN '1e verdieping-1' + router wlan2 'KeepOut2'; evidence 09 scratchpad b1444/ and b1444b/). Fresh state: wifi_outages 0, wifi_disconnect_events 0, wifi_ap_bssid C4:AD:34:FE:E5:43 ch 6 = the CAPsMAN registration-table entry for AC:27:6E:CE:45:D8 (interface MAC C4:AD:34:FE:E5:43); banner 'Link : AP C4:AD:34:FE:E5:43 ch 6 outages 0 disconnect events 0' and 'Drop : none since boot'. Deauth (/caps-man registration-table remove): API back after 6 s; wifi_outages 1, wifi_drop_reason 2 AUTH_EXPIRE (last 2), drop_bssid C4:AD:34:FE:E5:43 ch 6, drop_events 1, drop_down_sec 4, drop_self false; banner 'Drop : 14 s ago, down 4 s, reason 2 AUTH_EXPIRE (last 2 AUTH_EXPIRE, 1 events), AP C4:AD:34:FE:E5:43 ch 6'. A first block attempt with only the CAPsMAN access list was not a block: the unit roamed to the router's own wlan2 (74:4D:28:C7:07:40, same SSID) after 4 s, which the record showed correctly as a new outage. 90 s block with both access lists (/caps-man access-list reject + /interface wireless access-list authentication=no, comment-tagged, scheduler safety net): API answered 0 of 15 polls during the block, back 5 s after the unblock; device/info: wifi_outages 2->3 (exactly one outage), wifi_disconnect_events 2->21 (+19) = wifi_drop_events 19, first reason 2 AUTH_EXPIRE, last reason 203 ASSOC_FAIL, drop_bssid 74:4D:28:C7:07:40 ch 1, drop_down_sec 93 (block 91 s + reconnect). Banner 'Link : AP C4:AD:34:FE:E5:43 ch 6 outages 3 disconnect events 21' / 'Drop : 104 s ago, down 93 s, reason 2 AUTH_EXPIRE (last 203 ASSOC_FAIL, 19 events), AP 74:4D:28:C7:07:40 ch 1'. Router afterwards: both access lists 0, scheduler 0, /export terse identical before/after apart from the timestamp header. Settings untouched (no setting changed).

Not shown on the bench: the 'WiFi: link lost' log line (networkStuff.ino:199) is written while the link is down and the 'joined AP ... after N s' line right at reconnect, before a telnet client can reconnect, so neither reached the telnet capture; the same data is in the next banner (above). AC9 bench red not done (no pre-beta.84 build compatible with the current layout; the red is the host test). AC5 boot catch-up: the fresh-boot banner shows the joined AP and channel (above). AC8 by reading only (no Ethernet on .63): Ethernet.ino:103 marks the drop WIFI_SELF_ETHERNET, not counted (networkStuff.ino:129).

AC2 by reading: WiFi.onEvent callbacks write only under portENTER_CRITICAL(&g_wifiLinkMux) (networkStuff.ino:110-118 connected, 123-144 disconnected; no logging, no allocation); logging is wifiLinkEventsLog() on the loop task (networkStuff.ino:185-210). AC4 (self-caused marks for the Ethernet and AP-retry disconnects) not verified on the bench by 09.

Correction (09, coordinator review): AC2 and AC8 unticked, their text does not allow code reading as evidence. AC8 is not verifiable on hardware (no Ethernet on the bench); the reading evidence for AC2 and AC8 stays in the notes above. 1444 AC5 and AC6 stay ticked: their text allows reading (AC5 'or by reading the code ... say which', AC6 'Verified by reading the code'); the file:line citations are in the 1444 notes for the owner's Final Summary.
<!-- SECTION:NOTES:END -->
