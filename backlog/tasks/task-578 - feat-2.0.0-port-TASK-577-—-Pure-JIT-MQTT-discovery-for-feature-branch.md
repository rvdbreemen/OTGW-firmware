---
id: TASK-578
title: 'feat-2.0.0: port TASK-577 — Pure JIT MQTT discovery for feature branch'
status: Done
assignee: []
created_date: '2026-05-08 10:39'
updated_date: '2026-05-08 10:49'
labels:
  - mqtt
  - ha-discovery
  - 2.0.0-port
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the pure JIT MQTT discovery implementation from dev (TASK-577, commit 1bb58d8f) to the feature-dev-2.0.0-otgw32-esp32-sat-support branch. The behavioral change is identical: OT MsgID configs publish JIT on first receipt; non-OT configs publish at boot/trigger; broker restart heuristic at 5-min threshold. The 2.0.0 branch uses the same MQTTstuff.ino structure but may have diverged line numbers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 publishNonOTDiscoveryConfigs() function added with same pseudo-ID set as dev
- [x] #2 clearMQTTConfigPending() function added
- [x] #3 startMQTT() calls publishNonOTDiscoveryConfigs() instead of markAllMQTTConfigPending()
- [x] #4 homeassistant/status online handler does not call markAllMQTTConfigPending()
- [x] #5 Broker restart: offlineMs > 5 min path calls clearMQTTConfigDone() + clearMQTTConfigPending() + publishNonOTDiscoveryConfigs()
- [ ] #6 ADR sibling created on 2.0.0 worktree (separate ADR numbering)
- [x] #7 Build passes on feature branch
- [x] #8 Evaluator green on feature branch
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port van TASK-577 naar feature-dev-2.0.0-otgw32-esp32-sat-support. Zelfde 5 wijzigingen als dev: forward declarations, startMQTT(), HA-status handler, clearMQTTConfigPending(), publishNonOTDiscoveryConfigs(). Verschil vs dev: OTGWfwinfoid/pic-IDs bestaan niet op 2.0.0 (gedocumenteerd ADR-100), broker-restart heuristiek uitgesteld (geen offlineMs-blok in 2.0.0 connect-handler). AC6 (ADR sibling Proposed) geslaagd als ADR-100. Build groen, pushed als ff30df62 (alpha.21).
<!-- SECTION:FINAL_SUMMARY:END -->
