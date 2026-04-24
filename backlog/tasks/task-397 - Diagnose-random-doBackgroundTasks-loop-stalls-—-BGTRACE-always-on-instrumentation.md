---
id: TASK-397
title: >-
  Diagnose random doBackgroundTasks loop stalls — BGTRACE always-on
  instrumentation
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-24 09:37'
updated_date: '2026-04-24 09:37'
labels:
  - debug
  - diagnostics
  - loop-stall
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User reports that the main loop breaks randomly on dev (1.4.x) while it was stable on 1.3.x. Diff analysis found three new calls in the background-task chain since v1.3.5: debugTelnet.loop(), OTGWstream.loop() (both SimpleTelnet-library), and loopMQTTDiscovery(). Add per-handler micros() timing + heap snapshot instrumentation under a single #define BGTASKS_TRACE toggle. Always-on logging per handler per iteration so the last-seen handler before a stall identifies the culprit. Diagnostic build; revert after root cause found.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 BGTRACE macro + #define BGTASKS_TRACE toggle in OTGW-firmware.ino (or a debug header). When enabled, every handler in doBackgroundTasks and loop emits one line with handler name, duration in microseconds, heap, max block size.
- [ ] #2 Instrumentation covers all new-since-v1.3.5 handlers: debugTelnet.loop, OTGWstream.loop, loopMQTTDiscovery — plus the pre-existing chain (handleDebug, handleMQTT, handleOTGW, handleWebSocket, httpServer.handleClient, MDNS.update, loopNTP, evalOutputs, evalWebhook, handlePendingUpgrade).
- [ ] #3 Build clean on firmware target. Diagnostic only — not intended to ship; the #define stays at 1 while debugging, set to 0 to disable at compile time.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add #define BGTASKS_TRACE 1 + BGTRACE macro at top of OTGW-firmware.ino.
2. Instrument doBackgroundTasks: declare local _bgPrev before handler chain, call BGTRACE after each handler.
3. Instrument loop: local _bgPrev in the !isFlashing block, BGTRACE after loopMQTTDiscovery + evalOutputs + evalWebhook + handlePendingUpgrade.
4. Build, commit, push.
5. User flashes, runs until stall, pastes last lines; culprit identified by last-seen BGTRACE.
<!-- SECTION:PLAN:END -->
