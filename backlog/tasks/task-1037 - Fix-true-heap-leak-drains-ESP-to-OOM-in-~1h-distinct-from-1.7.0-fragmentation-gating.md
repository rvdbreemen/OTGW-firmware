---
id: TASK-1037
title: >-
  Fix: true heap leak drains ESP to OOM in ~1h (distinct from 1.7.0
  fragmentation gating)
status: To Do
assignee: []
created_date: '2026-07-19 09:45'
updated_date: '2026-07-20 21:05'
labels: []
dependencies: []
references:
  - >-
    Discord #nederlandse-ondersteuning msg 1527629013210632323 +
    1528302834876022795 (martreides
  - 2026-07-17/19); logs otgw-161.log
  - otgw-171.log
  - otgw-171-2.log
priority: high
ordinal: 156000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by martreides in Discord #nederlandse-ondersteuning (2026-07-17..19). OTGW V2.13 + WeMos D1 mini, PIC 6.6, reboots every 1-1.5h, HA unavailable. Three telnet captures (1.6.1 and 1.7.1) show an identical signature: heap flat around 20 KB for ~35-40 min after boot, then a monotone ramp down of roughly 800-1000 bytes/min until OOM about 15-20 min later, followed by Exception (2) epc1=0x40233cba excvaddr=0x00000008 (the known null 'new stcMDNS_RRAnswer' in LEAmDNS under OOM - a downstream symptom, not the cause).

This is NOT the fragmentation problem 1.7.0 gated. Evidence: free heap and maxBlock fall together (frag stays ~3%), and emergencyHeapRecovery reports 'before=888 after=888 delta=+0 actions=0x06' - recovery reclaims zero bytes, meaning everything allocated is still referenced. The 1.7.0 gates (canPublishMQTT, canServeHttp) are visibly working in the 1.7.1 capture and still cannot prevent death, because they throttle MQTT/HTTP/WS but do not gate mDNS allocations.

Cross-checks: 1.6.1 capture ran with NTP off and shows the same decay, so NTP/timezone lookup is exonerated. Version-independent across 1.6.1 and 1.7.1 points at a vendored library or Arduino Core leak rather than firmware feature code. Possibly related to the earlier geo83_44083 reports.

Ramp onset has no logged event in any of the three captures; all three were captured with a telnet client attached (and one with the web UI open, verifyAccess visible), so the leaking subsystem cannot be isolated from these logs alone.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Leak reproduced locally or on a bench device with a heap trace that identifies the allocating subsystem
- [ ] #2 Confirmed whether the leak persists with no telnet client and no web UI open (capture-mqtt-debug + USB serial from reporter)
- [ ] #3 Root cause identified at file/function level and documented with evidence
- [ ] #4 Fix keeps free heap stable over a 4h+ soak on a device previously showing the ramp
- [ ] #5 Reporter martreides confirms no reboots over 24h on a build with the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-19: Analysed both 1.7.1 telnet captures plus the 1.6.1 full capture (transcript-20260719-094605, OTGW-logs).

Crash chain, as far as the logs support it:
1. Heap descends ~800-1000 B/min from ~20 KB, accelerating. Source still unknown.
2. Around 5 KB free, maxBlock drops under 2048, canServeHttp() closes and never reopens (see TASK-1039).
3. MQTT gate follows at maxBlock < 1536. Publishing stops, HA marks the device unavailable.
4. emergencyHeapRecovery fires with delta=+0 every time (see TASK-1038).
5. Heap reaches ~880 bytes, maxBlock ~480. MDNS.update() at OTGW-firmware.ino:427 is ungated and runs ~1000x/s.
6. Next inbound mDNS query hits the unchecked new in _readRRAnswer(), gets NULL, constructor writes at this+8: epc1=0x40233cba excvaddr=0x00000008. Matches the decoded address exactly.

Ruled out for this reporter: the WS live-log path (emergencyHea actions never had bit 0x01 set, so no browser was connected) and NTP (the 1.6.1 capture ran with NTP off and decayed identically).

Speculative, not proven: mDNS may be both the leak and the crash site. LEAmDNS keeps per-query answer lists that grow, which fits the accelerating rather than linear decay; it is identical Core 2.7.4 code in 1.6.1 and 1.7.1, which fits version independence; and it is the confirmed crash site. Cheap discriminating test is a 1.7.1 build with MDNS disabled: flat heap means mDNS is both, continued decay without crashes means mDNS is only the crash site and we keep a live device to measure on.

