---
id: TASK-397
title: >-
  Diagnose random doBackgroundTasks loop stalls — BGTRACE always-on
  instrumentation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 09:37'
updated_date: '2026-04-24 19:00'
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
- [x] #1 BGTRACE macro + #define BGTASKS_TRACE toggle in OTGW-firmware.ino (or a debug header). When enabled, every handler in doBackgroundTasks and loop emits one line with handler name, duration in microseconds, heap, max block size.
- [x] #2 Instrumentation covers all new-since-v1.3.5 handlers: debugTelnet.loop, OTGWstream.loop, loopMQTTDiscovery — plus the pre-existing chain (handleDebug, handleMQTT, handleOTGW, handleWebSocket, httpServer.handleClient, MDNS.update, loopNTP, evalOutputs, evalWebhook, handlePendingUpgrade).
- [x] #3 Build clean on firmware target. Diagnostic only — not intended to ship; the #define stays at 1 while debugging, set to 0 to disable at compile time.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add #define BGTASKS_TRACE 1 + BGTRACE macro at top of OTGW-firmware.ino.
2. Instrument doBackgroundTasks: declare local _bgPrev before handler chain, call BGTRACE after each handler.
3. Instrument loop: local _bgPrev in the !isFlashing block, BGTRACE after loopMQTTDiscovery + evalOutputs + evalWebhook + handlePendingUpgrade.
4. Build, commit, push.
5. User flashes, runs until stall, pastes last lines; culprit identified by last-seen BGTRACE.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Diagnostic instrumentation ran its full lifecycle: added on dev, observed stalls in the field with last-seen probe pointing at the debugTelnet.loop/OTGWstream.loop path, and has now been removed per its own "revert after root cause found" mandate (TASK-404).

**Root causes identified via the BGTRACE trail:**
1. SimpleTelnet printf-stack buffer overflow under hot paths (64-byte stack truncated format strings and could smash adjacent stack variables). Fixed upstream by bumping SIMPLETELNET_PRINTF_STACK_LEN to 256 and committed as SimpleTelnet `25a0250`, pulled in via submodule bump to `cc4c88e9`.
2. ESP8266 Arduino Core 3.1.2 regression (PR esp8266/Arduino#8598 removed implicit WiFiClient/WiFiUDP::stopAll()) left lwIP state in a half-associated condition across resets. Addressed by rolling the ESP8266 target back to Core 2.7.4 LTS via espressif8266@2.6.3 in platformio.ini (TASK-398).
3. Secondary: the loopMQTTDiscovery drip cadence overlapped Status bursts. Addressed by the TASK-342/347 status-burst quiesce + cooldown (already landed on 2.0.0 pre-merge).

**Cleanup:** The BGTRACE macro and BGTASKS_TRACE toggle were removed from 2.0.0 via commit bb3f95f7 (TASK-404). The handler chain they used to wrap was already refactored on 2.0.0 (handlePICSerial/loopOTDirect replaced handleOTGW/OTGWstream.loop), so re-introducing the probes on 2.0.0 would require redoing them against the new handler names — not needed unless a new stall symptom appears.

**AC reconciliation:**
- AC #1 instrumentation was present on dev during the diagnostic window (satisfied at the time). Marked complete by virtue of the stall having been traced to its root cause via the added probes.
- AC #2 all new-since-v1.3.5 handlers were covered (debugTelnet.loop, OTGWstream.loop, loopMQTTDiscovery) per the commit that added them.
- AC #3 builds were clean with BGTASKS_TRACE=1 and 0 during the diagnostic window.

**Superseded by:** TASK-399 (SimpleTelnet stack bump), TASK-398 (Core 2.7.4 rollback), TASK-404 (probe cleanup).
<!-- SECTION:FINAL_SUMMARY:END -->
