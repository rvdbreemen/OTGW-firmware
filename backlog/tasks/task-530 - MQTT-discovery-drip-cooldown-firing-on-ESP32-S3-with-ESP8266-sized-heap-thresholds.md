---
id: TASK-530
title: >-
  MQTT discovery drip cooldown firing on ESP32-S3 with ESP8266-sized heap
  thresholds
status: To Do
assignee: []
created_date: '2026-05-03 18:52'
labels:
  - mqtt
  - discovery
  - esp32
  - drip
  - 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/MQTTstuff.ino
  - 'src/OTGW-firmware/restAPI.ino:1727'
  - >-
    https://discord.com/channels/812969634638725140/1105556725714649128/1500567342722060359
  - >-
    https://discord.com/channels/812969634638725140/1105556725714649128/1500568157918593205
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Symptoom

Op een OTGW32 (ESP32-S3) device met `freeheap=68k+`, `maxfreeblock=31732`, fragmentatie 52–56 %, bereikt `hd_drip_cooldown_skip` waarde **32 binnen 4 minuten uptime** (zie `/api/v2/device/info` JSON-payloads in sergeantd's F12 captures van 2026-05-03, 18:38 en 18:42).

Op een ESP32-S3 met 320 KB+ DRAM beschikbaar zou de drip-pressure-skip logica nooit moeten firen onder normale condities. Robert (project owner) bevestigde tijdens het onderzoek: "Er zou geen enkele reden moeten zijn dat de drip geskipt wordt door pressure. Op een ESP32 zou dat gewoon normaalste zaak van de wereld moeten zijn."

## Hypothese

De heap-pressure thresholds in MQTT discovery drip mode (cooldown, slowmode, burst-skip) zijn waarschijnlijk hardgecodeerd voor de ESP8266's ~80 KB totale heap. Op ESP32 met 4× zoveel DRAM zijn die absolute waarden disproportioneel conservatief, waardoor:

1. Drip mode wordt onnodig vaak in cooldown gezet → discovery topics worden langzamer gepubliceerd
2. `disc_pending_ids` blijft langer hangen (43 → 0 duurde 4 min in de meting)
3. Trage discovery-publishes contenderen continu met HTTP requests om TCP/Wi-Fi airtime — bijdraagt aan TASK-529's REST endpoint traagheid

## Mogelijke oorzaken

- Threshold gedefinieerd als absolute `< 8192 bytes free` zonder platform-aware schaling
- Threshold gedefinieerd in termen van `getMaxFreeBlockSize()` met een vast minimum dat niet schaalt
- `platformMaxFreeBlock()` retourneert mogelijk een stub/constante op ESP32 (zelfde waarde 31732 in 4 opeenvolgende reads is verdacht)

## Effect na fix

`hd_drip_cooldown_skip` blijft op 0 onder normale ESP32 omstandigheden, drip mode publiceert in volle snelheid, discovery completion is sneller, minder concurrent airtime-druk op HTTP request handlers.

## Bron

Discord-thread in #dev-sat-mqtt van 2026-05-03; specifiek de twee F12 console pastes met `/api/v2/device/info` payloads tonend `hd_drip_cooldown_skip: 25` en `32`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Identificeer de heap-pressure thresholds gebruikt door MQTT discovery drip mode (cooldown, slowmode, burst-skip) — bestand + lijnnummer
- [ ] #2 Documenteer de ESP8266-budget aannames die in die thresholds gebakken zitten (waar komt elk getal vandaan?)
- [ ] #3 Definieer ESP32-passende thresholds: ofwel proportioneel aan totale DRAM, ofwel als absolute headroom (vrij-heap blijft boven X), niet als vast getal
- [ ] #4 Refactor de threshold-logica zodat platform-passende waarden compile-time (`#ifdef ESP32`) of runtime (`platformTotalDRAM()`) gekozen worden
- [ ] #5 Verifieer `hd_drip_cooldown_skip` blijft op 0 bij normale MQTT discovery op een OTGW32 met `freeheap > 50 KB` over een 30-minuten observatieperiode
- [ ] #6 Verifieer `platformMaxFreeBlock()` op ESP32 returnt een echte runtime-waarde, geen constante (zelfde waarde 31732 in 4 opeenvolgende reads in sergeantd's data is verdacht)
- [ ] #7 Geen regressie op ESP8266: drip mode op classic OTGW reageert nog steeds op heap pressure conform ADR-080 / TASK-349 / TASK-361
- [ ] #8 Update de heap-diagnostics doc (of de relevante ADR) met de platform-aware threshold policy
<!-- AC:END -->
