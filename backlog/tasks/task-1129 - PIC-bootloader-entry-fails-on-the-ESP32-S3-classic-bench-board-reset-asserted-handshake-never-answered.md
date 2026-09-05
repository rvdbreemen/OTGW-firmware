---
id: TASK-1129
title: >-
  PIC bootloader entry fails on the ESP32-S3 classic bench board: reset
  asserted, handshake never answered
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-05 15:32'
updated_date: '2026-09-05 19:23'
labels:
  - bug
  - pic
  - hardware
dependencies: []
priority: high
ordinal: 277000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-972 AC#2 is blocked on this. Two authorized flash attempts on the classic-S3 bench board failed identically at FWSTATE_RSET with 'Too many retries' at 0 percent progress, meaning the PIC never entered bootloader mode. Normal serial comms are unaffected: PR=A returned a genuine live answer from the PIC after each attempt, so the PIC UART on 43/44 works. That asymmetry points at the reset path rather than the UART. Note boards.h carries two classic-on-S3 pin maps that differ exactly there: the plain LOLIN S3 Mini puts PIC reset on GPIO12, the S3 Mini Pro on GPIO40, while the UART is 43/44 in both. Only the combo build detects which board it is, via the on-board QMI8658C IMU; the esp32-classic build assumes the plain Mini unconditionally. The bench board reports MAC ac:27:6e:ce:45:d8 and emits I2C ESP_ERR_INVALID_STATE errors on the classic bus at every boot, so the external 0x26 watchdog is not answering either. A previously tested hypothesis that BLE task starvation delayed the handshake was refuted by retrying with BLE disabled.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The reason the PIC does not enter bootloader mode is identified, with evidence rather than inference
- [ ] #2 A PIC firmware flash started from the v2 UI completes on the bench board
- [ ] #3 The boot-time I2C failures on the classic bus are explained, and either fixed or recorded as expected for this unit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-05 hardware-onderzoek zonder netwerktoegang.

Bord geidentificeerd op COM8: ESP32-S3 (QFN56) rev v0.2, 4MB embedded flash, 2MB PSRAM, MAC ac:27:6e:ce:45:d8. Dat is het gedocumenteerde classic-S3 bankbord, niet de S3 Mini Pro (die heeft 9c:13:9e:f1:ab:08).

Het bord boot, maar is NIET bereikbaar: volledige ping-sweep van 192.168.88.0/24 vond alleen 1.x-apparaten (.35 en .68), otgw.local wijst naar .68, en er is geen OTGW provisioning-AP zichtbaar, ook niet na een harde reset via esptool. Verdere validatie is daarmee geblokkeerd tot het bord op WiFi zit.

Boot-console (USB-CDC) toont consistent I2C-fouten in de eerste 480 ms:
  i2cWriteReadNonStop(): i2c_master_transmit_receive failed: [259] ESP_ERR_INVALID_STATE
  Wire.cpp requestFrom(): i2cWriteReadNonStop returned Error 259
  i2cWrite(): i2c_master_transmit failed: [259] ESP_ERR_INVALID_STATE
ESP_ERR_INVALID_STATE duidt op een niet-geinitialiseerde I2C-driver op het moment van de aanroep, eerder dan op een NACK, dus dit kan ook een volgorde-probleem in de opstart zijn. Daarna is de USB-console stil, wat klopt: alle firmware-logging gaat naar telnet.

Codevergelijking 1.x versus 2.0.0: OTGWSerial::resetPic() is regel voor regel identiek, inclusief de 100 ms puls en de GW=R die eraan voorafgaat. Er is dus niets aan de resetroutine zelf geport.

Spoor dat de aandacht verdient (boards.h in src/libraries/Platform/src/): er bestaan twee pinmappen voor classic-op-S3 die precies op de resetlijn verschillen. Gewone LOLIN S3 Mini: PIN_PIC_RST 12, I2C 36/35. S3 Mini Pro: PIN_CLASSIC_PRO_PIC_RST 40, I2C 11/12. De PIC-UART is in beide 43/44. Alleen de combo-build detecteert het verschil, via de QMI8658C IMU op de Pro-bus; de esp32-classic build gebruikt de Mini-map onvoorwaardelijk.

Dat verklaart de klasse fout exact: een resetlijn op de verkeerde GPIO laat normale UART-communicatie intact (PR=A antwoordt) maar bereikt de bootloader nooit (0% voortgang, Too many retries op FWSTATE_RSET). Het MAC pleit tegen deze verklaring voor DIT bord, maar het is wel de plek om te kijken: verifieer met een scoop of logic analyzer of GPIO12 tijdens een flashpoging daadwerkelijk 100 ms laag gaat, en of dat pootje op deze unit met de PIC-reset verbonden is.

2026-09-05 avond, met netwerktoegang. Doorbraak in reproduceerbaarheid, en een verschuiving van de diagnose.

