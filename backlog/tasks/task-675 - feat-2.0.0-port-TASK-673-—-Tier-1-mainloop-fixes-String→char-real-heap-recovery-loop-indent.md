---
id: TASK-675
title: >-
  feat-2.0.0: port TASK-673 — Tier-1 mainloop fixes (String→char[], real heap
  recovery, loop indent)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-23 05:29'
updated_date: '2026-05-23 05:47'
labels:
  - responsiveness
  - refactor
  - mainloop
  - 2.0.0
dependencies: []
priority: medium
ordinal: 53000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling-port van TASK-673 (dev). Bevat fix 1, 2, 4. Geen fix 3 (BGTRACE bestaat niet in 2.0.0).

Master plan: /root/.claude/plans/hoe-zou-je-tier-cryptic-pascal.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 pendingUpgradePath is static char[80] binnen #if HAS_PIC; geen String meer
- [x] #2 emergencyHeapRecovery() implementeert ADR-acties met platformFreeHeap(); telnet log toont before/after
- [x] #3 Nieuwe ADR is geaccepteerd en verwijst naar dev sibling-ADR
- [x] #4 loop() body uniform geindenteerd 4 spaces (incl. 2.0.0-specifieke regels)
- [ ] #5 python build.py --firmware exit 0 voor zowel ESP8266 als ESP32 targets
- [x] #6 python evaluate.py --quick geen nieuwe failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete.

- Fix 1: pendingUpgradePath String -> static char[80] inside #if HAS_PIC; 5 callsites refactored to use direct buffer access and snprintf_P.
- Fix 2: emergencyHeapRecovery() implements ADR-107 three actions. Divergence from plan: 2.0.0 has startPICStream() not startOTGWstream(); used that. Also discovered webSocket is not extern-d in any header, so added a doWebSocketDisconnectAll() wrapper in webSocketStuff.ino (same scoping pattern as existing doWebSocketClose) and called that from helperStuff.ino. Used platformFreeHeap() per ADR-107 enforcement.
- Fix 4: loop() body indentation normalised; git diff -w confirmed whitespace-only.
- BGTRACE grep: 0 hits in 2.0.0 src/OTGW-firmware/ (as expected, never existed).

Build: ESP8266 target builds clean (0.82 MB merged binary). ESP32 target build fails in this sandbox because PlatformIO cannot download framework-arduinoespressif32 (SSL self-signed cert in chain to github.com). This is a pre-existing environmental constraint, not a code issue. AC #5 left unchecked pending validation in an environment with working ESP32 toolchain access.

Evaluator: 60 passed, 0 failed, 1 pre-existing warning (mqtt_configuratie.cpp not found). Health 98.5

BLOCKER: cannot create signed commit. /home/claude/.ssh/commit_signing_key.pub is empty (0 bytes); the /tmp/code-sign helper reports 'missing source' from the signing server (status 400). Infrastructure issue, not a code issue. All code changes are staged and ready; the working tree is clean apart from these staged changes. Build (ESP8266) green, evaluator green, all static checks pass. Requires either a working signing key or an explicit user request to bypass signing for this single commit.
<!-- SECTION:NOTES:END -->
