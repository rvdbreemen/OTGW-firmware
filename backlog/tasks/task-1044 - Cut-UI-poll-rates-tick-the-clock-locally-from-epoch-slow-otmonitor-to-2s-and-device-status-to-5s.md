---
id: TASK-1044
title: >-
  Cut UI poll rates: tick the clock locally from epoch, slow otmonitor to 2s and
  device status to 5s
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 21:53'
updated_date: '2026-07-19 22:30'
labels: []
dependencies: []
priority: high
ordinal: 163000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up on TASK-1043. That task capped the two UI-polled endpoints server-side; this one removes the reason the UI polls them so hard in the first place.

Measured on the martreides capture of 2026-07-19 (60 minutes, real boiler): 133 OpenTherm frames/min, 3515 Read-Ack/Write-Ack in the hour (~1/s of actual value updates), and the four fastest-moving values (RelModLevel, TrOverride, TSet, Tboiler) each update roughly once every 5 to 6 seconds. Everything else moves once every ~48 seconds. Polling otmonitor at 1 Hz therefore returns five to six identical payloads per real change.

The clock is the only thing in the UI that genuinely wanted 1 Hz, and it does not need the network for it. /api/v2/device/time already returns an `epoch` member alongside the formatted `dateTime`, so the browser can learn two offsets from a single response (clock skew against the browser, plus the device's timezone offset derived from the pair) and then tick locally. No timezone database in the browser, and a DST change is picked up at the next status poll.

Target rates per open page:
- /api/v2/otgw/otmonitor: client 2s, server window 1500ms
- /api/v2/device/time: client 5s, server window 4000ms, clock ticked locally at 1Hz
- /api/v2/device/info: unchanged, already throttled to about once a minute

That takes one open page from 121 to 43 requests per minute, a 65% reduction.

The server window is deliberately set below the client interval (about 75%). setInterval is not exact: background throttling and GC shift ticks, so a tick at 1990ms is normal, and a window of exactly 2000ms would hand 429s to a well-behaved client.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Clock display updates every second without a network request, derived from the epoch member
- [x] #2 Device timezone is honoured without shipping a timezone database to the browser
- [x] #3 otmonitor polls at 2s and device/time at 5s per open page
- [x] #4 Server rate-limit windows sit below the client intervals so normal timer jitter does not trigger 429
- [x] #5 Build passes and evaluator shows no new failures
- [x] #6 Verified on hardware: clock runs smoothly, heap and mode fields still update, no 429 in the console under a single open tab
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Geimplementeerd in commit 38bea0f2.

Web UI (data/index.js):
- learnDeviceClock() leidt twee offsets af uit een enkel antwoord: (epoch*1000 - Date.now()) is de scheefstand tussen browser- en apparaatklok, (parse(dateTime als UTC) - epoch*1000) is de tijdzoneverschuiving van het apparaat. Opgeteld in devClockOffsetMs.
- renderDeviceClock() tikt op 1 Hz en rendert bewust met getUTC*: de zoneverschuiving zit al in de offset, nogmaals lokaliseren zou dubbeltellen.
- otmonitor-interval 1000 -> 2000, statusinterval 1000 -> 5000.

Firmware (restAPI.ino): windowMs is nu een veld per endpoint (otmonitor 1500, device/time 4000) in plaats van een gedeelde constante. De problem+json detail en de RateLimit-Policy header rapporteren het echte venster.

Valkuil onderweg, opgelost: GATEWAY_MODE_REFRESH_INTERVAL telt status-poll-ticks en geen seconden. Met de tick van 1s naar 5s zou gateway-mode stil van elke minuut naar elke vijf minuten zijn geschoven. Constante van 60 naar 12, met de rekensom in het commentaar.

Verificatie: node --check schoon op index.js, build completed successfully zonder compileerfouten, artefacten vers om 00:02. Evaluator 34 passed / 1 failed, waarbij die ene (1 unresolved ADR reference van 1408) pre-existing is.

AC6 blijft OPEN: hardware-verificatie ontbreekt. Te controleren op een toestel: loopt de klok vloeiend en klopt hij met de apparaattijd, blijven heap en modusvelden bijwerken, en verschijnt er geen 429 in de console bij een enkel geopend tabblad.

HARDWARE-VERIFICATIE 2026-07-20, bench-toestel 192.168.88.68 (MAC 84:F3:EB:22:B8:E1), build 1.7.2-beta.1+ccb5014.

Ratelimiet:
- otmonitor: 1e request 200, direct daarna 429 met Retry-After: 2, Cache-Control: no-store, RateLimit "default";r=0;t=2, RateLimit-Policy "default";q=1;w=2, Content-Type application/problem+json en de volledige RFC 9457 body.
- device/time: 429 met Retry-After: 4 en w=4, dus het per-endpoint venster werkt.
- Na 5s wachten weer 200: het venster heropent.
- device/info en health: drie snelle requests achter elkaar, alle drie 200. Niet-gelimiteerde routes zijn ongemoeid.

Klok:
- Weergave liep van 00:23:34 naar 00:23:37 over 3 seconden reele tijd, lokaal getikt.
- devClockOffsetMs = 7196717, dus ~+2 uur zomertijd van het apparaat plus ~3,3 s echte klokscheefstand, automatisch geleerd uit het epoch/dateTime-paar. Geen tijdzonedatabase in de browser.

Console: nul 429-meldingen, nul errors of warnings. Alleen een informatieve LOG-regel.

Geserveerde index.js draagt de wijzigingen: 9 treffers op de klokfuncties, intervallen 2000 en 5000, GATEWAY_MODE_REFRESH_INTERVAL = 12.

Niet gemeten: de feitelijke requests per minuut in de browser. De automatiseringstab stond op visibilityState "hidden", waardoor de UI niet pollt en Chrome de timers throttlet. De 43/min volgt rekenkundig uit de geverifieerde intervallen, maar is niet waargenomen.

Kanttekening bij de opstelling: de ESP zat los van het carrier board, dus picavailable=false en geen MQTT-broker geconfigureerd.

AC6 afgerond op hardware: klok volgde de echte verstreken tijd (60s vooruit over 60s wandklok, ondanks Chrome-timerthrottling in de achtergrondtab), heap-display werkte bij van 19584/19208 naar 18792/18200, console zonder 429 of errors. De requests-per-minuut zelf blijven onmeetbaar in een geautomatiseerde achtergrondtab: Chrome throttlet timers op browserniveau, los van de visibilityState-override die de app-gates opent. De 43/min volgt rekenkundig uit de geverifieerde intervallen 2000/5000 in de geserveerde index.js.
<!-- SECTION:NOTES:END -->
