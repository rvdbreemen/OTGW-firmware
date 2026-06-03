---
id: TASK-818
title: >-
  fix(sat): stop per-tx 'simulation blocked bus tx' WS spam (10/sec floods
  live-log)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-03 05:37'
updated_date: '2026-06-03 15:51'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
satSimulationBlocksBusTx (SATcontrol.ino:1181) emits sendEventToWebSocket_P('!','simulation blocked bus tx') on every blocked bus-tx while simulation is active (~10/sec), flooding the Web UI live-log (10000-line buffer fills with it). The useful signal is already captured: telnet trace (SATDebugTf line 1180, bSAT-gated), state.sat.sLastBlockedCmd, and the TASK-801 trace ring shown on the SAT dashboard; plus the TASK-815 sim-start narration already states 'no bus traffic'. Remove the redundant per-tx WS broadcast.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Per-tx '!' WS event removed; live-log no longer floods during simulation
- [x] #2 Telnet trace + sLastBlockedCmd + trace ring capture retained (no loss of isolation visibility)
- [x] #3 Build clean esp32+esp8266; evaluate green; prerelease bumped
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the per-tx sendEventToWebSocket_P('!','simulation blocked bus tx') in satSimulationBlocksBusTx; it fired ~10/sec and flooded the live-log. Isolation visibility retained via telnet trace + sLastBlockedCmd + trace ring + sim-start narration. alpha.151, both envs build clean, evaluator 0-fail.
<!-- SECTION:FINAL_SUMMARY:END -->