Note on the 1.6.1 full capture: it is NOT usable as leak evidence. The capture script drives a headless Edge at 365 REST requests/min, and that run shows the fragmentation profile (maxBlock pinned at 5352 while free stayed 10-13 KB), not the leak ramp. Also found: his broker holds stale retained HA discovery from the 1.7.1 era (sw_version 1.7.1+c50cbcc on hvac_mode, hvac_action, uptime, fragskips, *_override) that the running 1.6.1 never fills, so those entities sit unavailable in HA independently of the reboots.

Related: TASK-1038 (recovery no-op), TASK-1039 (HTTP gate latch).

Discriminating experiment tracked as TASK-1040 (build 1.7.1-no-mdns.1).

2026-07-19 avond: eerste capture met het nieuwe browservrije script (transcript-20260719-210025, 1.7.1+c50cbcc, device 48E72958B013). Run 21:00 tot 22:00, apparaat stierf tijdens de meting.

Verloop: vrije heap schommelt 50 minuten rond 19-20 KB terwijl maxBlock vastgepind staat op 10976 (25% frag in de banner). Vanaf 21:50 zakken beide samen: 16224 -> 13240 -> 11008 -> 7928. Om 22:00:21 weer emergencyHeapRecovery met before=1352 after=1352 delta=+0. Laatste regel 22:00:22: HEAP-FRAG skip MQTT (maxBlock=760, heap=1608). De twee Exception (2)-regels in dit bestand staan in de crashlog-sectie en zijn historie, niet live.

BELANGRIJKSTE BEVINDING: er stonden TWEE OTGW-webpagina's open tijdens de meting, terwijl de tooling-browser uit stond (-SkipBrowserCapture bevestigd in de summary). Aangetoond via de poll-architectuur in data/index.js: per open pagina lopen twee onafhankelijke 1s-timers, index.js:269 (refreshOTmonitor -> /api/v2/otgw/otmonitor) en index.js:281 (refreshDevTime + refreshGatewayMode -> /api/v2/device/time, en gethrottled /api/v2/device/info). Per tabblad dus 60 + 60 + ~1 per minuut.

Gemeten over 60 minuten: otmonitor 7192 (121/min), device/time 7190 (121/min), device/info 118 (~2/min). Alle drie exact factor 2 boven het per-tabblad-verwachtingspatroon. Twee sessies, sluitend over drie onafhankelijke endpoints.

Dat verklaart ook de 365/min in de browsergedreven capture van vanochtend: zijn twee sessies plus onze headless Edge.

CORRECTIE op eerdere notitie: de claim dat het lek ook zonder weblast optreedt is NIET houdbaar. In otgw-171.log stond de REST-toggle op [0], dus REST-verkeer werd daar niet gelogd. Afwezigheid van REST-regels betekende daar "niet gelogd", niet "niet gebeurd". Die vergelijking is ongeldig.

Volgende stap is goedkoop en sluit direct iets uit: alle OTGW-tabbladen dicht en kijken of het apparaat dan langer dan anderhalf uur haalt. Geen nieuwe firmware nodig.

UITSLAG DISCRIMINEREND EXPERIMENT (TASK-1040), capture transcript-20260720-191135, build 1.7.1-no-mdns.1+5475fee, device 48E72958B013, boot 19:11, dood 20:34:47 (83 min uptime).

VERDICT: mDNS is NIET het lek. Het verval treedt zonder mDNS gewoon op. Maar het beeld is genuanceerder dan een simpel nee.

1. NIEUW EN OPVALLEND: 59 minuten lang stond de heap byte-identiek stil op 20992 vrij / 14416 grootste blok, sample na sample. Dat is in geen enkele eerdere capture voorgekomen; stock-builds schommelden altijd honderden bytes. Het weghalen van mDNS elimineerde alle idle-churn volledig.

2. Vanaf 20:12 alsnog een monotoon en versnellend verval: 20992 -> 4528 in 22 minuten, van ~100 B/min oplopend naar ~1500 B/min.

3. Het toestel is gestorven om 20:34:47. Bewijs: laatste telnet-regel 20:34:47, capture pas gestopt 20:40:14, en het toggle-herstel meldde "partial (1 of 2) - An existing connection was forcibly closed by the remote host". De verbinding was dus al hard weg.

