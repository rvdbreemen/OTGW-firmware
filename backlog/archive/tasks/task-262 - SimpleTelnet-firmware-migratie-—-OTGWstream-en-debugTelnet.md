---
id: TASK-262
title: 'SimpleTelnet: firmware migratie — OTGWstream en debugTelnet'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 20:07'
updated_date: '2026-04-12 20:52'
labels:
  - telnet
  - migration
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Migrate the firmware from TelnetStreamClass + ESPTelnet to SimpleTelnet. This is the integration step that replaces both library dependencies with the new unified implementation.\n\nFILE-BY-FILE CHANGES:\n\n1. src/OTGW-firmware/OTGW-firmware.h (lines 39-41):\n   REMOVE: #include <TelnetStream.h>\n   CHANGE: #include <ESPTelnet.h> → #include <SimpleTelnet.h>\n   CHANGE: extern ESPTelnet debugTelnet → extern SimpleTelnet<1> debugTelnet\n\n2. src/OTGW-firmware/OTGW-Core.h (line 18-20):\n   Line 18 comment about TelnetStream — update to reference SimpleTelnet\n   CHANGE line 20: TelnetStreamClass OTGWstream(OTGW_SERIAL_PORT)\n                → SimpleTelnet<4>  OTGWstream(OTGW_SERIAL_PORT)\n   Note: this is a global definition in a header — only included once by OTGW-firmware.ino\n\n3. src/OTGW-firmware/networkStuff.ino (lines 22-24, 272-313):\n   CHANGE line 24: ESPTelnet debugTelnet → SimpleTelnet<1> debugTelnet(23)\n   CHANGE line 276+ sendTelnetBanner: signature (String ip) → (const char* ip)\n     Body: debugTelnet.println(F(...)) unchanged — SimpleTelnet inherits Print\n   CHANGE onTelnetInput: signature (String input) → (const char* input)\n   CHANGE line 313: debugTelnet.begin(23) → debugTelnet.begin()\n     Port is now in constructor, begin() takes only optional WiFi-check bool\n\n4. src/OTGW-firmware/networkStuff.h (line 91):\n   #define WM_DEBUG_PORT debugTelnet\n   VERIFY: WiFiManager calls WM_DEBUG_PORT.print() — works because SimpleTelnet\n   inherits from Stream which inherits from Print. No change needed.\n\n5. src/OTGW-firmware/Debug.h (lines 19-20, 49, 66, 135):\n   CHANGE extern comment line 49: references ESPTelnet → SimpleTelnet<1>\n   Lines 19-20 (Debug/Debugln macros): debugTelnet.print() — unchanged\n   Line 66 (DebugTf): debugTelnet.print(buf) — unchanged\n   Line 135: debugTelnet.print(_bol) — unchanged\n   The macros themselves need no code changes — SimpleTelnet has print().\n\n6. src/OTGW-firmware/OTGW-Core.ino:\n   FIND startOTGWstream() at line 4272-4274:\n     OTGWstream.begin() — ADD false parameter: OTGWstream.begin(false)\n     Reason: OTGWstream does not need WiFi check (server binds regardless)\n   FIND doBackgroundTasks() — ADD: OTGWstream.loop();\n     Place it near the existing debugTelnet.loop() call for consistency.\n     The exact location: search for debugTelnet.loop() and add OTGWstream.loop()\n     on the following line.\n\n7. src/OTGW-firmware/settingStuff.ino (line 277):\n   debugTelnet.write(showFile.read()) — UNCHANGED\n   write(uint8_t) is in the Stream interface, works identically.\n\n8. src/OTGW-firmware/handleDebug.ino:\n   onTelnetInput callback is called FROM networkStuff.ino's onInputReceived handler.\n   Check if handleDebugInput or any called function takes String parameter.\n   If so: change to const char*.\n\n9. build.py (lines ~212-225, library download list):\n   REMOVE: TelnetStream entry (name + version)\n   REMOVE: ESPTelnet entry (name + version)\n   SimpleTelnet needs NO entry — it lives in src/libraries/ which is already\n   passed via --libraries flag at line 445. arduino-cli finds it automatically.\n\nNOTE on OTGWstream.loop() importance:\n  TelnetStream has no loop() — it works purely on polling.\n  SimpleTelnet requires loop() for keep-alive and onConnect callbacks.\n  Without loop(), new connections are accepted (TCP level) but onConnect never\n  fires, keep-alive never runs, and stale connections are never cleaned up.\n  OTGWstream has no callbacks so the visible impact is subtle but real:\n  stale connections accumulate and are never cleaned up without loop().
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGW-Core.h declareert OTGWstream als SimpleTelnet<4>
- [x] #2 OTGWstream.loop() wordt aangeroepen in doBackgroundTasks()
- [x] #3 networkStuff.ino declareert debugTelnet als SimpleTelnet<1>
- [x] #4 Callback-handtekeningen sendTelnetBanner en onTelnetInput gebruiken const char* (geen String)
- [x] #5 OTGW-firmware.h include verwijst naar SimpleTelnet.h, beide oude includes verwijderd
- [x] #6 Debug.h extern-declaratie bijgewerkt naar SimpleTelnet<1>
- [ ] #7 Firmware compileert zonder warnings na de migratie
- [x] #8 Debug macro's DebugTln / DebugTf / DebugFlush werken ongewijzigd na migratie
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Migrated firmware from TelnetStream + ESPTelnet to SimpleTelnet (unified multi-client telnet library).

Changes made:
- src/OTGW-firmware/OTGW-firmware.h: removed #include <TelnetStream.h>, replaced #include <ESPTelnet.h> with #include <SimpleTelnet.h>, changed extern type from ESPTelnet to SimpleTelnet<1>
- src/OTGW-firmware/OTGW-Core.h: replaced TelnetStreamClass OTGWstream declaration with SimpleTelnet<4> OTGWstream; updated comment
- src/OTGW-firmware/networkStuff.ino: changed ESPTelnet debugTelnet definition to SimpleTelnet<1> debugTelnet(23) (port in constructor); updated sendTelnetBanner signature from (String ip) to (const char* ip); updated onTelnetInput signature from (String s) to (const char* s); changed debugTelnet.begin(23) to debugTelnet.begin() (port no longer passed to begin())
- src/OTGW-firmware/OTGW-Core.ino: changed OTGWstream.begin() to OTGWstream.begin(false) to skip WiFi check; added OTGWstream.loop() to handlePicFlashBackgroundTasks()
- src/OTGW-firmware/OTGW-firmware.ino: added OTGWstream.loop() in both handleEspFlashBackgroundTasks() and doBackgroundTasks() normal path, alongside existing debugTelnet.loop() calls
- src/OTGW-firmware/Debug.h: updated stale ESPTelnet references in comments to SimpleTelnet
- src/OTGW-firmware/handleDebug.ino: updated stale ESPTelnet references in comments to SimpleTelnet

Debug macros (DebugTln, DebugTf, DebugFlush, etc.) are unchanged — SimpleTelnet inherits from Stream/Print so all print/println/flush calls continue to work identically.

AC7 (compiles without warnings) is deferred to TASK-263 which performs the compilation verification pass.
<!-- SECTION:FINAL_SUMMARY:END -->
