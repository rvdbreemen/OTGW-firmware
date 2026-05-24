---
id: TASK-688
title: >-
  Persist OT-bus support map to LittleFS as JSON (boiler + thermostat),
  read-on-boot, 15-min debounced atomic write
status: To Do
assignee: []
created_date: '2026-05-24 07:16'
labels:
  - diagnostics
  - opentherm
  - persistence
  - observability
  - user-experience
dependencies: []
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
- [ ] #1 Six bitmaps total exist as file-scope statics in OTGW-Core.ino: thermostatSentRead, thermostatSentWrite, boilerAckedRead, boilerAckedWrite (new) plus boilerUnsupportedRead, boilerUnsupportedWrite (existing from TASK-685). Updated inside processOT on the appropriate frame types. Two new dirty flags (thermostatFileDirty, boilerFileDirty) set on any 0->1 transition in their respective halves.
- [ ] #2 On every Read-Data / Write-Data master frame, the matching thermostatSent* bit is set. On every Read-Ack / Write-Ack slave frame, the matching boilerAcked* bit is set. The existing Unknown-Data-Id handling (TASK-685) continues to set boilerUnsupported* and also sets boilerFileDirty.
- [ ] #3 setup() invokes loadOtSupportFiles() once after LittleFS.begin(). For each file: if absent or header magic/version mismatch, leave bitmaps zero and log one DebugTln warning. Otherwise stream-parse the integer arrays into their bitmaps.
- [ ] #4 doTaskMinuteChanged() runs a 15-minute counter (or DECLARE_TIMER_MS at 15*60*1000). When the counter elapses, saveOtSupportFilesIfDirty() is called: for each file whose dirty flag is set, the file is written via the atomic *.tmp/remove/rename dance, then the flag is cleared. Files with no dirty flag are not touched.
- [ ] #5 Reading uses Stream::find + parseInt (no large RAM buffer). Writing uses File.print per integer (no large RAM buffer).
- [ ] #6 On corrupt / partially-written *.tmp leftover from a power loss, the next boot ignores the *.tmp file (only the canonical filename is loaded), saveOtSupportFilesIfDirty() overwrites it with a fresh good copy on the first write cycle.
- [ ] #7 python build.py --firmware exits 0.
- [ ] #8 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->
