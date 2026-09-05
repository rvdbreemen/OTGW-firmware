---
id: TASK-1129
title: >-
  PIC bootloader entry fails on the ESP32-S3 classic bench board: reset
  asserted, handshake never answered
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-05 15:32'
updated_date: '2026-09-05 15:36'
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
<!-- SECTION:NOTES:END -->