Bord opnieuw geflasht met verse alpha.362+ed79ac1 (eerst esp32-combo, daarna esp32-classic, beide merged-full op 0x0) en headless geprovisioneerd met bin/provision-wifi-ap.py. Het bord draait op 192.168.88.63.

DE FLASH IS NIET HET PROBLEEM, DE PIC-DETECTIE IS DAT. Het telnet-commando p (Manual reset PIC) geeft:
  detectPIC( 970): No ETX found after reset: no Pic detected!
  detectPIC( 971): All PIC-related functions are disabled (no PIC-based OTGW detected)
Dat is dezelfde handshake-fase als waar de flash op strandde (FWSTATE_RSET), maar nu met een enkele toetsaanslag te reproduceren in plaats van met een flashpoging. Elke verdere diagnose kan dus goedkoop en zonder risico voor de PIC.

Twee onafhankelijke builds komen tot dezelfde conclusie:
- esp32-combo: hardware.mode = OT-Direct. De boot-detectie vond geen PIC en koos de OTDirect-engine. Er stroomden wel OT-frames (Request Boiler R80000100), dus die kwamen uit OTDirect, niet uit een PIC.
- esp32-classic: hardware.mode = Degraded. Deze build verwacht een PIC, vindt er geen, en valt terug.
In beide gevallen antwoordt het commando a (PR=A) met "PIC not available or not active in this mode".

Dat is een verschil met juli, toen op ditzelfde MAC (ac:27:6e:ce:45:d8) na elke mislukte flash nog wel een echte PIC antwoordde met firmware 6.6, pic16f1847, gateway. Er is sindsdien dus iets veranderd aan de hardware-opstelling of aan de PIC zelf.

Open vraag die alleen fysiek te beantwoorden is: in welke carrier zit deze S3 nu. Als het een OTGW32-carrier is in plaats van een Classic, verklaart dat alles wat hier gemeten is, inclusief de werkende OTDirect-frames. Zit hij wel op een Classic-carrier, dan blijft de resetlijn de eerste verdachte: verifieer met een scoop of GPIO12 tijdens commando p daadwerkelijk ~100 ms laag gaat.

Los hiervan gevonden en apart de moeite waard: de webserver start op geen van beide builds. Poort 80 wordt actief geweigerd (dus niet gehangen, en niet de AsyncTCP-wedge uit ADR-139: CONFIG_ASYNC_TCP_STACK_SIZE staat al op 16384), terwijl telnet op 23 gewoon werkt en de firmware verder volledig draait: MQTT, OT-verwerking, BLE-sensoren, heap ~48k. Dat verdient een eigen taak.

CORRECTIE, zelfde avond. De maintainer meldt dat de ESP op dit moment LOS van de carrier zit.

Daarmee vervalt alle PIC-bewijs van vanavond. Geen PIC gedetecteerd, No ETX found after reset, combo die OT-Direct kiest, classic die Degraded wordt en de I2C-fouten op de classic-bus: dat is allemaal het verwachte gedrag van een losgekoppeld bord, niet een defect. De 0x26 watchdog en de PIC zitten op de carrier.

Wat blijft staan is het oorspronkelijke onderwerp van deze taak: de mislukte flashpogingen van 2026-07-06, toen het bord WEL op de carrier zat en PR=A na elke poging een echte pic16f1847 v6.6 liet horen, terwijl de bootloader-entry op FWSTATE_RSET strandde op 0%.

Winst die wel overeind blijft: het telnet-commando p (Manual reset PIC) doorloopt precies dezelfde detectiestap en logt No ETX found after reset. Zodra het bord terug op de carrier zit is dat dus de goedkope reproductie, in plaats van een flashpoging. Verwacht gedrag na terugplaatsen: p vindt de PIC wel. Doet hij dat niet, dan is de resetlijn opnieuw verdachte nummer een.

Met carrier aangesloten en na een herstart: REPRODUCEERT NIET.

  detectPIC( 967): ETX found after reset: Pic detected!

Het telnet-commando p vindt de PIC direct. device/info meldt hardware_type otgw-classic, hardwaremode PIC, otcommandinterface PIC, picdeviceid pic16f1847. De bootloader-entry die in juli op FWSTATE_RSET strandde, faalt vandaag dus niet op deze stap.

De PIC draait op dit moment diagnose-firmware 2.2, niet gateway 6.6 zoals in juli. Iemand heeft hem sindsdien dus wel degelijk succesvol geflasht, wat op zichzelf pleit tegen een structureel defect in het flashpad op dit bord.

Status: het oorspronkelijke symptoom is niet reproduceerbaar en er is geen aanwijzing meer voor een defect. Voorstel is deze taak te sluiten en TASK-972 AC#2 opnieuw te beproeven met een echte flash vanuit de v2-UI, in plaats van hier op een spook te blijven jagen. Wel bewaren: commando p is de goedkope reproductie van de detectiestap, mocht het ooit terugkomen.
<!-- SECTION:NOTES:END -->
