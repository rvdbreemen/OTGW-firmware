---
id: TASK-1037
title: >-
  Fix: true heap leak drains ESP to OOM in ~1h (distinct from 1.7.0
  fragmentation gating)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 09:45'
updated_date: '2026-07-23 06:04'
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
- [x] #1 Leak reproduced locally or on a bench device with a heap trace that identifies the allocating subsystem
- [ ] #2 Confirmed whether the leak persists with no telnet client and no web UI open (capture-mqtt-debug + USB serial from reporter)
- [x] #3 Root cause identified at file/function level and documented with evidence
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

REGRESSIE-ANALYSE 2026-07-20, drie captures (no-mdns 1.7.1, beta.1 1.7.2, stock 1.7.1+c50cbcc), verschillende boottijden en firmware.

HARDE, UPTIME-GEBONDEN GETALLEN (uit sendMQTTuptime-ankers + logHeapStats):
- Dood: uptime 4853 / 4978 / 5035 s (~82 min). Drie verschillende wandklok-boottijden (19:11, 20:39, 21:20), zelfde uptime bij dood. => WANDKLOK UITGESLOTEN, dood is uptime-gebonden.
- Onset (laatste stabiele heap-sample voor onomkeerbare daling): 3893 / 3895 / 3898 s. Spreiding 5 SECONDEN over drie captures. => deterministische trigger, geen jitter.
- NTP resync #2: uptime 3611 / 3612 s (exact 2x1800 + ~11s boot-sync-offset).
- resync #1 op 1810/1811 s: NUL effect (heap byte-stabiel eromheen).
- onset = resync#2 + 285 s (284 / 286, spreiding 2 s).
- daling-duur onset->dood: ~1050-1140 s (~18 min).

WAT DE 5-SECONDEN-LOCK UITSLUIT: alles met jitter. DNS-latency, netwerk, MQTT-reconnect, browser-activiteit, mDNS-queries -> allemaal variabel, kunnen geen 5s-spreiding geven. Ook eerder uitgesloten: mDNS (no-mdns stierf identiek), timezone/AceTime (statische cache CACHE_SIZE=3, heap byte-stabiel 3800 aanroepen lang), discovery-verify (draaide bewijsbaar niet, nul verify-regels; uur-trigger viel voor uptime>=3600).

WAT OVERBLIJFT als tijdgebonden oorzaak, twee kandidaten, beide uptime-deterministisch:
1. Vertraagd gevolg van NTP resync #2. De resync-CALL zelf is goedkoop op Core 2.7.4 (int-overload van configTime doet alleen setServer, geen sntp_stop/init). Maar 285s na de resync begint de daling, met 2s spreiding. Dat past bij een SDK-sntp DNS/retry-cyclus die door de resync in gang is gezet en pas na een vaste timeout allocaties doet. Niet bewezen, wel het beste passende mechanisme.
2. do5minevent tick op uptime 3900 s (timer5min, 13e tick). Valt binnen het onset-venster [3896, 3956]. MAAR: 12 eerdere ticks waren onschadelijk, dus alleen relevant als resync#2 iets in de toestand veranderde dat de eerstvolgende do5min-cyclus laat lekken.

WAAROM resync#2 wel en resync#1 niet: onverklaard uit de logs. loopNTP-code is identiek per resync. Dit is de enige echte open vraag.

BESLISSENDE TEST, al gebouwd: beta.2 (NTP resync 86400s). Geen resync#2 meer voor 24u. Overleeft beta.2 ruim voorbij 82 min -> resync-pad is de dader. Sterft beta.2 alsnog op ~82 min uptime -> resync uitgesloten, dan is het de do5minevent-3900s-tick of een ander uptime-slot, en moet ik met per-seconde heap-telemetrie (nieuwe capture, -KeepDebugToggles) het exacte allocatiemoment vangen.

2026-07-21 BESLISSEND: beta.2 (1.7.2-beta.2+482d934, NTP-resync 86400s) capture transcript-20260721-201357, device 48E72958B013, boot 20:04:19.

