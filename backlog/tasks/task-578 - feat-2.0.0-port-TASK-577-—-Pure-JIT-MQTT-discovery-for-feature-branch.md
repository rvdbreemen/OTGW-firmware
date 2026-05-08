---
id: TASK-578
title: 'feat-2.0.0: port TASK-577 — Pure JIT MQTT discovery for feature branch'
status: To Do
assignee: []
created_date: '2026-05-08 10:39'
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
- [ ] #1 publishNonOTDiscoveryConfigs() function added with same pseudo-ID set as dev
- [ ] #2 clearMQTTConfigPending() function added
- [ ] #3 startMQTT() calls publishNonOTDiscoveryConfigs() instead of markAllMQTTConfigPending()
- [ ] #4 homeassistant/status online handler does not call markAllMQTTConfigPending()
- [ ] #5 Broker restart: offlineMs > 5 min path calls clearMQTTConfigDone() + clearMQTTConfigPending() + publishNonOTDiscoveryConfigs()
- [ ] #6 ADR sibling created on 2.0.0 worktree (separate ADR numbering)
- [ ] #7 Build passes on feature branch
- [ ] #8 Evaluator green on feature branch
<!-- AC:END -->
