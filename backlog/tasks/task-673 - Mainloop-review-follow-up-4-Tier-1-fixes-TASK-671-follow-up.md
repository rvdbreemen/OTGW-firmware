---
id: TASK-673
title: 'Mainloop review follow-up: 4 Tier-1 fixes (TASK-671 follow-up)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 05:28'
updated_date: '2026-05-23 05:42'
labels:
  - responsiveness
  - refactor
  - mainloop
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tier-1 fixes uit tweede mainloop-review:
(1) pendingUpgradePath String->char[]
(2) emergencyHeapRecovery() echte recovery (zie ADR)
(3) BGTRACE volledig verwijderen
(4) loop() indent

Master plan: /root/.claude/plans/hoe-zou-je-tier-cryptic-pascal.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 pendingUpgradePath is static char[80]; OTGW-Core.ino bevat geen String voor deze variabele meer
- [x] #2 emergencyHeapRecovery() implementeert ADR-acties (WS-disconnect, OTGWstream stop+start, clearMQTTConfigPending); telnet log toont before/after heap
- [x] #3 Nieuwe ADR is geaccepteerd en gerefereerd vanuit helperStuff.ino
- [x] #4 BGTRACE volledig verwijderd: geen BGTASKS_TRACE define, geen BGTRACE macro, geen _bgPrev variabelen, geen BGTRACE(...) callsites; TASK-397 op Done
- [x] #5 loop() body uniform geindenteerd 4 spaces; geen gedragswijziging
- [x] #6 python build.py --firmware exit 0
- [x] #7 python evaluate.py --quick geen nieuwe failures (1 String-warning minder)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Tier-1 follow-up from the second mainloop review (TASK-671). Four targeted fixes in one commit:

Changes
- pendingUpgradePath: String -> static char[80] (ADR-004 hot-path compliance). HTTP handler String locals untouched (YAGNI: not on hot path).
- emergencyHeapRecovery(): implements the three actions from ADR-079 (Accepted 2026-05-23) — drop WS clients (via doWebSocketDisconnectAll wrapper, same scoping pattern as doWebSocketClose), stop+restart OTGWstream port 25238, clear MQTT discovery pending bitmap. One [heap-recovery] telnet line reports before/after heap + action bitmask. Redundant yield() removed.
- BGTRACE / BGTASKS_TRACE / _bgPrev / every BGTRACE callsite removed (TASK-397 closed). The handler-list comment block in OTGW-firmware.ino also removed. doBackgroundTasks() and loop()'s post-timer block now call handlers directly.
- loop() body indentation normalised to 4 spaces (was 6/4 mix); the inner _bgPrev scope-block disappeared with BGTRACE.

Tests
- python build.py --firmware: exit 0, binary 0.71 MB (no size regression).
- python evaluate.py --quick: 0 failures, 0 new warnings.
- python evaluate.py (full): String warning count 21 -> 20 as predicted.
- grep checks all clean: no 'String pendingUpgradePath', no 'BGTRACE|BGTASKS_TRACE|_bgPrev', no yield() in emergencyHeapRecovery().

Notes / follow-ups
- startOTGWstream() verified idempotent (calls WiFiServer::begin(); same pattern as runtime applyLegacyPort25238Setting() toggle). Action 2 of ADR-079 retained as planned.
- New wrapper doWebSocketDisconnectAll() added in webSocketStuff.ino to keep helperStuff.ino free of the WebSocketsServer.h include chain (mirrors the existing doWebSocketClose() pattern).
- Branch claude/main-loop-review-tXvWx pushed to origin. PR not yet open — github MCP requires interactive OAuth in this sandbox; URL to open the PR is shown in git push output (rvdbreemen/OTGW-firmware/pull/new/claude/main-loop-review-tXvWx).
- Hardware/beta validation gate is the only AC that cannot be self-verified in this environment; firmware-side correctness is fully covered by the build + evaluator gates.

Closes TASK-673. Closes TASK-397.
<!-- SECTION:FINAL_SUMMARY:END -->
