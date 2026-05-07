---
id: TASK-564
title: 'fix(settings): debounce settings.ini writes; skip no-op flushes'
status: To Do
assignee: []
created_date: '2026-05-07 17:49'
updated_date: '2026-05-07 18:01'
labels:
  - settings
  - littlefs
  - heap
  - 2.0.0
dependencies: []
priority: high
ordinal: 27000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SergeantD's telnet log (alpha.3, 2026-05-07) shows six full /settings.ini rewrites in 14 s for a single SAT/BLE form interaction (13:30:54 to 13:31:08), each 30-80 ms of LittleFS work. Several are no-op writes (satblemac flushed twice with the same empty value; satexternaltemp toggled false→true→false→true with a flush each). The 'deferred' naming in flushSetting() is misleading — flushes are sequential, not coalesced. While each write runs, lwIP cannot service WebSocket / HTTP, contributing directly to the WS code 1006 reconnect loop and indirectly to TASK-529 latency. Fix: collect dirty fields for ~250 ms in a debounce window, write once; treat identical-value writes as no-op (compare new value vs current settings struct before marking dirty). Cross-ref: TASK-529 latency, TASK-563 WS dedup. Same defect family — shared lwIP starvation contributors.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 updateSetting() compares newValue against current settings struct; if identical, returns without marking dirty (no flush triggered). Verified by smoke test toggling satblemac twice with same empty value — only one flush observed
- [ ] #2 When updateSetting() marks a field dirty, it schedules a debounce timer (~250 ms). If another updateSetting() arrives within the window, the timer is reset and dirty fields accumulate
- [ ] #3 When the debounce timer fires (or when a synchronous flush is explicitly required, e.g. before reboot), all accumulated dirty fields are written in a single /settings.ini rewrite
- [ ] #4 The original synchronous-flush API (flushSetting on demand, e.g. before MQTT restart) still works; debounce does not block deterministic flushes
- [ ] #5 Smoke test: rapid toggle of three SAT BLE fields within 5 s produces exactly one /settings.ini rewrite (verified via logHeapStats / writeSettings log lines)
- [ ] #6 Field validation on alpha.9+ (Discord): SergeantD or another tester confirms the WebSocket flap during SAT/BLE form edits is reduced or eliminated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 2** of the SergeantD-driven 2.0.0 fix sequence. Debounce timer + dirty bitmap already exist in settingStuff.ino:22, 1013 — missing piece is per-field no-op detection in updateSetting() (line 576) before line 1012 marks dirty.
<!-- SECTION:NOTES:END -->
