---
id: TASK-529
title: >-
  Profile and fix 3-4 sec latency on /sat/status, /device/info, /settings
  (OTGW32 ESP32-S3)
status: To Do
assignee: []
created_date: '2026-05-03 18:51'
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
