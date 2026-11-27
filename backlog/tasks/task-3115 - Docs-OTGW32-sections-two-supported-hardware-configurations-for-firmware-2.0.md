---
id: TASK-3115
title: 'Docs: OTGW32 sections, two supported hardware configurations for firmware 2.0'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T05:41:00Z'
updated_date: '2026-11-27T06:20:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1170000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-27 (chat, relayed by the coordinator): the sale of the OTGW32 starts; extend the documentation with sections about the OpenTherm Gateway OTGW32; there is no ban any more on naming it; refer to it as one of the 2 supported hardware configurations for firmware 2.0 (OTGW Classic with an ESP32-S3 module, and the OTGW32). Facts only from code, ADRs and bench notes; sales date, price, shop and availability stay out.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with the list of files and sections in the task notes, reported to the coordinator before the large write
- [ ] #2 README, 2.0.0 release notes intro, manuals en and nl, hardware pinout page, flash/install guide and FAQ/troubleshooting have OTGW32 sections; every fact cites code, ADR or bench note in the task notes
- [ ] #3 Leftover 'never name the OTGW32 board in public' rules in process docs and skills reworded; findings in the core block of CLAUDE.md, AGENTS.md and copilot-instructions reported to the coordinator
- [ ] #4 No em dash, plain user text, docs tests green
- [ ] #5 Open questions for Robert listed (sales date, price, shop, availability)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 PLAN (approved by the coordinator, Robert's words: OTGW32 docs, README, 2.0 release notes, disclaimer that the OTGW32 is a new device that still has to prove itself in the field; two chapters; only version 1.0 exists, the first production run; silent launch, nothing on Discord). Files: A) README.md: section Supported hardware for 2.0 with exactly two configurations (OTGW Classic with an ESP32-S3 module and PIC; OpenTherm Gateway OTGW32, OT-Direct, no PIC), disclaimer in that section (not in the headline). B) docs/releases/RELEASE_NOTES_2.0.0-draft.md and the beta notes template: intro and the same disclaimer. C) docs/manuals/en and nl: new chapter OTGW32 hardware configuration and new chapter Differences between an OTGW Classic with PIC and an OTGW32 (comparison table); adjust ch01, ch02, ch03, ch06, ch07, ch10 (nl: h01, h02, h03, h06, h07, h10). D) docs/hardware/PINOUT.md and combo-esp32-s3-pinout.md, docs/guides/FLASH_GUIDE.md and WIFI_FIRMWARE_UPDATE.md (+ reference), docs/api/MQTT.md where hardware matters, SAT docs where hardware matters, wiki drafts. E) Facts only, each cited (file:line or ADR) in these notes from OTGW-field-data/_comms/otgw32_facts_sheet.md; no sales date, price, shop or availability.

