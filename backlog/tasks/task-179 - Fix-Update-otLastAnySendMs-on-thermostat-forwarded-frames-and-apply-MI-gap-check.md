---
id: TASK-179
title: >-
  Fix: Update otLastAnySendMs on thermostat-forwarded frames and apply MI= gap
  check
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:31'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two frame-send paths in loopOTDirect() bypass the MI= inter-message gap tracking: the monitor-mode transparent forward (line 1187) and the gateway-mode normal forward path (line 1227) both call sendMasterRequestAsync() but neither updates otLastAnySendMs after success, nor are they gated by the MI= gap check. This means a thermostat frame can fire immediately after a scheduled poll, violating the otMinIntervalMs guarantee. Fix: update otLastAnySendMs inside sendMasterRequestAsync() on every successful real-bus send (not loopback), so all paths are covered automatically. Loopback mode must not update the real-bus gap timer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otLastAnySendMs is updated for every frame successfully sent to the real OT bus, regardless of origin
- [x] #2 Thermostat-forward paths in loopOTDirect() respect the MI= inter-message gap before forwarding
- [x] #3 scheduleMasterRequest() MI= check is not broken by the fix
- [x] #4 Loopback mode is not affected (simulated sends should not update the real-bus gap timer)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Moved otLastAnySendMs update into sendMasterRequestAsync() for the real bus path.

Previously, thermostat-forward calls to sendMasterRequestAsync() in loopOTDirect() did not update otLastAnySendMs, meaning the MI= inter-message gap was not tracked for those frames. This could allow back-to-back frames without the configured minimum gap.

Fix: update otLastAnySendMs = millis() inside sendMasterRequestAsync() immediately after a successful otMaster.sendRequestAsync() call. Loopback path is excluded (no real bus activity). Removed duplicate otLastAnySendMs assignments from scheduleMasterRequest() callers.
<!-- SECTION:FINAL_SUMMARY:END -->
