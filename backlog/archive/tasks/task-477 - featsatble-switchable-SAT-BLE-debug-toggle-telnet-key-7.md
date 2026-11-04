---
id: TASK-477
title: 'feat(satble): switchable SAT BLE debug toggle (telnet key ''7'')'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-28 18:57'
updated_date: '2026-04-28 21:29'
labels:
  - satble
  - debug
  - esp32
dependencies: []
references:
  - src/OTGW-firmware/SATble.ino
  - src/OTGW-firmware/debugStuff.h
  - src/OTGW-firmware/handleDebug.ino
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a runtime debug toggle for SAT BLE scan/parse/match/update events, mirroring the existing REST/MQTT/SAT/OTDirect debug-toggle pattern. Telnet key '7' toggles `state.debug.bSATBLE` on/off.

## Why

A user-reported BLE sensor (Xiaomi-prefix `A4:C1:38:92:4F:AC`) was not registering. Investigation revealed `SATble.ino` has zero instrumentation on the scan/parse/match/update paths: only `satBLEInit()` logs unconditionally. When no advertisement matches the filter, the entire BLE chain is invisible in telnet log.

This task adds switchable instrumentation so the user can on-demand observe:
- Which advertisements arrive (MAC, RSSI, service-data presence)
- Why they're rejected (unknown format with uuid16, filter mismatch, no free slot)
- Successful parses with full sensor values
- Best-sensor selection in `satBLEUpdateState`
- Scan-cycle starts

## Scope (3 files)

### debugStuff.h
- Add `bool bSATBLE = false;` to `struct DebugSection` (~line 58). Default OFF (production-quiet).

### SATble.ino (ESP32-only file, all changes within existing `#if defined(ESP32)` wrap)
- Add 4 macros after #includes: `SATBLEDebugTln`, `SATBLEDebugTf`, `SATBLEDebugln`, `SATBLEDebugf` (do-while(0) pattern matching OTDDebug)
- Add instrumentation at 7 key paths (see plan file for exact locations and message formats)
- Init meldingen at lines 259/262/264 stay always-on (boot-time visibility unconditional)

### handleDebug.ino
- Menu line for `7) Toggle debuglog - SAT BLE sensor scan: ...` (wrapped in `#if defined(ESP32)`)
- `case '7':` toggle handler (wrapped in `#if defined(ESP32)`)

## Acceptance Criteria
<!-- AC:BEGIN -->
- AC1: ESP32 build green; ESP8266 build also green (struct field declared but unused)
- AC2: Telnet menu shows `7) Toggle debuglog - SAT BLE sensor scan: false` after boot
- AC3: Pressing '7' toggles `state.debug.bSATBLE`, with confirmation message
- AC4: With toggle ON: scan-starts, advertisements (parsed + rejected), MAC mismatches, slot-full, successful sensor updates all visible in log
- AC5: With toggle OFF: zero BLE-related log entries except init meldingen
- AC6: Diagnostic for A4:C1:38:92:4F:AC: which rejection path fires can be identified from the log

## References

- Plan: `C:\Users\rvdbr\.claude\plans\the-design-package-still-elegant-globe.md`
- Pattern source: `OTDirect.ino:28-30` (macros), `handleDebug.ino:115-175` (key dispatch)
- Memory: `feedback_debug_guards_platform.md` (platform isolation requirement)
<!-- SECTION:DESCRIPTION:END -->

- [x] #1 ESP32 build green; ESP8266 build also green (struct field declared but unused on ESP8266)
- [x] #2 Telnet '?' menu shows '7) Toggle debuglog - SAT BLE sensor scan: false' on ESP32 after boot
- [x] #3 Pressing '7' on telnet toggles state.debug.bSATBLE with confirmation 'Debug SAT BLE: ON/OFF'
- [x] #4 With toggle ON: scan-starts, advertisements (parsed + rejected), MAC mismatches, slot-full, successful sensor updates all visible in telnet log
- [x] #5 With toggle OFF: zero BLE-related log entries except always-on init meldingen
- [x] #6 Diagnostic for A4:C1:38:92:4F:AC sensor: which rejection path fires can be identified from log output
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementatie afgerond:

1. debugStuff.h: bSATBLE = false toegevoegd aan struct DebugSection (na bOTDirect)
2. SATble.ino: 4 SATBLEDebug* macros toegevoegd na #includes binnen ESP32 wrap, plus 7 instrumentation spots:
   - onResult begin: 'ad from MAC rssi=N hasServiceData=B'
   - !parsed: 'ad rejected (unknown format)'
   - !bleMatchesConfiguredMAC: 'ad from MAC rejected (filter=...)'
   - slot < 0: 'ad from MAC rejected (no free slot)'
   - na update: 'sensor MAC slot=N temp=Y hum=Z batt=W rssi=V'
   - satBLELoop voor scan->start: 'scan starting (interval=Xs duration=Ys)'
   - satBLEUpdateState: 'best sensor slot=N mac=M temp=Y age=Tms' OR 'no valid sensor (count=N)'
3. handleDebug.ino: menu line '7) Toggle debuglog - SAT BLE sensor scan' en case '7' toggle, beide ESP32-only via #if defined(ESP32)

Diff: 49 insertions / 3 deletions over 3 files. Build draait in achtergrond ID buhiygcla.

Commit d8a73952 geland. Build groen (exit 0) op zowel ESP8266 als ESP32 — AC1 verified. AC2-6 wachten op flash + telnet diagnose-sessie.

2026-04-28: User requested closure; ESP32 telnet SAT BLE debug-toggle behavior accepted as working.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a switchable SAT BLE debug section controlled by telnet key 7. The ESP32 debug menu exposes the toggle, runtime state flips with ON/OFF confirmation, and the SAT BLE scan, parse, rejection, slot, and best-sensor paths are instrumented when enabled while remaining quiet when disabled; user verification accepts the diagnostic flow as working.
<!-- SECTION:FINAL_SUMMARY:END -->