2026-11-27 OPEN QUESTIONS for Robert: (1) reading of his sentence: 'Version 1.0 of the OTGW32 is the first production run' (the original 'Een enkele van de OTGW 32, 1.0 is de eerste productierun' is unclear); (2) sales date, price, shop, availability and who sells it stay out; (3) the existing hardware docs call the board an OT-Thing OTGW32 PCB: is the OT-Thing project the origin or maker, and may that be said? Until he answers the new text names only the OpenTherm Gateway OTGW32; (4) official product name spelling: first mention OpenTherm Gateway OTGW32, then OTGW32 (decided by the coordinator, Robert's rule). Decisions of the coordinator 2026-11-27: disclaimer in the hardware section and in the README hardware section, not in the README headline; no order or manual page to link.

2026-11-27 CHUNK 1 written (README Hardware Support with the two configurations and the disclaimer; manuals en and nl chapters 11 and 12; ch01 and ch02 en and nl; RELEASE_NOTES_2.0.0-draft supported hardware; build_manual header; docs/MANUAL.md regenerated). Facts and their sources: OTGW-field-data/_comms/otgw32_facts_sheet.md (288 lines, every fact with file:line or ADR; written by a research agent, not re-verified line by line by me). Citations removed from the user text of chapter 11 (kept here):  (`OTGW-firmware.ino:413-500`, `OTGW-firmware.h:918-922`, ADR-201)  (`graphHistoryStuff.h:43-45`, ADR-245)  (ADR-257)  (`restAPI.ino:44`, ADR-300)  (ADR-224) (`src/libraries/Platform/src/boards.h:304-308`) (`boards.h:100-102`) (`boards.h:103-104`) (`MQTTstuff.ino:2395-2399`) (`OTDirecttypes.h:49-56`) (`OTDirect.ino:107-108`, `OTDirect.ino:4531-4541`) (`OTDirect.ino:1467-1487`) (`docs/guides/WIFI_FIRMWARE_UPDATE.md:5`) (`src/OTGW-firmware/data/v2.js:2171-2178`, `3925-3965`) (`diagRecorderStuff.h:61-62`) (`OTGW-firmware.h:1061-1066`, `sensors_ext.ino:308-309`)

2026-11-27 NOT VERIFIED and therefore worded as not tested in the chapters: real thermostat or real boiler on an OTGW32 (only loopback and master without thermostat on the bench), Ethernet W5500 and OLED on real hardware, OTmonitor and the HA OpenTherm Gateway integration over port 25238 on an OTGW32, USB first install on an OTGW32, bypass relay (code compiled out, so the docs say it is not available), which header pins are free for S0, output and Dallas, flash mode of the board, which release zip an OTGW32 owner should take (docs say esp32-combo; build.py also defines an esp32-otgw32 asset slug, CI builds combo only). Doc conflicts found: ch01 mode names (fixed), bypass listed as a working mode in ch03, ch09, docs/api/README.md and docs/api/MQTT.md (to fix), combo-esp32-s3-pinout.md watchdog note, ADR-066 and ADR-127 statements that the code contradicts, the otgw-otdirect MQTT topic table in ch09 with no publisher found. Remote telemetry, ADR-298 (app slot 2304K), ADR-300 (REST cap 4) and ADR-268 are Accepted but not in the HEAD code, so the chapters do not describe them as present.

2026-11-27 07:5x VERIFICATION of 14 consequential claims, each opened by otgw-docs in the code at origin/dev (not the research agent's text): (1) OT-Direct modes gateway 1, monitor 2, bypass 0, master 3, loopback 4: verified, OTDirecttypes.h:49-56. (2) OTGW32 has no PIC, direct OT, 4 MB flash, no PSRAM (ESP32-S3FN4): verified, boards.h:98-104 and 302-309. (3) Bypass not available: verified, HAS_BYPASS_RELAY is defined nowhere in src, platformio.ini or build.py; OTDirect.ino:107-108 maps mode 0 to gateway; OTDirect.ino:4533-4541 answers NG to GW=P. (4) GW= letters (0 or M monitor, 1 gateway, 2 or S master, P bypass, L loopback): verified, OTDirect.ino:4515-4550. (5) Auto-detect: 5 s window, master for this boot only, not persisted: verified, OTDirect.ino:1465-1490. (6) Boot detection: up to 3 PIC probes, OT-Direct only when no PIC and no PSRAM and no Pro IMU, otherwise degraded with the PIC UART open: verified, OTGW-firmware.ino:460-500 and OTGW-firmware.h:916-922. (7) Graph history 1 h without PSRAM, 7 d ring with PSRAM: verified, graphHistoryStuff.h:41-47. (8) BLE needs PSRAM and REST answers 409: verified, SATble.ino:627-628 and restAPI.ino:4659-4661 (the UI wording of the disabled switch was not opened). (9) REST in-flight cap 2: verified, restAPI.ino:44. (10) Dallas default pin 10 equals the step-up enable pin and is refused as a board pin; S0 default 12 and output default 16 equal the W5500 SCK and RST pins: verified, boards.h:79,90-91, Sensorstypes.h:23, S0types.h:22, Outputstypes.h:22, OTGW-firmware.h:1058-1067, sensors_ext.ino:308-311. (11) Partition table safeboot 960K, app 2240K, LittleFS 768K: verified, partitions_otgw_esp32.csv (0xF0000, 0x230000, 0xC0000). (12) Web UI hides the PIC firmware tab without a PIC: verified, v2.js:2171-2175. CORRECTED: the command prompt character is the single angle quote 'OT ›' and 'PIC ›' (v2.js:3947-3950), not '>'; fixed in ch11 en and nl. (13) otgw-firmware/hardware_type and hardware_mode are published retained: verified, MQTTstuff.ino:2393-2400. (14) Heating interruption of 1 to 2 minutes during a WiFi update on a board without PIC: verified, docs/guides/WIFI_FIRMWARE_UPDATE.md:5; port 25238 default off: verified, MQTTstuff.h:73. NOT re-opened by me (left for the architect's spot check): the ADR-derived statements (ADR-224 return temperature slot, ADR-229, ADR-241, ADR-257, ADR-300), SAT being the only temperature controller on OT-Direct (ch09:739), the claim that nothing writes boardmode 2, and the bench statements (what was and was not tested on the OTGW32 unit).
<!-- SECTION:NOTES:END -->