4. GEEN crash-signatuur beschikbaar voor deze dood. De reboot_log is alleen bij boot en bij poll #2 opgehaald, beide voor 20:34. Wat er in reboot_log staat is historie van voor deze build (Software/System restart, External Watchdog, en een oudere Exception (2)). Om te weten hoe deze build stierf is de reboot_log van de VOLGENDE boot nodig.

5. Onset valt samen met twee gebeurtenissen in dezelfde minuut: NTP-resync om 20:11:31 (met timezone-lookup) en crashlog-poll #2 om ~20:11:37 (de capture-samenvatting bevestigt "2 polls"). Allebei hadden echter een onschuldige eerste keer: NTP om 19:41:30 zonder enig effect (heap bleef exact 20992), crashlog om 19:11:37 idem. Dat pleit tegen een simpel lek-per-gebeurtenis.

6. NIET veroorzaakt door MQTT- of OT-belasting. OTGW-publicaties liggen vlak op 195-272 per 5 minuten over de hele run, ook dwars door de onset heen. OT-frames idem.

7. De discovery-republish (119 configs) zit NA uptime 4847s, dus na 20:32, twintig minuten NA de onset. Gevolg, geen oorzaak; vermoedelijk een MQTT-herverbinding onder heap-druk.

BLINDE VLEK, en dit is nu het belangrijkste punt: met -QuietDebugToggles staat REST-logging uit, dus browser- en REST-verkeer is in deze capture ONZICHTBAAR. Uit de vorige run weten we dat deze gebruiker twee tabbladen open had. Een browser die om 20:12 opengaat zou exact dit patroon geven en zou hier niet te zien zijn.

CONSEQUENTIE VOOR DE TOOLING: het stille script loste instrument-perturbatie op en creeerde een nieuw gat. De volgende meting moet OTmsg, MQTT, MQTTGate en Sensors stil houden maar REST API AAN laten. Dat is precies de verdachte belasting, en die kost weinig logvolume.

2026-07-20: derde capture (transcript-20260720-212118, beta.1 525921b, martreides device). Zelfde signatuur: boot 21:20, heap byte-stabiel ~20048 gedurende 63 min, collapse vanaf 22:24, dood ~22:43. NTP-resyncs 21:50 (geen effect) en 22:20 (onset +4min). Bevestigt over drie captures: verval begint na de TWEEDE resync, eerste is onschuldig.

TIMEZONE/ZONE-PROCESSING UITGESLOTEN als lek. Bewijs, niet vermoeden:
- ExtendedZoneProcessorCache<CACHE_SIZE=3> (OTGW-firmware.h:523-527) is statisch, compile-time gealloceerd. createForZoneName haalt een processor uit die vaste pool en alloceert geen heap per aanroep.
- createForZoneName draait ELKE SECONDE via minuteChanged() (helperStuff.ino:700, aangeroepen op OTGW-firmware.ino:462). In de 63 min voor de collapse ~3800 aanroepen, heap byte-identiek 20048. Een per-call lek zou de heap nooit stabiel laten. Dit weerlegt de eerdere hypothese dat createForZoneName intern alloceert.
- Geen enkel timezone-spoor in de fout: geen Timezone Invalid, geen assert, geen zone-error, geen heap-junk. De enige tz-regel is "Starting timezone lookup for [Europe/Amsterdam]" per resync, en die gebruikt dezelfde gratis cache. Rode haring.
- De eerste resync doet dezelfde lookup zonder schade.

Correctie op eerdere sessie-notitie: doTaskMinuteChanged roept hour/day/yearChanged aan die ELK opnieuw createForZoneName doen (helperStuff.ino:673-720), dus 4 aanroepen op een minuutwissel en 1 per seconde anders. Inefficient (aparte TimeZone per helper i.p.v. gedeeld), maar heap-neutraal door de statische cache. Geen lek.

RESTEREND, niet te scheiden in deze capture: onset valt samen met tweede NTP-resync (22:20) EN tweede crashlog-poll van het script (~22:21, "2 polls"). Minuten uit elkaar, niet isoleerbaar. Plus REST-verkeer onzichtbaar (oude -QuietDebugToggles capture), dus browser-activiteit rond de onset niet te zien.

VOLGENDE: beta.2 (NTP 1x/dag) isoleert de NTP-tak; nieuwe capture met -KeepDebugToggles "REST API,NTP" maakt REST + resync beide zichtbaar in een run.
<!-- SECTION:NOTES:END -->
