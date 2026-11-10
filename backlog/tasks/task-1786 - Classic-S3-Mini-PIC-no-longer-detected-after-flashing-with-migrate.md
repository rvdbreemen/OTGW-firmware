---
id: TASK-1786
title: 'Classic + S3 Mini: PIC no longer detected after flashing with --migrate'
status: To Do
assignee: []
created_date: '2026-11-10T18:52:00Z'
updated_date: '2026-11-10T22:18:00Z'
labels:
  - needs-maintainer-ok
  - bug
  - support
milestone: 2.0.0
dependencies: []
ordinal: 881000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report from thaghostnl, #nederlandse-ondersteuning 2026-11-10 19:44:03 UTC, msg 1569784722903662795 (https://discord.com/channels/812969634638725140/815561033036333076/1569784722903662795): "Zie ik iets over het hoofd? Op de nodo-shop Opentherm Wi-Fi heb ik nu de LOLIN S3 mini en na het flashen met de migrate optie, wordt de PIC niet meer gedetecteerd." (Nodo Shop OpenTherm Wi-Fi gateway, now with a LOLIN S3 Mini; after flashing with the migrate option the PIC is no longer detected.) No capture yet; asked for one including a reboot, plus the firmware version (bot reply msg 1569786232030888029). Related: ADR-201 (combo board detection persists only a positive PIC verdict and re-probes every boot) and TASK-1774 (rollback / settings reset after the filesystem OTA); in the capture also check whether the settings were still there after the migrate flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture from the reporter shows the boot and the PIC detection (or the reporter is asked again)
- [ ] #2 Cause found from the capture and a fix or answer agreed with Robert
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for capture.

Time correction (2026-11-10): the report time above is local CET. From the message snowflake the correct time is 2026-11-10 18:44:03 UTC (msg 1569784722903662795).

Captures received 2026-11-10 18:10 UTC (msgs 1569791410595565642, 1569791462365724854), analysed in the comms session (local copy RvdB/capture-thaghost-1110). 1.7.7 (Wemos, IP .42, host otgw): PIC 'gateway v6.8 id pic16f1847', MQTT connected to 192.168.10.2, uptime 15 min. 2.0.0-beta.170+d260e69 (S3, MAC AC:27:6E:CB:E5:90, IP .66, host OTGW2): /api/v2/device/info board 'Nodoshop OTGW Combo (ESP32-S3)', hardwaremode Degraded, otcommandinterface None, otdirectavailable false; telnet banner PIC 'no pic found', OTbus offline; 'PIC is unknown, probe pic using PR=A' at 18:59:05 UTC with no PIC answer in the 11 s left; 0 OT frames. Board Mode 0 (auto). reboot_log: 18:54:05 power on, 18:58:03 software reset 'Reboot after upgrade'; the capture started 18:58:35, 32 s after that reboot, so the boot-time detection lines are not in it. Settings on 2.0.0 look like defaults (MQTT disabled, broker homeassistant.local), unlike 1.7.7 (broker 192.168.10.2): settings were not carried over (relates to TASK-1774?), not verified how he flashed. Next: a capture that is running BEFORE a restart (restart from the web UI during the capture), plus confirm it is the same gateway board and how the S3 was seated.

2026-11-10 030509, code analysis (offline; capture read from capture-thaghost-1110, unpacked in my scratchpad only; 97's first pass not repeated):
1. Which path ran (fact from device/info + code): chip_model_est ESP32-S3FH4R2, psram_found 1 (2 MB), boardmode 0 (auto). OTGW-firmware.ino:420-484: auto probes detectPIC() up to 3x; no PIC and isOtgw32HardwarePossible() false (PSRAM = LOLIN module in a Classic socket, TASK-1730) -> UART closed, OT-Direct NOT started, eMode stays HW_MODE_DEGRADED, iBoardMode stays 0 (ADR-201 items 1 and 4: only a positive verdict is persisted). That matches hardwaremode Degraded, otdirectavailable false, boardmode 0 exactly: the firmware did what ADR-201/TASK-1730 prescribe for a Classic whose PIC does not answer. Not a detection-logic fault by this reading; the PIC is silent on the wire.
2. Pins and baud, 2.0 vs 1.7.7 (code): 2.0 combo Classic map boards.h:162-166: PIC RX GPIO44 (RX hole), TX GPIO43 (TX hole), reset GPIO12 (D5 hole), OTGWSerial.begin(9600, SERIAL_8N1, ...) (OTGW-firmware.ino:420). 1.7.x (wt-otgw-1.x.x OTGW-firmware.h:55/62): OTGWSerial on ESP8266 UART0 default pins (GPIO3 RX / GPIO1 TX = the same RX/TX holes) at 9600, reset D5 (GPIO14). Same holes, same baud, same reset hole. So a correctly seated S3 Mini on the same board talks to the PIC over the same wires as the Wemos. Not checked here: the LOLIN S3 Mini pinout itself (boards.h comment table only; no datasheet opened).
3. Settings at defaults (fact): hostname OTGW2 is the 2.0 default (OTGW-firmware.h:132); MAC AC276ECBE590 / host OTGW2 / .66 differ from the 1.7.7 unit (otgw, .42): this is a different ESP module. flash_otgw.bat --migrate restores settings.ini only from --host (migrate_backup/migrate_restore); a fresh S3 module or --no-backup boots with defaults. So the defaults point to a module swap, not to TASK-1774. Exact flash command still unknown.
4. Why no reply at all (opinion, ranked; not provable without the boot): (a) the S3 Mini is not on the same board as the working Wemos, or it is mis-seated: the S3 Mini has two extra inner pin rows, one row off means no RX/TX; (b) a board fault, as crashevans reported for his Classic+S3 (TASK-1730); (c) a firmware fault on UART0 or GPIO12 at boot: no evidence, and the same code detects the PIC on the bench Classic units. 
5. Next steps (for 97's request to him): download /bootdetect.log via FSexplorer (written every boot, OTGW-firmware.ino:466; holds the [combo] detect line with eMode/picEnabled/boardMode/RST/RX/TX of each boot, including the first one after flashing); a capture running across a web UI restart; a photo of the S3 Mini in the socket; confirm it is the same board where the Wemos with 1.7.7 sees PIC 6.8; the exact flash command.
2026-11-10 (comms) second 2.0 capture (20:58 local, zip 20261110-205823-upload-2.0.0-beta-pic-niet-gedetecteerd.zip in ../capture-thaghost-1110b, Discord msg 1569805728946851881): spans a web-UI reboot ('Reboot after upgrade' 20:58:16 local) but telnet reconnected 11 s later (gap 20:58:27-20:58:38), so the boot detection window is again missing. Board Mode is now 1 (pic) (first capture 19:58: 0 auto); forced PIC mode still logs 'PIC is unknown, probe pic using PR=A' with no reply, otframes 0 frames, hardware.mode Degraded. Photo: PCB silkscreen 'Opentherm Gateway V2.12 (c) 2024 nodo-shop.nl' (user wrote V2.13), PIC16F1847 seated, 24V/5V/3V3 LEDs lit, purple LOLIN S3 Mini in the Wemos socket; pin seating not judgeable from a top view. Robert 2026-11-10: the board is the same as other Nodo boards and should work. Asked thaghostnl (msg 1569838519357022209) for /bootdetect.log and side photos of both header rows. No cause claimed.
<!-- SECTION:NOTES:END -->