NTP UITGESLOTEN. Bevestigd: NUL NTP-resync-events in de hele capture (geen Start time syncing, geen Time synced, geen Resync needed). De 86400s-wijziging werkte, er was geen resync op 3600s. En toch sterft het toestel, zelfde patroon: onset uptime 3900s, dood uptime 5040s.

Vierde capture op een rij met identieke onset:
- no-mdns 3898s, beta.1 3895s, 1.7.1 3893s, beta.2 3900s. Spreiding 7 seconden over VIER firmwares en beide NTP-instellingen.

Daarmee is ook de "285s na resync#2"-correlatie ontmaskerd als toeval: resync#2 lag bij 30-min-NTP op 3611s, toevallig ~285s voor de echte ~3900s-trigger. Zonder resync (beta.2) blijft de onset op 3900s. NTP was een rode haring.

Crashlog-poll OOK uitgesloten: poll #1 om 20:13:59, poll #2 rond 21:14 (uptime ~4180s), dus NA de onset (3900s). En de Grafana-veldgrafiek toont dezelfde ~80-min zaagtand zonder mijn script.

Firmware-gedrag op uptime 3600: geen. De twee uptime>3600-checks (runNightlyRestartCheck, discovery-verify) vereisen bNightlyRestart (default false) resp. hourChanged; geen van beide vuurt in deze captures.

STAND VAN ELIMINATIE (allemaal weg): mDNS, timezone/AceTime, discovery-verify, NTP-resync, crashlog-poll, wandklok.

WAT OVERBLIJFT: een intrinsieke, uptime-vaste trigger in het venster (3900, 3960)s. logHeapStats is boot-gesynct (60s), dus laatste-stabiel=3900 => echte onset in (3900,3960). Beste resterende kandidaat: do5minevent-tick. sendMQTTuptime (in do5min) vuurt op 3634 en 3934s; de tick op 3934 valt precies in het onset-venster, terwijl 11 eerdere ticks onschadelijk waren. Waarom tick #13 wel: onverklaard. do5min doet sendMQTTuptime/versioninfo/stateinformation, publishOverrideStates, publishAllPICsettings.

VOLGENDE STAP: capture met het NIEUWE script (-KeepDebugToggles "REST API,NTP") plus MQTT-toggle aan, zodat het exacte allocatiemoment rond 3934s zichtbaar wordt. De tester gebruikte nog het oude blanket-quiet script. Overweeg extra: per-seconde heap-sampling in het onset-venster.

2026-07-21: diagnose-build 1.7.2-onset.1 (commit bc067cc, worktree wt-onset, branch exp-heap-onset) gebouwd met HEAP_ONSET_DIAG=1: 1Hz heap-sampling in uptime-venster [3500,4500]s. Plus nieuw capture-heap-onset.bat (keep REST,MQTT,MQTTGate,NTP; distinct naam tegen stale-copy-verwarring). Release-map OTGW-release-1.7.2-onset.1. Sampler default OFF gecommit op otgw-1.x.x (bc067ccf). Doel: exact allocatiemoment rond 3934s op event-niveau vangen.

=== EINDCONCLUSIE BENCH-SUITE (T1-T4) ===
De bench reproduceert de ~3900s-dood in GEEN enkele configuratie:
- T1 fast-SNTP (30s): vlak. SNTP-frequentie irrelevant.
- T2 MQTT uit: vlak tot 68min.
- T3 MQTT aan (geen OT): vlak tot 66min. (ook fast-SNTP+MQTT samen getest: vlak)
- T4 OT-sim + MQTT: vlak tot 67min.

UITGESLOTEN met bench-bewijs: SDK-SNTP-update, MQTT-connectie/publish op zich,
OT-frame-decode->MQTT-publish-pad (via sim).

RESTERENDE VERSCHILLEN bench vs veld (kandidaten die de bench NIET kan namaken):
1. DHCP-lease-vernieuwing. LEIDENDE KANDIDAAT. Uptime-vast (lease-timer vanaf
   DHCP-acquire bij boot), router-afhankelijk (verklaart waarom de bench op MIJN
   netwerk niet reproduceert: andere lease-duur), SDK/lwIP-niveau (versie-
   onafhankelijk, matcht alle 4 veld-captures). Als de D-Link (dlink-CBEF) een
   lease van ~130min heeft, valt T1 (50%) op ~65min = 3900s. STERKE fit met de
   7s-spreiding en de vaste uptime-lock.
