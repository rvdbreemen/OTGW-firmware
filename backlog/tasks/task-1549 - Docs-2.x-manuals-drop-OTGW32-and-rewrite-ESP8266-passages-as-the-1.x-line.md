---
id: TASK-1549
title: 'Docs: 2.x manuals drop OTGW32 and rewrite ESP8266 passages as the 1.x line'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T17:57:00Z'
updated_date: '2026-11-01T19:04:00Z'
labels:
  - docs
dependencies: []
ordinal: 703000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01 (AskUserQuestion after TASK-1548): one cleanup task. docs/manuals EN+NL on dev name OTGW32 1-22x per chapter and ESP8266/Wemos 1-39x (counts from scripts/docs-scope-2x.sh). OTGW32 is an unannounced prototype: remove it from the public manuals, describe OT-Direct neutrally. ESP8266 appears only as 'the 1.x line'. Run as /update-docs-2x --scope full before beta.2.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/docs-scope-2x.sh reports OTGW32=0 for every EN chapter and the NL chapters grep clean
- [ ] #2 ESP8266/Wemos mentions only in 1.x-line or going-back context, each remaining one listed in the notes
- [ ] #3 EN and NL chapters stay in sync; no em dashes
- [ ] #4 Done before the v2.0.0-beta.2 tag
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ch01/h01 done: OTGW32 EN 7->0, NL 6->0; ESP8266 EN 13->5, NL 17->6 (all 1.x context or 'Wemos D1 mini socket'). NL flash_esp.py lines (158,178,112) replaced by flash_otgw.bat/.sh. Renamed headings: 'Both Platforms (ESP8266 and ESP32)' -> 'Standard Features', 'ESP32 / OTGW32 Only' -> 'Hardware-Dependent Features', NL 'Platformvergelijking: ESP8266 vs ESP32' -> 'Platformvergelijking'; no inbound links. NL RAM row dropped (no source for S3 figure); NL bug-report field 'platformtype' -> 'firmwarelijn (1.x of 2.x)'. docs/MANUAL.md in scope too (combined manual, hand-maintained, diverged from chapters: targeted edits only). scripts/docs-scope-2x.sh findings now cover NL + MANUAL.md with the widened pattern.

ch02/h02 done: OTGW32 EN 13->0, NL 16->0; ESP8266 EN 27->5, NL 27->6 (socket wording or 1.x context). Supported-hardware, pin-map and flash sections rewritten for Classic + S3 Mini/Mini Pro and flash_otgw.bat/.sh; W5500 section cut to one neutral sentence; BLE/OLED labels corrected; Dallas default doc'd as GPIO 10 from code (Sensorstypes.h:23) - wiring on S3 not verified. Fixed by lead: ch01/h01 'one-click OTA from GitHub' removed (no GitHub API call in src), broken ../docs/BREAKING_CHANGES.md link -> ../../BREAKING_CHANGES.md in ch02/h02. Out-of-scope findings: flash_otgw.sh/.bat banners and --help print 'Nodoshop OTGW32' (user-visible); docs/MANUAL.md line 13 names OTGW32 (handled in the MANUAL.md pass).

ch03/h03 done: OTGW32 EN 10->0, NL 10->0 (incl. 'OTTHING-platform'); ESP8266 4->0 both. OT-Direct labels neutral; Ethernet indicator 'only with Ethernet hardware'; one-click GitHub update sections removed (2.x Update page takes the zip, updateServerHtml.h:88-107); EN heap throttle list replaced by ADR-167 critical-only gate (helperStuff.ino:894,995); NL overview file list corrected to v2.html/v2.js/v2-bundle.css. Findings: firmware UI string v2.html:830 names OTGW32 and has an em dash (needs a firmware task + bump); classic-UI sections in ch03 still outdated (index.js ETag flow); OT-Direct mode table ch03 vs NL ch01 mode list disagree, not checked against code.

