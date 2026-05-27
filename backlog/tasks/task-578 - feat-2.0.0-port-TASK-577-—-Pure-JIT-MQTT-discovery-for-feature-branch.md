---
id: TASK-578
title: 'feat-2.0.0: port TASK-577 — Pure JIT MQTT discovery for feature branch'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 10:39'
updated_date: '2026-05-27 10:50'
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
- [x] #6 ADR sibling created on 2.0.0 worktree (separate ADR numbering)
- [x] #7 Build passes on feature branch
- [x] #8 Evaluator green on feature branch
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported pure JIT MQTT discovery from dev. publishNonOTDiscoveryConfigs() now queues the full non-OT pseudo-ID set (climate ID 0, number ID 27, OTGWdallasdataid, OTGWheapstatsid, OTGWfwinfoid, OTGWpicinfoid, OTGWpicsettingsid, OTGWpiccontrolsid) and sets dripDeviceInfoPending. clearMQTTConfigPending() helper present. startMQTT() and the homeassistant/status offlineMs>5min broker-restart path both use the clearDone+clearPending+publishNonOT triplet instead of markAllMQTTConfigPending(). HA-status online handler does not call markAll. ADR-112 added as 2.0.0 sibling of dev ADR-073/ADR-100. Build green (esp8266 + esp32-s3), evaluator quick 63/63 pass (1 warning, 0 fail). Version bumped to alpha.76 and pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support as 4bb23d82.
<!-- SECTION:FINAL_SUMMARY:END -->