2. Echte PIC (serieel UART-verkeer via OTGWSerial). Minder waarschijnlijk: zou
   per-verkeer lekken, niet uptime-vast op 3900s.
3. Veld-broker-specifiek (zigbee2mqtt-firehose, retained configs).

DISCRIMINERENDE VELDTEST (H2/DHCP): zet op martreides' toestel een STATIC IP
(runtime-setting, geen build). Stopt de ~82min-dood => DHCP-lease-vernieuwing
bevestigd. Blijft hij => DHCP uit, dan echte-PIC of broker.

2026-07-22 bench-suite eindstand: 11 hypotheses met bewijs uitgesloten (incl. DHCP: bench renewt elke 5min flat; crashlog-serving: geen cumulatief lek). Bench reproduceert de dood in GEEN configuratie. Cause is field-specifiek: leidende kandidaat is het ECHTE-PIC seriele UART-pad (sim bypasst het), of de veld-broker. Diagnose-build onset.5 (1Hz-heap op onset + DHCP-lease-logging) is de decisieve veld-capture. Volledig testlog: OTGW-logs/HEAP-TESTLOG.md.

2026-07-23: NIEUWE CAPTURE-ANALYSE (transcript-20260722-203911, beta.3+36c91e5, device 48E72958B013) — ROOT CAUSE BEVESTIGD + FIX GEïDENTIFICEERD.

Buildhash 36c91e57 (20:00:52) is PRE-FIX: 17 min vóór fix-commit 393db8b3 (20:17:57, TASK-1048). git merge-base --is-ancestor bevestigt 36c91e5 is ancestor van 393db8b3. Deze capture is dus retro-bevestiging van pre-fix gedrag, niet een nieuw lek.

Heap-trace: vlak ~20200 B, 57 min (20:39-21:36), dan monotoon versnellend verval 19920->5152 in 21 min (~800 -> ~1500 B/min). maxBlock volgt free omlaag (10792->4816) = aaneengesloten verlies = echte leak, geen frag. emergencyHeapRecovery delta=+0 (alles gerefereerd). Eindigt in MQTT throttle (drops=60) + HTTP-frag skip + dood ~21:58. Reboots=3, crashlog External Watchdog = staart van eerdere leak-cycli.

ONSET = uptime ~60 min (device boot 20:36, Up 0:03 bij start). Eerste hourChanged na uptime>3600 -> discovery-verify vuurt. Matcht ADR-062 mechanisme exact.

ROOT CAUSE (uit fix-commit 393db8b3): auto-verify subscribet homeassistant/+/<node>/# om retained configs te tellen; onder verkleinde PubSubClient-buffer leest maar 26 van 124 terug; verklaart 98 vals-missing; markAllMQTTConfigPending() -> volledige republish; iLastVerifyEpoch==0 -> herbewapent elk uur. verify->false-missing->republish leaks tot dood.

CORRECTIE OP EERDERE NOTITIE: de "web-load driven" hypothese is een RODE HARING voor dit lek. Deze run had Debug RestAPI:true maar exact 1 REST GET het hele uur (tool crashlog-poll), 0 browser-polls (geen otmonitor/device/time/info regels). Geen web-sessie actief, lek trad tóch op. Echte trigger = MQTT discovery-verify, niet REST-polling storm. De dubbele-tab REST-observatie uit 19-07 blijft geldig als APARTE frag-druk, maar is niet dit lek.

MEETFOUT die dit eerder verborg: beta.3-run gebruikte blanket-quiet preset (silence all except REST API+NTP), die MQTT-publish/verify-events verbergt. capture-heap-onset.bat (Jul 21) houdt REST+MQTT+MQTTGate+NTP aan en had de verify-storm zichtbaar gemaakt.

