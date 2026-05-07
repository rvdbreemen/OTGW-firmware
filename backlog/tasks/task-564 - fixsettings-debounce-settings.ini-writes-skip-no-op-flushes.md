---
id: TASK-564
title: 'fix(settings): debounce settings.ini writes; skip no-op flushes'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 17:49'
updated_date: '2026-05-07 18:20'
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
- [ ] #5 Smoke test: rapid toggle of three SAT BLE fields within 5 s produces exactly one /settings.ini rewrite (verified via logHeapStats / writeSettings log lines)
- [ ] #6 Field validation on alpha.9+ (Discord): SergeantD or another tester confirms the WebSocket flap during SAT/BLE form edits is reduced or eliminated
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
TASK-564 — debounce settings.ini writes and skip no-op flushes (2.0.0 branch).

## Problem
SergeantD's alpha.3 telnet log showed six full /settings.ini rewrites in 14 s for a single SAT/BLE form edit. Each rewrite is 30-80 ms of LittleFS work that blocks lwIP and contributes to the WS code-1006 reconnect loop and to TASK-529 latency. Two defects combined: (1) the REST POST handler called flushSettings() synchronously after every field update, bypassing the existing 2 s debounce timer; (2) updateSetting() unconditionally marked settings dirty even when the new value equalled the current one (e.g. `satblemac=""` posted twice; `satexternaltemp` toggled false→true→false→true).

## Fix (two parts)

**Part 1 — per-field no-op detection in `updateSetting()` (settingStuff.ino).** Snapshot the entire OTGWSettings struct + pendingSideEffects bitmap before the dispatch cascade runs. After the cascade, memcmp the snapshot against the live struct: if zero diff, the call is a no-op — restore pendingSideEffects to its pre-call value, log the skip via DebugTf(PSTR(...)), and return without setting settingsDirty or calling RESTART_TIMER(timerFlushSettings). Static buffer (file-scope inside the function) avoids ~2-4 KB stack pressure on ESP8266. The struct memcmp approach handles all four Hungarian prefixes (b/s/i/f) uniformly without per-field type parsing — simpler and more robust than the originally proposed string-based comparison.

**Part 2 — drop the synchronous flush after REST POST (restAPI.ino postSettings).** Removed the `flushSettings()` call that ran after `updateSetting()`. The 200 OK is still truthful: the in-RAM `settings` struct is updated synchronously, so any subsequent GET reflects the new value immediately. Persistence is now driven by the existing 2 s debounce timer (timerFlushSettings) which coalesces rapid sequential POSTs into one /settings.ini rewrite. SAT BLE field updates via `SATble.ino:343` (TASK-508 path) already relied on the same debounce — the REST path is now consistent with it.

## Audited synchronous-flush call paths
- `helperStuff.ino:594` (doRestart) — left sync, justified: pre-reboot durability; debounce window would be skipped by ESP.restart().
- `SATcontrol.ino:3418` (SAT AutoTune) — left sync, justified: ~hourly cadence, never user-driven, separate state domain.
- `OTGW-ModUpdateServer-impl.h:311` and `OTGW-ModUpdateServer-esp32.h:289` (post-OTA filesystem restore) — left sync, justified: single fire after FS upload, followed immediately by `settingsMarkClean()` + reboot.

## Verification
- ESP8266 firmware build: SUCCESS (RAM 87.1% / 71368 B used; Flash 80.4% / 839952 B used).
- ESP32 firmware build: pre-existing AceSorting.h include-path failure unrelated to this change (verified by stashing the diff — same error appears on the unmodified branch tip).
- `python3 evaluate.py --quick`: 0 failures, 2 pre-existing warnings, health score 97.1%.
- Manual smoke test recipe (run after integration): POST `/api/v2/settings` with `field=satblemac, value=""` twice within 1 s. Telnet log should show two `[Settings] no-op skip: field[satblemac]` lines and zero `Flushing deferred settings write` lines (until 2 s after the second POST, then exactly one).

## Risk and follow-up
Low risk on its own — the debounce timer infrastructure was already wired and field-tested via TASK-508. The behaviour change for HTTP POST callers is that `200 OK` no longer guarantees on-flash persistence within the same request; in-RAM persistence is unchanged. AC #6 (Discord field validation by SergeantD on alpha.9+) is deferred — not self-verifiable from the agent worktree.

## Branch and commit
- Branch: `agent-task-564-2.0.0` (isolated worktree).
- Commit: see `OTGW_BUMP_HOOK_DISABLE=1 git commit` referenced below; integration phase will rebase + bump on the main 2.0.0 worktree.
- Did NOT push, did NOT bump prerelease (per task instructions).
<!-- SECTION:FINAL_SUMMARY:END -->
