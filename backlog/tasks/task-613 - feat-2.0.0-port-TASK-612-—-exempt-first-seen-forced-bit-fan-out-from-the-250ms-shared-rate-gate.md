---
id: TASK-613
title: >-
  feat-2.0.0: port TASK-612 — exempt first-seen/forced bit fan-out from the
  250ms shared rate-gate
status: To Do
assignee: []
created_date: '2026-05-16 11:00'
labels:
  - mqtt
  - bug
  - feat-2.0.0
dependencies: []
priority: high
ordinal: 35000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev-line TASK-612 (PR rvdbreemen/OTGW-firmware#572) to the 2.0.0 ESP32/SAT feature line.

The 2.0.0 OTGW-Core.ino carries the identical defect verified by static analysis: shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte() route every bit/byte sub-topic (Status/StatusVH, ASF/RBP/VH/RemoteOverride) through a single global mqttLastGatedPublishMs (MQTT_GATED_PUBLISH_SPACING_MS=250ms). A parent OT message decodes its whole fan-out inside one processOT() call, so only the first slot passes the spacing gate; the rest defer (firstSeen stays true) and retry only on the next parent frame. msgId 0 Status recurs ~3s and changes (valueChanged bypasses the gate) so it self-heals; rarely-polled ASF(5)/RBP(6)/VH parents starve, leaving those HA entities 'unknown' after a clean boot.

Fix (identical to dev): in both gate functions, rate-gate ONLY the recurring 60s heartbeat (intervalElapsed && !forcePublish); the one-shot first-seen and forced bursts bypass the 250ms spacing, exactly as valueChanged already does. Net behaviour: publish on first sight, then only on change or every 60s.

Platform note: this is platform-independent MQTT-publish-gating logic; no ESP32/SAT-specific deviation from the dev fix. Line numbers differ from dev (2.0.0 carries extra SAT/ESP32 code) but the code blocks are byte-identical pre-fix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Both shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte() exempt first-seen/forced from the 250ms gate; only the 60s heartbeat remains rate-gated
- [ ] #2 Fix is semantically identical to dev TASK-612 (PR #572); no ESP32/SAT-specific divergence
- [ ] #3 Prerelease bumped per 2.0.0 scheme (alpha.N -> alpha.N+1)
- [ ] #4 Evaluator green vs 2.0.0 baseline (python evaluate.py --quick no new failures)
- [ ] #5 Branch claude/port-task612-bitgate-2.0.0 pushed; draft PR opened against feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- AC:END -->
