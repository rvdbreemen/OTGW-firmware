---
id: TASK-688
title: >-
  Persist OT-bus support map to LittleFS as JSON (boiler + thermostat),
  read-on-boot, 15-min debounced atomic write
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 07:16'
updated_date: '2026-05-24 07:22'
labels:
  - diagnostics
  - opentherm
  - persistence
  - observability
  - user-experience
dependencies: []
references:
  - TASK-686 (REST/MQTT/WebUI surface that this layer makes durable)
  - 'Conversation: user-requested JSON format'
  - robustness guidance
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Companion to TASK-686. Today the in-RAM bitmaps that track 'what the boiler implements / has answered' reset on every reboot. A field tester who reboots loses the prior knowledge and has to wait for the OT bus to re-walk the same msgIDs before the diagnostic surfaces refill. This task makes the knowledge durable and extends it to cover the thermostat side too, so a single device can report a complete bilateral OT-support snapshot.

Scope:
1. Add thermostat-side msgID tracking (mirror of the existing boiler tracking).
   - thermostatSentRead[32]:  bit set = master has sent Read-Data for this id at least once this session.
   - thermostatSentWrite[32]: bit set = master has sent Write-Data for this id at least once this session.
2. Add boiler-positive tracking (so the snapshot reports both 'works' and 'no').
   - boilerAckedRead[32]:  bit set = slave has answered Read-Ack at least once.
   - boilerAckedWrite[32]: bit set = slave has answered Write-Ack at least once.
3. Persist both halves to two JSON files on LittleFS:
   - /ot-thermo.json  ~ <120 bytes typical, <2 KB worst case.
   - /ot-boiler.json  ~ <240 bytes typical, <4 KB worst case.
4. Read the files at setup() so a reboot inherits prior knowledge. Missing / corrupt files start fresh with a logged warning (do not crash).
5. Debounced atomic write from doTaskMinuteChanged() every 15 minutes when the corresponding dirty flag is set. Atomic = write to *.tmp, remove old, rename. Two flags (thermostatFileDirty, boilerFileDirty) so a file is only rewritten when its own content changed.

File format (JSON, line-broken for readability):
/ot-thermo.json:
  {"v":1,"device":"thermostat","sent_read":[0,1,3,5,...],"sent_write":[1,4,7,8,16,...]}
/ot-boiler.json:
  {"v":1,"device":"boiler","acked_read":[0,3,5,17,...],"acked_write":[1,4,8,16,...],"unsupported_read":[27,33,36,...],"unsupported_write":[14,16,...]}

Why JSON (overriding the binary suggestion): user-requested; a tester / maintainer can open the file in any editor and immediately understand it without a decoder script. The streaming write + Stream::find/parseInt read pattern keeps RAM usage flat regardless of the file's growth.

Robustness:
- Magic gate on read: 'v' field present and equal to the supported version; else discard and start fresh.
- Atomic write: *.tmp -> remove -> rename. A power loss mid-write leaves the *.tmp dangling; next boot ignores it (only the canonical filename is loaded) and a fresh write replaces it.
- Stream-based read (Stream::find('key:[') then parseInt loop until ']'): no big RAM buffer.
- Stream-based write (file.print(int) per id, no big RAM buffer).
- Flash wear ceiling: at most 2 writes per 15 min = 192 writes per day; LittleFS pages rated for 100k+ erase cycles.
- LittleFS must be mounted before the load call; ensured by placing the load in setup() after the existing LittleFS.begin().