ch04/h04 done: OTGW32 1->0, ESP8266 1->1 (discovery paragraph now '1.x firmware on the ESP8266'); resetgateway no-op worded as OT-Direct mode (OTGW-Core.ino:1015); removed '(classic NodoShop OTGW, ESP32 OT-Shield, etc.)' from Device Manufacturer paragraph (judgement call). ch05/h05 done: OTGW32 EN 2->0 NL 3->0, ESP8266 2->0, em dashes 5->0 each; no SAT control statements changed; cycle tail window '180 s' (SATcycles.ino:103); PSRAM example now the LOLIN S3 Mini/Mini Pro (boards.h:305-307); 'Thermo-Nova' dropped from the BLE board list (no such board in boards.h).

ch06/h06 done: OTGW32 2->0, ESP8266 EN 10->0 NL 12->0. Ethernet 6.2.1-6.2.5 cut to one sentence (boards.h:172, restAPI.ino:4106-4116); ESP8266 LLMNR, SDK time bug, lwIP/Core 3.1.2 sections removed; 6.1.2 DHCP re-announce claim corrected for 2.x (networkStuff.ino:258-262, 388-404; TASK-432). Numbering gap 6.6->6.8 left on purpose: four cross-refs cite 6.10.

ch07/h07 done: OTGW32 7->0 both; ESP8266 EN 4->1, NL 7->1 (7.8 CONT stack, 1.x context). 7.12 'ESP32-Specific Troubleshooting (OTGW32)' became 'BLE Temperature Sensors' (OTDirect 18V and W5500 subsections removed); NL OTA-recovery and factory-flash steps now use safeboot + flash_otgw.bat/.sh (--migrate --host). Lead fixed EN 'section 7.9' -> '7.10' for the Telnet console. Not checked in code: ADR-047 two-layer recovery claim (7.10a), safeboot 'redo the update' wording (from ADR-191).

ch08/h08 done: OTGW32 13->0 both; ESP8266 EN 39->3, NL 37->3 (socket wording, 1.x line, C4 L2 1.x PIC link). Build text now build.bat/build.sh (venv bootstrap, machine lock build.py:3015, 4 jobs build.py:2475), esp32-combo only; ESP8266 PlatformIO notes, Arduino IDE and W5500 runtime detection sections removed; ADR-063/072 rows dropped. Facts corrected: safeboot partition layout (partitions_otgw_esp32.csv:20-23), 'no RTOS' -> FreeRTOS tasks (ADR-123/130), '#ifdef ESP8266/ESP32' rule -> platform shims/HAS_* (evaluate.py check_esp_abstraction_boundary), MDNS_NEEDS_UPDATE 0, PIC UART 9600 baud. Stale, out of scope (finding): classic-UI asset table and 'Adding a setting' step 5, heap gate levels vs ADR-167, PubSubClient/sync-httpServer examples, ADR-004 listed as current, flash_esp.py internals subsection, platformio.ini:148 OTGW32 comment.

ch09/h09 done: OTGW32 EN 10->0 NL 9->0, ESP8266 5->0, EN em dashes 4->0. OT-Direct routes/fields worded neutrally; device/info example board = 'Nodoshop OTGW Combo (ESP32-S3)' (boards.h:252) plus hardware_type 'otgw-classic' (restAPI.ino:3207, OTGW-firmware.h:660); MQTT row otgw-firmware/board (not published) -> hardware_type. Lead removed the OT-Direct command-topic block {TopTopic}/set/{UniqueId}/otgw32/room_temp|room_setpoint (MQTTstuff.ino:915, rejected unless OT-Direct): works only without a PIC and the path carries the board name. Findings for Robert: firmware contracts still carry the name (otgw32/ command topics, published otgw32/ch_suspended + flame_*, 503 text 'OTGW32 functions disabled' restAPI.ino:516, hardware_type 'otgw32' in OT-Direct mode, v2.html:830 UI string); stale ch09 content (otgw-otdirect/* status table vs MQTTstuff.ino:1994-2000, picavailable removed TASK-754, hardwaremode emits 'PIC'/'OT-Direct', cpufreq 80).
<!-- SECTION:NOTES:END -->
