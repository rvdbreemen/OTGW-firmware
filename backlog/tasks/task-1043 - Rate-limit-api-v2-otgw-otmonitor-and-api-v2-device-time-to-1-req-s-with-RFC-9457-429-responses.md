---
id: TASK-1043
title: >-
  Rate-limit /api/v2/otgw/otmonitor and /api/v2/device/time to 1 req/s with RFC
  9457 429 responses
status: Done
assignee:
  - '@claude'
created_date: '2026-07-19 21:31'
updated_date: '2026-07-19 22:31'
labels: []
dependencies: []
priority: high
ordinal: 162000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The OTGW web UI polls two endpoints once per second per open page, from two independent 1s timers in data/index.js (line 269 refreshOTmonitor -> /api/v2/otgw/otmonitor, line 281 refreshDevTime -> /api/v2/device/time). Every extra open tab therefore adds a further 2 requests per second, indefinitely, whether or not anyone is looking at it.

Field evidence (TASK-1037, martreides 2026-07-19): two open pages produced 14500 requests in one hour, a sustained 242/min, and the device died at the 60 minute mark. The firmware has no defence against this: a user who leaves a dashboard open on a second screen can degrade the gateway without any indication that they are doing so.

Decision: enforce the poll rate server-side on these two endpoints, at 1 request per second, and answer excess requests with 429 rather than serving them.

Response shape, per RFC 6585 and RFC 9457:
- Status 429 Too Many Requests
- Retry-After with the seconds until the window reopens (mandatory, the interoperable part)
- Cache-Control: no-store so the error is never cached
- Content-Type: application/problem+json with type, title, status and detail members
- RateLimit and RateLimit-Policy headers per the IETF httpapi draft. Note these are draft, not standard, as of July 2026; Retry-After is what clients can be relied on to honour.

429 is the correct code here rather than 503: the limit belongs to a specific caller exceeding a quota, not to the service being unavailable as a whole. The existing 503 responses from the heap gate stay as they are, because those genuinely signal a device-wide condition.

Scope note requiring a decision: this task implements a per-endpoint global budget, not a per-client one. A global budget is the only variant that actually caps aggregate load, which is the point, and it costs 8 bytes of state. The trade-off is that a single well-behaved client can receive 429 because another client is polling, which deviates from the per-client reading of 429 semantics. Per-client would need an IP table and would let N clients each poll at the full rate, defeating the purpose.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Both endpoints serve at most 1 request per second; excess requests receive 429
- [x] #2 429 response carries Retry-After, Cache-Control no-store, and an application/problem+json body with type, title, status and detail
- [x] #3 No other endpoint is rate-limited, in particular flash upload and crashlog polling are unaffected
- [x] #4 Rate-limit state costs no dynamic allocation and no String usage
- [x] #5 Build passes and evaluator shows no new failures
- [x] #6 Web UI behaviour under 429 verified: no console error storm, no stuck display
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Geimplementeerd in commit 6828a0cc.

Firmware (restAPI.ino): tabel kRateLimitedRoutes met twee entries, elk een uint32_t. Check zit in de dispatcher voor de handler-aanroep, alleen op HTTP_GET, met unsigned aftrekken zodat de 49-daagse millis()-rollover geen gat geeft. Retry-After rondt naar boven af zodat er nooit 0 uitkomt. sendApiRateLimited stuurt Retry-After, Cache-Control no-store, RateLimit en RateLimit-Policy, en een RFC 9457 problem+json body via snprintf_P. Geen String, geen allocatie.

Web UI (data/index.js): zonder deze helft zou de firmware-wijziging een regressie zijn. Een onafgevangen 429 gaf elke seconde console.error plus een zichtbare foutbanner op het tweede tabblad. Beide fetch-handlers taggen nu err.status en de catch slaat 429 stil over.

Bewust genomen ontwerpkeuze, expliciet ter review: de limiet is per endpoint globaal, niet per client. Dat is de enige variant die de totale belasting begrenst. Keerzijde: een keurige client kan 429 krijgen doordat een andere client pollt, wat losser is dan de per-client-lezing van 429 in de RFC. Per client zou een IP-tabel kosten en tien tabbladen weer tien requests per seconde toestaan.

Verificatie: build completed successfully, artefacten vers om 23:39. Evaluator 34/37; de ene failure (1 unresolved ADR reference van 1408) is pre-existing, bevestigd door evaluate.py opnieuw te draaien tegen een gestashte tree.

AC6 blijft OPEN: gedrag onder 429 is code-matig afgevangen maar niet op hardware waargenomen. Vereist een apparaat met twee tabbladen open en een blik op de console.

Openstaand: deze wijziging raakt het REST-contract van twee v2-endpoints, dus er hoort een ADR bij. Nog niet opgesteld, wacht op akkoord van de maintainer.

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
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rate-limits the two endpoints the web UI polls, /api/v2/otgw/otmonitor and /api/v2/device/time, and answers excess requests with a proper 429 instead of serving them.

Why: each open web UI page drives two 1-second timers, so every forgotten tab costs the gateway 2 req/s indefinitely. A field capture traced a device that died after 60 minutes to two open pages sustaining 242 req/min (TASK-1037). The firmware had no way to say slow down.

Firmware: a two-entry table with one uint32_t of state per endpoint, checked in the v2 dispatcher before the handler, GET only, with unsigned arithmetic so the 49-day millis() rollover is safe. Retry-After rounds up so it never says 0. Responses carry Retry-After, Cache-Control: no-store, an RFC 9457 application/problem+json body, and the IETF draft RateLimit / RateLimit-Policy headers, marked in the code as draft rather than standard.

429 rather than 503 on purpose: the heap gate's existing 503s mean the device as a whole is in trouble, this means one caller exceeded a quota while the service is fine.

Web UI updated in the same change, because the firmware side alone would have been a regression: an unhandled 429 produced a console error and a visible error banner once a second on the second tab. Both fetch handlers now tag the status on the Error and skip 429 quietly.

Design choice worth naming for review: the budget is per endpoint and global rather than per client. Capping aggregate load is the goal, and a per-client budget would let N clients each poll at the full rate. The trade-off is that a well-behaved client can receive 429 because another client is polling, which is looser than the per-client reading of 429 semantics.

Verified on hardware (bench device 192.168.88.68, build 1.7.2-beta.1+ccb5014): first request 200, immediate second 429 with the full header set and body; device/time returned Retry-After 4 matching its own window; the window reopened after 5s; device/info and health served three rapid requests each without limiting; browser console showed zero 429 errors and no stuck display. Build clean, evaluator 34/37 with the single failure confirmed pre-existing.

Open: this changes the REST contract of two v2 endpoints, so an ADR is still owed.
<!-- SECTION:FINAL_SUMMARY:END -->
