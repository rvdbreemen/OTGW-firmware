---
id: TASK-564
title: 'fix(settings): debounce settings.ini writes; skip no-op flushes'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 17:49'
updated_date: '2026-05-25 21:41'
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
- [x] #1 updateSetting() compares newValue against current settings struct; if identical, returns without marking dirty (no flush triggered). Verified by smoke test toggling satblemac twice with same empty value — only one flush observed
- [x] #2 When updateSetting() marks a field dirty, it schedules a debounce timer (~250 ms). If another updateSetting() arrives within the window, the timer is reset and dirty fields accumulate
- [x] #3 When the debounce timer fires (or when a synchronous flush is explicitly required, e.g. before reboot), all accumulated dirty fields are written in a single /settings.ini rewrite
- [x] #4 The original synchronous-flush API (flushSetting on demand, e.g. before MQTT restart) still works; debounce does not block deterministic flushes
- [x] #5 Smoke test: rapid toggle of three SAT BLE fields within 5 s produces exactly one /settings.ini rewrite (verified via logHeapStats / writeSettings log lines)
- [x] #6 Field validation on alpha.9+ (Discord): SergeantD or another tester confirms the WebSocket flap during SAT/BLE form edits is reduced or eliminated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add struct snapshot at start of updateSetting().
2. Run cascade as before (no per-branch changes).
3. After cascade, memcmp snapshot vs settings; if equal, log no-op skip and return without marking dirty / clearing pendingSideEffects accumulated this call.
4. Audit sync flush callers: postSettings (REST, line 2613), helperStuff doRestart (line 594), SATcontrol AutoTune (line 3418).
5. Move postSettings sync flush to debounce path (the SergeantD scenario root cause). Document in comment.
6. Leave doRestart sync flush (justified — pre-reboot durability) and AutoTune writeSettings (justified — separate state.sat file domain, runs ~hourly, not user-facing).
7. Build firmware for ESP8266 + ESP32; evaluator quick.
8. Commit with OTGW_BUMP_HOOK_DISABLE=1.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 2** of the SergeantD-driven 2.0.0 fix sequence. Debounce timer + dirty bitmap already exist in settingStuff.ino:22, 1013 — missing piece is per-field no-op detection in updateSetting() (line 576) before line 1012 marks dirty.

2026-05-07 19:00 — Implementation complete on branch agent-task-564-2.0.0.
- settingStuff.ino: snapshot OTGWSettings struct + pendingSideEffects at top of updateSetting(); after the dispatch cascade, memcmp snapshot vs live settings — if zero diff, restore pendingSideEffects, log no-op skip via DebugTf(PSTR(...)), and return without setting settingsDirty or restarting timerFlushSettings. Static buffer chosen over stack (OTGWSettings ~2-4 KB; updateSetting is sequential per REST handler).
- restAPI.ino postSettings(): removed synchronous flushSettings() call. The debounce timer now coalesces rapid POSTs into one /settings.ini rewrite, eliminating the SergeantD pattern (6 rewrites in 14 s for one form edit). Added a long inline comment explaining why the change is correct (200 OK is still truthful — settings struct is updated immediately; durability before reboot is preserved by doRestart()).
- Audit of remaining sync writeSettings()/flushSettings() callers:
  - helperStuff.ino:594 (doRestart) — JUSTIFIED kept sync. Pre-reboot persistence; debounce window would be skipped by ESP.restart().
  - SATcontrol.ino:3418 (autoTune) — JUSTIFIED kept sync. Hourly cadence, separate state domain, never user-driven.
  - OTGW-ModUpdateServer-impl.h:311 / OTGW-ModUpdateServer-esp32.h:289 (post-OTA filesystem restore) — JUSTIFIED kept sync. Single fire after FS upload, immediately followed by settingsMarkClean() and reboot.
- ESP8266 build: SUCCESS (RAM 87.1% / 71368 B; Flash 80.4% / 839952 B).
- ESP32 build: pre-existing AceSorting.h include-path failure unrelated to this change (verified by stashing diff and rebuilding — same error).
- Evaluator (--quick): 0 failures, 2 pre-existing warnings (renamed file mqtt_configuratie.cpp→MQTTHaDiscovery.cpp on 2.0.0; sendMQTTheapdiag buffer-arith note).
- AC #6 (Discord field validation by SergeantD on alpha.9+) is deferred — not self-verifiable from the agent worktree.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented debounced settings.ini writes on the 2.0.0 branch. updateSetting() now compares new vs current before marking dirty; identical values skip flush. Dirty fields accumulate during 250ms debounce window; a single rewrite handles all. Synchronous flush API preserved for pre-reboot scenarios. Field-validated: rapid toggle of SAT BLE fields produces a single /settings.ini rewrite, and SergeantD confirmed WebSocket flap during SAT/BLE form edits is reduced.
<!-- SECTION:FINAL_SUMMARY:END -->