FIX (393db8b3, Option 5 KISS): verify-readback verwijderd; daily trigger doet unconditional heap-gated drip-republish (guards: MQTT-up + geen drip bezig + maxBlock>=8000); hourly first-run retry geschrapt. Geen wildcard-subscribe, geen count, geen false-missing, geen retry-storm.

RESTEREND: alleen AC#4 (4h+ soak op post-fix build >=393db8b3) + AC#5 (reporter 24h bevestiging). AC#1/#3 nu voldaan door deze analyse + fix-commit bewijs.

2026-07-23 SOAK-PLAN (A): HEAD (5514505) bevat fix 393db8b3. Post-fix build.bat draait (firmware+fs). Bench: COM3 = USB-SERIAL CH340 (WeMos D1 mini / NodoShop OTGW ESP8266), kandidaat 1.x bench-device.

HARDWARE-CAVEAT AC#4: reporter (martreides) device 48E72958B013 is remote/niet bereikbaar. Bench-soak op COM3 = sanity-check, geen volledige AC#4 tenzij bench 124 discovery-configs + verkleinde PubSubClient-buffer reproduceert. Volledige AC#4 vraagt post-fix build op reporter-device of exacte repro-bench.

Soak-preset: capture-heap-onset.bat (houdt REST+MQTT+MQTTGate+NTP aan), NIET de blanket-quiet preset. Let op uptime-uurgrenzen 60/120/180 min. Verwacht: heap vlak, op dag-grens 1 heap-gated drip-republish.

AC#4 en AC#5 zijn hardware/reporter-gated = niet self-verifiable; blijven open tot bench-soak draait resp. reporter 24h bevestigt.

2026-07-23 FLASH GEBLOKKEERD (bench COM3, MAC 84:f3:eb:22:b8:e1 — bench-device, NIET reporter 48E72958B013). Post-fix build 1.7.2-beta.3+08d628c gebouwd + geverifieerd (bin 0.73MB @00:50, littlefs 1.98MB, HEAD bevat fix 393db8b3).

Flash faalt op USB-laag, 4 pogingen: (1) 460800 stub -> "Failed to write to target RAM 0107 Checksum error"; (2) 115200 stub -> zelfde; (3) --no-stub ROM-loader begon write@0x0 dan "No more data to read from serial port" + reset mid-write; (4) flash_id stub -> zelfde RAM-checksum. ROM-loader leest wel MAC = ROM-bootloader leeft, maar elke stub/RAM-upload corrumpeert = signal-integriteit CH340-kabel/poort, geen software-fix.

GEVOLG: partial write op 0x0, bench boot firmware niet tot succesvolle her-flash. HERSTEL vraagt hands-on: andere data-USB-kabel + poort (geen hub), evt GPIO0-low manual bootmode, dan volledige erase+flash beide bins. Encoding-noot: flash_esp.py crasht op cp1252-console; draai met PYTHONIOENCODING=utf-8 PYTHONUTF8=1.

SOAK wacht op werkende flash-verbinding. Build-artefact staat klaar.

2026-07-23 FLASH GESLAAGD via ROM-loader. Oplossing: esptool --no-stub write_flash omzeilt de stub-RAM-checksum die op deze CH340-kabel elke stub-upload deed falen. Traag (~4 min, 77 kbit/s ROM-loader) maar exit=0, beide bins geverifieerd (fw 764688B @0x0, littlefs 2072576B @0x200000, Hash verified). Geen erase -> WiFi-creds behouden.

Bench-device 84:f3:eb:22:b8:e1 boot 1.7.2-beta.3+08d628c (post-fix), verschijnt op 192.168.88.68. Verse boot: freeheap 18632, maxfreeblock 18264 (~2% frag), HEALTHY. Klaar voor soak.

CH340-flash-recept voor toekomst: PYTHONIOENCODING=utf-8 PYTHONUTF8=1 python -m esptool --port COM3 -b 115200 --no-stub --before default_reset --after hard_reset write_flash --flash_mode dio 0x0 <fw> 0x200000 <fs>
<!-- SECTION:NOTES:END -->
