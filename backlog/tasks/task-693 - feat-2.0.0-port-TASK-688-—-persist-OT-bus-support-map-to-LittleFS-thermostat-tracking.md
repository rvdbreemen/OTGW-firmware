---
id: TASK-693
title: >-
  feat-2.0.0: port TASK-688 — persist OT-bus support map to LittleFS +
  thermostat tracking
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 08:19'
updated_date: '2026-05-24 08:35'
labels:
  - port-from-dev
  - diagnostics
  - opentherm
  - persistence
dependencies: []
priority: medium
ordinal: 61000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-688 / commit ee926eaa. Adds bilateral OT-support tracking: four new bitmaps (thermostatSentRead/Write, boilerAckedRead/Write) plus persistence to /ot-thermo.json and /ot-boiler.json. loadOtSupportFiles() reads on boot (Stream::find + parseInt, magic gate on 'v':1); saveOtSupportFilesIfDirty() writes atomically (*.tmp + remove + rename) every 15 minutes via a discrete do15minevent() in loop(). Depends on TASK-692 port.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Six file-scope uint8_t[32] bitmaps in OTGW-Core.ino: thermostatSentRead, thermostatSentWrite, boilerAckedRead, boilerAckedWrite (new) plus boilerUnsupportedRead, boilerUnsupportedWrite (already moved to file scope in TASK-692 port). Two new dirty flags thermostatFileDirty and boilerFileDirty, set on any 0->1 transition.
- [x] #2 Accessors isThermostatMsgIdSentRead/Write(uint8_t id) and isBoilerMsgIdAckedRead/Write(uint8_t id) added in OTGW-Core.ino with declarations in OTGW-firmware.h.
- [x] #3 processOT(): every master Read-Data sets thermostatSentRead bit; every master Write-Data sets thermostatSentWrite bit; every slave Read-Ack sets boilerAckedRead bit; every slave Write-Ack sets boilerAckedWrite bit. Existing Unknown-Data-Id branch also sets boilerFileDirty alongside boilerUnsupportedDirty.
- [x] #4 setup() in OTGW-firmware.ino calls loadOtSupportFiles() once after LittleFS.begin() / readSettings / checklittlefshash. Magic gate: file header must contain '"v":1' or the file is ignored and a DebugTln warning is logged. After load, both dirty flags are cleared.
- [x] #5 A new do15minevent() function in OTGW-firmware.ino calls saveOtSupportFilesIfDirty() (defined in OTGW-Core.ino). The dispatch uses a discrete DECLARE_TIMER_MIN(timer15min, 15, CATCH_UP_MISSED_TICKS) in loop() — NOT piggybacked on doTaskMinuteChanged.
- [x] #6 saveOtSupportFilesIfDirty(): for each file with its dirty flag set, write to <path>.tmp, LittleFS.remove(canonicalPath) (no-op if absent), LittleFS.rename(tmpPath, canonicalPath). Flag cleared on successful rename.
- [x] #7 Stream-based read uses File::find + File::parseInt (no large RAM buffer). Stream-based write uses File::print(int) per id (no large RAM buffer). File format: /ot-thermo.json = {"v":1,"device":"thermostat","sent_read":[...],"sent_write":[...]}; /ot-boiler.json = {"v":1,"device":"boiler","acked_read":[...],"acked_write":[...],"unsupported_read":[...],"unsupported_write":[...]}
- [x] #8 Power-loss recovery: a corrupt .tmp file is ignored at next boot (loadOtSupportFiles only opens the canonical path). The next dirty write overwrites it.
- [x] #9 python build.py --firmware exits 0.
- [x] #10 python evaluate.py --quick shows no new failures vs current 2.0.0 baseline.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-Core.ino: add 4 new file-scope bitmaps (thermostatSentRead/Write, boilerAckedRead/Write), 2 new dirty flags (thermostatFileDirty, boilerFileDirty). 4 new accessors. Update processOT classification block to set the new bitmaps on master Read-Data/Write-Data + slave Read-Ack/Write-Ack; also set boilerFileDirty on Unknown-Data-Id 0->1.
2. Add loadOtSupportFiles() and saveOtSupportFilesIfDirty() functions in OTGW-Core.ino with helper readBitmapArrayFromJson / writeBitmapArrayToJson / writeOtThermoFile / writeOtBoilerFile (atomic write via *.tmp + remove canonical + rename).
3. OTGW-firmware.h: declare new accessors + loadOtSupportFiles + saveOtSupportFilesIfDirty.
4. OTGW-firmware.ino setup(): call loadOtSupportFiles() after checklittlefshash().
5. Add do15minevent() in OTGW-firmware.ino + DECLARE_TIMER_MIN(timer15min, 15, CATCH_UP_MISSED_TICKS) + DUE check in loop().
6. Bump prerelease (alpha.60 -> alpha.61).
7. Build + eval.
8. Commit.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev TASK-688 (commit ee926eaa / PR #640) to 2.0.0: persist the bilateral OT-bus support map to LittleFS as JSON, read on boot, atomic 15-min debounced write. Extended the boiler-only tracking from the prior port to cover the thermostat side too.

## State (OTGW-Core.ino, file scope)
- 4 new bitmaps: thermostatSentRead/Write, boilerAckedRead/Write (32 B each).
- 2 new dirty flags: thermostatFileDirty, boilerFileDirty (0->1 transitions only).
- 4 new accessors: isThermostatMsgIdSentRead/Write, isBoilerMsgIdAckedRead/Write.
- Running RAM: 6 x 32 B bitmaps + 3 B dirty flags + 32 B master-direction scratch = 227 B static.

## processOT integration
- Master Read-Data: set thermostatSentRead bit + clear boilerLastMasterWasWrite bit; if 0->1 set thermostatFileDirty.
- Master Write-Data: set thermostatSentWrite bit + set boilerLastMasterWasWrite bit; if 0->1 set thermostatFileDirty.
- Slave Read-Ack: set boilerAckedRead bit; if 0->1 set boilerFileDirty.
- Slave Write-Ack: set boilerAckedWrite bit; if 0->1 set boilerFileDirty.
- Slave Unknown-Data-Id (existing branch): also sets boilerFileDirty alongside boilerUnsupportedDirty.

## Persistence (LittleFS)
- /ot-thermo.json: {"v":1,"device":"thermostat","sent_read":[...],"sent_write":[...]}
- /ot-boiler.json: {"v":1,"device":"boiler","acked_read":[...],"acked_write":[...],"unsupported_read":[...],"unsupported_write":[...]}
- Read on boot: setup() calls loadOtSupportFiles() right after checklittlefshash().
- Stream-based parser (Stream::find + parseInt, 16-byte header sniff for magic gate on '"v":1'). Missing/corrupt files start fresh with one logged warning. After load, both dirty flags cleared.
- 15-min write via discrete do15minevent() + DECLARE_TIMER_MIN(timer15min, 15, CATCH_UP_MISSED_TICKS) in loop(). NOT piggybacked on doTaskMinuteChanged.
- Atomic write: <path>.tmp + LittleFS.remove(canonical) + LittleFS.rename. Power loss between rename steps leaves a dangling *.tmp; next boot ignores it (only canonical path is loaded); next dirty write replaces.

## Notes vs dev
- ESP32 LittleFS API matches ESP8266 for the calls used (open, find, parseInt, read, peek, available, remove, rename). No #ifdef needed.
- File::find() + File::parseInt() are Arduino Stream methods, available on both platforms.
- 2.0.0's loop() carries timer1s/timer3s/timer500ms/timer60s/timer5min already; timer15min slot inserted before timer5min in DUE order so the file write runs first when both 5-min and 15-min boundaries align.

## Verification
- python build.py --firmware --target esp8266 exits 0 (2.0.0-alpha.61).
- python evaluate.py --quick: 60/1/0 (98.5%, unchanged from baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
