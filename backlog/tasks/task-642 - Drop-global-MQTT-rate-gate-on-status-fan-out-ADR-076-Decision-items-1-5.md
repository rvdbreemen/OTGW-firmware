---
id: TASK-642
title: Drop global MQTT rate-gate on status fan-out (ADR-076 Decision items 1-5)
status: To Do
assignee: []
created_date: '2026-05-21 07:36'
labels:
  - mqtt
  - adr-076
  - task-402-fixup
  - beta
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement ADR-076 Decision items 1-5: remove the 250 ms MQTT_GATED_PUBLISH_SPACING_MS rate-gate from the status fan-out. The global rate-gate (mqttLastGatedPublishMs token) caused per-bit slot starvation on heartbeat ticks because all sibling slots in a single processOT() fan-out simultaneously hit intervalElapsed=true; the byte slot grabbed the token first and the bit slots lost it for the next 60 s heartbeat too. Bits 2-5 of OT_Statusflags consistently entered HA as unknown as a direct result. After this task, the publish predicate for every gated slot is firstSeen OR valueChanged OR (now - lastPublished >= 60 s) — exactly ADR-052's per-slot contract with no global override. Heap-tier back-pressure via canPublishMQTT() (ADR-030) remains the sole throttle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Static declaration of mqttLastGatedPublishMs is removed from OTGW-Core.ino (was line 289)
- [ ] #2 Constexpr MQTT_GATED_PUBLISH_SPACING_MS is removed from OTGW-Core.ino (was line 290)
- [ ] #3 Rate-gate if-block (intervalElapsed && !forcePublish && mqttLastGatedPublishMs != 0 && (nowMs - mqttLastGatedPublishMs) < MQTT_GATED_PUBLISH_SPACING_MS) is removed from shouldPublishTrackedStatusBit() in OTGW-Core.ino
- [ ] #4 Rate-gate if-block is removed from shouldPublishTrackedStatusByte() in OTGW-Core.ino
- [ ] #5 mqttLastGatedPublishMs = nowMs assignments inside both functions are removed (no remaining writes to the deleted global)
- [ ] #6 mqttLastGatedPublishMs = 0 reset is removed from resetMqttTrackedState() (was line 437)
- [ ] #7 All previously-existing first-seen, valueChanged, and 60 s heartbeat publish paths still set mqttPendingBitSlot/ByteSlot before returning true
- [ ] #8 STATUS_HEARTBEAT_INTERVAL_SEC remains 60 and the intervalElapsed predicate is unchanged
- [ ] #9 Prerelease tag bumped via bin/bump-prerelease.sh and the bumped version.h + data/version.hash are staged with the firmware change
- [ ] #10 python build.py --firmware exits 0
- [ ] #11 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [ ] #12 No code comment in OTGW-Core.ino still references MQTT_GATED_PUBLISH_SPACING_MS, mqttLastGatedPublishMs, or the rate-gate as an active mechanism (references in /// blocks may explain the removal under ADR-076)
- [ ] #13 ADR-076 is cited in at least one code comment near the deleted region as the authority for the removal
<!-- AC:END -->
