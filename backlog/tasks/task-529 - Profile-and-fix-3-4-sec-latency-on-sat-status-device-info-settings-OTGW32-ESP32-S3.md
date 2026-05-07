---
id: TASK-529
title: >-
  Profile and fix 3-4 sec latency on /sat/status, /device/info, /settings
  (OTGW32 ESP32-S3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-03 18:51'
updated_date: '2026-05-07 18:01'
labels:
  - performance
  - esp32
  - web
  - sat
  - 2.0.0
dependencies: []
references:
  - 'src/OTGW-firmware/restAPI.ino:1582'
  - 'src/OTGW-firmware/restAPI.ino:1965'
  - 'src/OTGW-firmware/SATcontrol.ino:1524'
  - 'src/OTGW-firmware/jsonStuff.ino:241'
  - >-
    https://discord.com/channels/812969634638725140/1105556725714649128/1500565875055661066
  - >-
    https://discord.com/channels/812969634638725140/1105556725714649128/1500570043514359949
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Symptoom

Op een OTGW32 (ESP32-S3) device met build `2.0.0-alpha+168bd9e` (gerapporteerd door sergeantd op 2026-05-03 in #dev-sat-mqtt) hebben drie v2 REST endpoints onacceptabel hoge latencies:

- `/api/v2/sat/status` — 3917 / 4359 / 4468 ms (avg 4248 ms)
- `/api/v2/device/info` — 3152 / 3349 / 3375 / 3604 ms (avg 3370 ms)
- `/api/v2/settings` — 3045 / 3178 ms (avg 3112 ms)

Op dev (1.5.x) returnen dezelfde endpoints in <200 ms.

## Side effects

1. **WebSocket disconnects** vallen exact ~50–524 ms na elke trage REST call (5 opeenvolgende drops in firmware-side telnet log) — browser keepalive verloopt terwijl server JSON rendert.
2. **`ERR_CONNECTION_TIMED_OUT`** in browser F12 console op `/api/v2/device/time` en `/api/v2/otgw/messages/3` — wijst op TCP slot exhaustion in de sync ESP32 `WebServer` (één request tegelijk; trage requests vullen de listen-backlog en nieuwe connecties krijgen geen handshake).
3. **DHW slider feedback frozen** — boiler ontvangt het commando wel (zie `OTD: cmd "SW=42"` succesvol uitgevoerd), maar zonder WS-state-echo lijkt de UI bevroren.

## Wat het NIET is (uitgesloten via dev/feature diff)

| Endpoint | dev entries | feature entries | groei | latency |
|---|---:|---:|---:|---:|
| `/sat/status` | 123 | 125 | +2 (+1.6%) | **4248 ms (slowest)** |
| `/device/info` | 55 | 86 | +31 (+56%) | 3370 ms |
| `/settings` | 54 | 126 | +72 (+133%) | **3112 ms (fastest)** |

JSON-groei correleert **negatief** met latency. `/settings` heeft de grootste payload (2.3× zoveel entries) maar is het snelste endpoint. Dat sluit "meer JSON = trager" als dominante oorzaak uit. Bovendien: per-`sendContent` kost ~1.2 ms op dev vs ~13 ms op feature — zelfde code path (`jsonStuff.ino:241–303` byte-identiek voor ESP8266 hot path), zelfde hardware.

Werkelijke bottleneck moet zitten in: TCP/Wi-Fi airtime contentie, FreeRTOS task scheduling op ESP32, MQTT discovery drip publishes, of telnet REST-debug overhead. Concrete metingen vereist.

## Context

- Device: OTGW32 (ESP32-S3), 240 MHz, OT-Direct mode (geen PIC), 4 min uptime na `Task watchdog` reset, bootcount=9
- Heap: 65–84 KB free, fragmentatie 52–56 %
- `disc_pending_ids` = 43 → 0 over 4 min, `hd_drip_cooldown_skip` = 25 → 32
- Wifi RSSI -54 / -59 (signalkwaliteit goed, geen netwerk-issue)
- Geen actieve OT bus (`otgwconnected=false`, `thermostatconnected=false`)

## Bron-evidence

Voor het volledige onderzoek zie de Discord-thread in #dev-sat-mqtt op 2026-05-03 vanaf 17:29 (sergeantd's eerste rapport) tot 18:43 (F12 console capture). Twee bot-replies bevatten de stap-voor-stap analyse en de paradox-meting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Instrumenteer `millis()` start/end binnen `satSendStatusJSON`, `sendDeviceInfoV2`, `sendDeviceSettings` om pure JSON-renderingstijd te scheiden van totale handler-tijd (incl. TCP flush)
- [ ] #2 Reproduceer de traagheid op een OTGW32 (ESP32-S3) device en lever een breakdown per endpoint: T_render, T_tcp_flush, T_total
- [ ] #3 Meet apart met REST-debug uit, MQTT uit, en drip mode uit om de bijdrage van elke variabele te isoleren
- [ ] #4 Bepaal of de bottleneck zit in `httpServer.sendContent` flush, MQTT/telnet airtime-contentie, of een andere concurrent task (BLE, OTDirect, FreeRTOS scheduling)
- [ ] #5 Beslis of migratie van sync `WebServer` naar `AsyncWebServer` op ESP32 de TCP slot exhaustion oplost — voor- en nadelen documenteren
- [ ] #6 Reduceer gemiddelde latency voor de drie endpoints tot <500 ms op OTGW32 onder normale operationele condities
- [ ] #7 Verifieer geen regressie op ESP8266 (1.5.x dev path mag niet langzamer worden)
- [ ] #8 Browser F12 capture toont na de fix géén `ERR_CONNECTION_TIMED_OUT` meer tijdens normale paginalaad
- [ ] #9 Documenteer hoofdoorzaak en fix in een ADR (nieuw of update van bestaande)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the ESP32 REST and SAT JSON handlers involved in /api/v2/sat/status, /api/v2/device/info, and /api/v2/settings.
2. Add lightweight timing instrumentation that separates handler/render time from total request time and surfaces it in a way we can read over REST/telnet without changing endpoint contracts.
3. Audit known contention points around sendContent/sendJsonMap, telnet REST-debug logging, and discovery drip pressure so we can reduce obvious self-inflicted latency without hardware-only guesswork.
4. Build and evaluate the branch, then update TASK-529 with findings and what still needs on-device measurement.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Added request-scoped REST perf instrumentation for /api/v2/sat/status, /api/v2/device/info, and /api/v2/settings.
- jsonStuff streaming helpers now accumulate sendContent/send_P time into T_send and chunk counts while handlers measure T_total; T_render is derived as total minus send time.
- Exposed latest perf metrics through /api/v2/device/info and added optional telnet traces when REST debug is enabled.
- Local validation passed with ./build.sh --target esp32 and .build-venv/bin/python evaluate.py --quick.
- Remaining ACs still require OTGW32 hardware runs: reproduce the slow case, compare REST-debug/MQTT/drip variants, determine root cause, decide on AsyncWebServer migration, verify <500 ms latency and browser timeout recovery, and document the outcome in an ADR.

---
**Field evidence — SergeantD on alpha.3+ca845dd, 2026-05-07 (Discord)**

Device: OTGW32 (ESP32-S3), OT-Direct mode, no PIC, no boiler attached, MQTT connected, BLE active (4 sensors).
Heap healthy: 69-76 KB free, 31.7 KB max block, fragmentation 43%, hd_ws_drops=0, hd_mqtt_drops=0.

`/api/v2/device/info` perf payload:
- perf_sat_status_total_ms = 3843 (send=3816, render=27, chunks=406)
- perf_device_info_total_ms = 3686 (send=3382, render=304, chunks=350)
- perf_device_info_max_ms = 4091 (samples=15)
- perf_settings_total_ms = 2840 (send=2804, render=36, chunks=292)

Render is 27-304 ms; the time is in TCP send/flush. Confirms the sync `WebServer` flush hypothesis (AC #4).

**Contributing factors observed in the same telnet capture** (worth folding into the investigation, not just `WebServer` flush):
1. **settings.ini write storm during UI form edits** — six full LittleFS rewrites in 14 s for a single form interaction (13:30:54 to 13:31:08), each 30-80 ms. While each write runs, lwIP cannot service WS / HTTP. Tracked separately in TASK-NNN (settings debounce).
2. **SAT loop re-emits unchanged OT commands every 30 s** (`MM=100` re-enqueued 17 times in 13 min). Adds command-queue pressure (`high-water=2..3 (capacity=12)` observed). Tracked separately in TASK-NNN (SAT write-on-change).

Both aggravators put extra work on the same scheduler that has to flush HTTP responses. Resolving them may not eliminate the 4 s peak but should reduce its frequency.

**Linked WebSocket symptom**: WS code 1006 reconnect loop (browser side) + server-side `webSocketEve disconnected` bursts (`burst window=5000ms total=3 conn=1 disc=2`). Visible during HTTP traffic; matches TCP-slot starvation hypothesis. Frontend reconnect-dedup is split into its own task (JS-only).

---
**Cross-reference backfill (2026-05-07):**
- Settings debounce contributing factor → TASK-564
- SAT write-on-change contributing factor → TASK-565
- Frontend WebSocket reconnect-dedup symptom → TASK-563

---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 4** (investigation, parallel track — does not block Ship 1-3). Direction-of-investigation decision waits on the alpha.10 perf JSON from a tester after Ship 1 + Ship 2 land. Three possible outcomes mapped in the plan; do not commit to AsyncWebServer migration before the data point.
<!-- SECTION:NOTES:END -->