Out of scope:
- Surfacing the new fields (thermostat-sent / boiler-acked) via REST/MQTT/WebUI. Internal-use files only per user direction. Maintainers fetch the files via FSexplorer when triaging.
- HA discovery suppression for unsupported msgIDs (still TASK-687, milestone 2.1.0).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Six bitmaps total exist as file-scope statics in OTGW-Core.ino: thermostatSentRead, thermostatSentWrite, boilerAckedRead, boilerAckedWrite (new) plus boilerUnsupportedRead, boilerUnsupportedWrite (existing from TASK-685). Updated inside processOT on the appropriate frame types. Two new dirty flags (thermostatFileDirty, boilerFileDirty) set on any 0->1 transition in their respective halves.
- [x] #2 On every Read-Data / Write-Data master frame, the matching thermostatSent* bit is set. On every Read-Ack / Write-Ack slave frame, the matching boilerAcked* bit is set. The existing Unknown-Data-Id handling (TASK-685) continues to set boilerUnsupported* and also sets boilerFileDirty.
- [x] #3 setup() invokes loadOtSupportFiles() once after LittleFS.begin(). For each file: if absent or header magic/version mismatch, leave bitmaps zero and log one DebugTln warning. Otherwise stream-parse the integer arrays into their bitmaps.
- [x] #4 doTaskMinuteChanged() runs a 15-minute counter (or DECLARE_TIMER_MS at 15*60*1000). When the counter elapses, saveOtSupportFilesIfDirty() is called: for each file whose dirty flag is set, the file is written via the atomic *.tmp/remove/rename dance, then the flag is cleared. Files with no dirty flag are not touched.
- [x] #5 Reading uses Stream::find + parseInt (no large RAM buffer). Writing uses File.print per integer (no large RAM buffer).
- [x] #6 On corrupt / partially-written *.tmp leftover from a power loss, the next boot ignores the *.tmp file (only the canonical filename is loaded), saveOtSupportFilesIfDirty() overwrites it with a fresh good copy on the first write cycle.
- [x] #7 python build.py --firmware exits 0.
- [x] #8 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. State layout (file scope in OTGW-Core.ino):
   - Existing (already at file scope from TASK-686 step 1): boilerLastMasterWasWrite[32], boilerUnsupportedRead[32], boilerUnsupportedWrite[32], boilerUnsupportedDirty.
   - New: thermostatSentRead[32], thermostatSentWrite[32], boilerAckedRead[32], boilerAckedWrite[32], thermostatFileDirty, boilerFileDirty.
   - Net new RAM: 4 x 32 + 2 = 130 bytes. Running total: ~227 B static.
   - Accessors in OTGW-Core.ino + declarations in OTGW-firmware.h: isThermostatMsgIdSentRead/Write, isBoilerMsgIdAckedRead/Write.

2. processOT updates (no behaviour change to existing branches):
   - Master Read-Data:  set thermostatSentRead bit; if was 0 set thermostatFileDirty.
   - Master Write-Data: set thermostatSentWrite bit; if was 0 set thermostatFileDirty.
   - Slave Read-Ack:    set boilerAckedRead bit; if was 0 set boilerFileDirty.
   - Slave Write-Ack:   set boilerAckedWrite bit; if was 0 set boilerFileDirty.
   - Slave Unknown-Data-Id (existing block): also set boilerFileDirty.

3. File I/O (new functions in OTGW-Core.ino):
   - void loadOtSupportFile(const char* path, /* targets */) -- stream-parse JSON via Stream::find + parseInt. Returns true on header magic match; false on missing/corrupt (caller logs once and continues with zero state).
   - void writeOtSupportFile(const char* path, ...) -- stream-write JSON to <path>.tmp, LittleFS.remove(path), LittleFS.rename(tmp -> path). One File handle, no large buffer.
   - void loadOtSupportFiles() -- called from setup() after LittleFS.begin(). Loads both files.
   - void saveOtSupportFilesIfDirty() -- per dirty flag, writes the corresponding file; clears the flag.

4. Cron tick: doTaskMinuteChanged maintains a uint8_t minutesSinceLastOtSupportFlush. Increments by 1 each call; when >=15, invokes saveOtSupportFilesIfDirty() and resets counter to 0. Boundary: a write only happens when dirty AND counter elapsed -- bounds writes to <= 2 per 15-min window.

5. setup() integration: after LittleFS.begin() and before doBackgroundTasks() starts, call loadOtSupportFiles(). Order check: ensure the OT-bus reader does not run before LittleFS is mounted.

