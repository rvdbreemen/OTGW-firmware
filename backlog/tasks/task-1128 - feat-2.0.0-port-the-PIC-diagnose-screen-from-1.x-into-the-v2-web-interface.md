---
id: TASK-1128
title: 'feat-2.0.0: port the PIC diagnose screen from 1.x into the v2 web interface'
status: Done
assignee:
  - '@claude'
created_date: '2026-09-05 15:31'
updated_date: '2026-09-05 20:56'
labels:
  - feature
  - webui
  - pic
dependencies: []
priority: medium
ordinal: 276000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the PIC diagnose screen shipped on otgw-1.x.x (TASK-1127, v1.7.6-beta.2) to the 2.0.0 line, v2 shell only. The PIC can run Schelte Bron's diagnostic firmware, which replaces OpenTherm with an interactive text test menu on the serial line; today that needs a separate terminal tool. Not a mode: the page exists when device/info.picfwtype is 'diagnose', which a PIC-less board never reports at all. Transport is an adaptation, not a copy: 2.0.0 already has the byte-transparent raw stream (TASK-1111) with a single splice point in drainOTRawQueue(), but its producer is gated off by default, widening it forces a consumer gate, and the write path must go through enqueuePICTx() because direct UART writes are barred by both the linter and the threading model. Design reuses the existing v2 .console and .cmdbar components, so the whole port needs one new CSS rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A diagnose page exists in the v2 shell and is reachable only while device/info reports picfwtype 'diagnose'; it is absent on a gateway PIC and on any board without a PIC
- [x] #2 The PIC menu appears when the page is opened, without the user having to send anything first
- [x] #3 Typed input reaches the PIC and its output appears on the page, verified live against a diagnose PIC on real hardware
- [x] #4 picfwtype rides on the 15s health poll so a PIC reflash is noticed without a page reload
- [x] #5 The page is built from existing v2 components and adds no more than one new CSS rule
- [x] #6 esp32-classic and esp32-combo build green, evaluate.py --quick shows no new failures, node --check passes on v2.js
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Geimplementeerd en live geverifieerd op de bank-ESP32-S3 (192.168.88.63, esp32-classic, alpha.363) tegen een pic16f1847 met diagnose-firmware 2.2.

Firmware: producer-gate verbreed naar een diagnose-PIC, consumer-gate toegevoegd zodat er niets naar een uitgeschakelde poort 25238 gaat, forwardDiagnoseChunk op de OT-log-websocket achter een STX-marker, POST /api/v2/otgw/diagnose via enqueuePICTx, en picfwtype toegevoegd aan de health-poll.

Frontend: zesde pagina in de v2-shell, opgebouwd uit .card, .cmdbar en .console. Precies een nieuwe CSS-regel (.seg.hidden). DIAG-tak staat voor rawFromLine in onWsMessage. Nav-item verschijnt bij detectie, pagina opent zich eenmalig via een latch, en een stale localStorage-restore van de diagnose-pagina wordt genegeerd.

Waargenomen gedrag: nav-item verschijnt, pagina opent zichzelf, menu wordt bij openen opgehaald, 1 start de LED-test en blijft stil zoals gedocumenteerd, lege Enter verlaat de test en tekent het menu opnieuw.

Bijvangst: het scherm legde direct een echte bug bloot. Tussen de menuregels verschijnt ESP-IDF-logtekst die de PIC terugechoot en met Invalid test beantwoordt, wat betekent dat de ESP die tekst de PIC-UART in schrijft. Vastgelegd als TASK-1131.

Open: AC 6 (build en gates) wacht nog op een combo-build, en het scherm is nog niet getoetst met een gateway-PIC om te zien dat het verdwijnt.

Gates rond: esp32-classic en esp32-combo beide SUCCESS op alpha.363, evaluate.py 68/76 zonder failures, node --check schoon op v2.js. Alles in een commit met de bump erbij (ae5c3de2d).

Nog niet gedaan en bewust apart gehouden: toetsen dat het scherm verdwijnt zodra de PIC weer gateway-firmware draait. Dat vraagt een PIC-flash en dus een expliciete toestemming per keer.

Negatieve pad alsnog getoetst zonder te flashen, door de health-respons te onderscheppen en picfwtype op gateway te zetten. Resultaat: nav-item verdwijnt en de actieve pagina springt terug naar home. Daarmee is AC 1 op beide helften gedekt: aanwezig bij diagnose, afwezig bij gateway, en structureel afwezig op een bord zonder PIC omdat het veld dan niet wordt verstuurd.

Verse pagina-load geeft nul console-fouten. De 28 fouten die eerder in de sessie zichtbaar waren kwamen uit mijn eigen fetch-onderschepping, niet uit de pagina.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Brings the PIC diagnose screen to the 2.0.0 line, v2 shell only, verified on hardware against a live pic16f1847 running diagnostic firmware 2.2.

The screen is a sixth page in the v2 shell, built from the existing .card, .cmdbar and .console components, adding exactly one CSS rule. It is not a mode: it exists while picfwtype reports diagnose, and a board without a PIC never sends that field, so one test covers firmware type and hardware alike.

The transport was the real work and needed adapting rather than copying. The raw producer was gated on the legacy 25238 port, which defaults off, so a splice there would have been dead code; widening it to admit a diagnose PIC forced a consumer-side gate so bytes cannot reach a port the user switched off, while the drain still dequeues unconditionally and cannot pin. Output is mirrored onto the OT-log WebSocket behind an STX marker, tested before rawFromLine so menu text cannot be mistaken for an OpenTherm frame. Input goes through enqueuePICTx, because writing the UART from a REST handler is barred by both the linter and the threading model, and the endpoint reports a real result since that call can fail. picfwtype now rides the health poll, which is what lets a PIC reflash be noticed without a page reload.

Two deliberate divergences from 1.x, both for nativeness: input sits above output as on the Monitor page, and there is no auto-home, because v2 restores the last page well before it knows the hardware. The nav entry is revealed on detection, the page opens once behind a latch, and a stale restore of the diagnose page is ignored.

Verified live: nav appears, page opens itself, menu is fetched on open, typing 1 starts the LED test and stays silent as documented, an empty Enter leaves the test and redraws the menu, and feeding gateway back through the health poll hides the entry and returns to home.

Gates: esp32-classic and esp32-combo both SUCCESS, evaluate.py 68/76 with 0 failures, node --check clean, zero console errors on a fresh load.

Follow-ups raised: TASK-1131 for ESP-IDF log output leaking onto the PIC UART, which this screen made visible, and ADR-177 proposed for keeping the raw byte stream verbatim.
<!-- SECTION:FINAL_SUMMARY:END -->
