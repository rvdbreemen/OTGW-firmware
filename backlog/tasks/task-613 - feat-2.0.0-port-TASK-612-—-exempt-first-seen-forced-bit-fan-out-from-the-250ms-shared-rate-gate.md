---
id: TASK-613
title: >-
  feat-2.0.0: port TASK-612 — exempt first-seen/forced bit fan-out from the
  250ms shared rate-gate
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 11:00'
updated_date: '2026-05-16 11:09'
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
- [x] #1 Both shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte() exempt first-seen/forced from the 250ms gate; only the 60s heartbeat remains rate-gated
- [x] #2 Fix is semantically identical to dev TASK-612 (PR #572); no ESP32/SAT-specific divergence
- [x] #3 Prerelease bumped per 2.0.0 scheme (alpha.N -> alpha.N+1)
- [x] #4 Evaluator green vs 2.0.0 baseline (python evaluate.py --quick no new failures)
- [x] #5 Branch claude/port-task612-bitgate-2.0.0 pushed; draft PR opened against feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports dev-line TASK-612 (PR #572) to the 2.0.0 line. Draft PR #588 -> feature-dev-2.0.0-otgw32-esp32-sat-support.

Static analysis confirmed 2.0.0 OTGW-Core.ino carried the byte-identical defect: shouldPublishTrackedStatusBit/Byte() throttle the first-seen/forced/heartbeat branch with a single global mqttLastGatedPublishMs (250ms) shared across every bit/byte slot; a parent's whole fan-out decodes in one processOT() call so only the first slot passes and the rest defer to the next (rarely-occurring for ASF/RBP/VH) parent frame, leaving those HA entities 'unknown'.

Fix (identical to dev, no ESP32/SAT divergence): rate-gate only the recurring 60s heartbeat (intervalElapsed && !forcePublish); first-seen/forced bursts bypass the 250ms spacing as valueChanged already does. Both gate functions patched. Prerelease alpha.34 -> alpha.35.

Verification: evaluator 95.6% unchanged vs 2.0.0 baseline (50a460b6) before/after; the single failure is the pre-existing 2.0.0-baseline PROGMEM check (2 violations), not introduced here. Firmware build not runnable locally (arduino-cli network-blocked) but PR #588 CI runs pio run -e esp32 and -e esp8266 which verify both targets.

Delivery note: committed (signed, 0cdb3d40) from a worktree relocated under the workspace root because the managed signing proxy rejects sibling-path worktrees ('missing source'); no signing bypassed; CLAUDE.md worktree isolation honoured.

Left In Progress pending PR #588 CI build green + field validation (parity with PR #572: every bit sub-topic >=1 publish within the first minute of OT traffic after a clean boot), mirroring how dev-side TASK-612 is gated.
<!-- SECTION:FINAL_SUMMARY:END -->