6. Build + evaluate + commit + push.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in commit ee926eaa:
- 6 file-scope bitmaps in OTGW-Core.ino: thermostatSentRead/Write, boilerAckedRead/Write (new) + boilerUnsupportedRead/Write (existing); scratch boilerLastMasterWasWrite preserved.
- 3 dirty flags: thermostatFileDirty, boilerFileDirty (new) + boilerUnsupportedDirty (existing). All set on 0->1 only.
- processOT branches for OT_READ_DATA, OT_WRITE_DATA, OT_READ_ACK, OT_WRITE_ACK, OT_UNKNOWN_DATA_ID.
- loadOtSupportFiles called from setup() right after LittleFS.begin() / readSettings / checklittlefshash.
- saveOtSupportFilesIfDirty called from new do15minevent() driven by DECLARE_TIMER_MIN(timer15min, 15, CATCH_UP_MISSED_TICKS) -- discrete timer following the do5minevent pattern (NOT piggybacked on doTaskMinuteChanged, per user direction).
- Stream-based read (Stream::find + parseInt) and write (file.print per id). No large RAM buffer.
- Atomic write via *.tmp -> remove canonical -> rename. Magic gate "v":1 on read; missing/corrupt logs one warning and starts fresh.

Build: python build.py --firmware exit 0.
Evaluator: python evaluate.py --quick 34/0/0 (100%).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Persisted the bilateral OT-bus support map (thermostat-sent + boiler-acked + boiler-unsupported) to LittleFS as two JSON files, read on boot and atomically rewritten every 15 minutes when the corresponding dirty flag is set.

## Architecture
- **In-RAM model**: six 32-byte bitmaps + three dirty flags + 32 B scratch = 227 B static RAM (was 97 B in TASK-686 baseline). Each (msgID, direction) pair is one bit.
- **Persistence layer**: stream-based JSON I/O (Stream::find for keys, parseInt for values on read; file.print per integer on write). No large buffer in either direction.
- **Atomicity**: write to <path>.tmp, remove canonical, rename. A power loss between rename steps leaves a *.tmp dangling; the next boot ignores it (only canonical names are loaded) and the next dirty write replaces it.
- **Magic gate**: each file has a "v":1 field; missing/corrupt/wrong-version files log one warning and start with a fresh empty state.
- **Cron**: discrete `do15minevent()` driven by `DECLARE_TIMER_MIN(timer15min, 15, CATCH_UP_MISSED_TICKS)` declared in loop(), matching the existing `do5minevent` pattern (NOT piggybacked on `doTaskMinuteChanged` per user direction).
- **Wear ceiling**: at most 2 writes per 15-min window per file. ~192 writes/day under continuous discovery on LittleFS sectors rated 100k+ erase cycles.

## Files
- `/ot-thermo.json` — `{"v":1,"device":"thermostat","sent_read":[...],"sent_write":[...]}`
- `/ot-boiler.json` — `{"v":1,"device":"boiler","acked_read":[...],"acked_write":[...],"unsupported_read":[...],"unsupported_write":[...]}`

Maintainers fetch the files via FSexplorer when triaging — internal-use only; no REST/MQTT surfacing of the new fields per user direction.

## processOT changes
Four new idempotent set-bit branches added to the existing classification block:
- Master Read-Data  -> thermostatSentRead
- Master Write-Data -> thermostatSentWrite
- Slave Read-Ack    -> boilerAckedRead
- Slave Write-Ack   -> boilerAckedWrite
Dirty flag fires only on 0->1 transitions so the file writer does work just once per newly-discovered (id, direction).

## Verification
- python build.py --firmware exits 0.
- python evaluate.py --quick: 34 passed / 0 warnings / 0 failures (100% health).

## Risks / follow-ups
- Boiler swap mid-life: bitmaps accumulate forever; a replaced boiler shows the union with the previous one. Acceptable for internal-use. Reset would mean adding a "delete files" REST endpoint; out of scope.
- Thermostat-sent + boiler-acked are not surfaced via REST/MQTT/WebUI (internal-use only per user direction). If needed later, a /api/v2/otgw/ot-support endpoint mirroring the existing /api/v2/otgw/boiler-support is a small addition.
<!-- SECTION:FINAL_SUMMARY:END -->
