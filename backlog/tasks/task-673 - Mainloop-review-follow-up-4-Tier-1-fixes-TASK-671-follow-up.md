---
id: TASK-673
title: 'Mainloop review follow-up: 4 Tier-1 fixes (TASK-671 follow-up)'
status: To Do
assignee: []
created_date: '2026-05-23 05:28'
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
- [ ] #1 pendingUpgradePath is static char[80]; OTGW-Core.ino bevat geen String voor deze variabele meer
- [ ] #2 emergencyHeapRecovery() implementeert ADR-acties (WS-disconnect, OTGWstream stop+start, clearMQTTConfigPending); telnet log toont before/after heap
- [ ] #3 Nieuwe ADR is geaccepteerd en gerefereerd vanuit helperStuff.ino
- [ ] #4 BGTRACE volledig verwijderd: geen BGTASKS_TRACE define, geen BGTRACE macro, geen _bgPrev variabelen, geen BGTRACE(...) callsites; TASK-397 op Done
- [ ] #5 loop() body uniform geindenteerd 4 spaces; geen gedragswijziging
- [ ] #6 python build.py --firmware exit 0
- [ ] #7 python evaluate.py --quick geen nieuwe failures (1 String-warning minder)
<!-- AC:END -->
