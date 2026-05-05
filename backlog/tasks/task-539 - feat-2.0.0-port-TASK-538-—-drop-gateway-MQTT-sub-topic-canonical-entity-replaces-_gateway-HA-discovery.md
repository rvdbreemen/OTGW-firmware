---
id: TASK-539
title: >-
  feat-2.0.0: port TASK-538 — drop /gateway MQTT sub-topic, canonical entity
  replaces _gateway HA discovery
status: To Do
assignee: []
created_date: '2026-05-05 05:10'
labels:
  - mqtt
  - ha-discovery
  - port
  - feat-2.0.0
dependencies:
  - TASK-538
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-branch fix from TASK-538 to feature-dev-2.0.0-otgw32-esp32-sat-support. The 2.0.0 branch may diverge in MQTT layout (ESP32 + OTDirect + SAT support added), so the same logical change must be re-applied carefully rather than cherry-picked blindly. Goal: with bSeparateSources=true on 2.0.0, the firmware emits {canonical, /thermostat, /boiler} sub-topics and HA discovery entities — no /gateway sub-topic, no _gateway entity. The third per-source discovery variant becomes the canonical (empty suffix/name/segment).\n\nReference implementation on dev (master path):\n- src/OTGW-firmware/MQTTstuff.ino: resolveSourceIndex drops OTGW_REQUEST_BOILER; mqttSourceKeys[] shrunk to 2 entries with MQTT_SOURCE_KEY_COUNT constant; copySourceTableEntry bound check uses the constant.\n- src/OTGW-firmware/mqtt_configuratie.cpp: expandAndStreamSensorSources renamed gateway -> canonical (empty suffix/name/segment); kSourceVariantCount derived from sizeof.\n\nVerify equivalent files exist on 2.0.0 — if MQTT subsystem was refactored for ESP32 / OTDirect, locate the corresponding hooks before porting. Pay attention to:\n- Any new source classifications introduced for OTDirect (where the OTGW is BOTH master and slave on different sides) — does OTDirect introduce a 4th source category that should map to canonical too?\n- ESP32-side MQTT publish path: confirm canonical-topic write still precedes the per-source publish call.\n- HA discovery entity shape on 2.0.0 (may use a different builder).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Locate the equivalent of resolveSourceIndex / mqttSourceKeys on feature-dev-2.0.0 and apply the same drop-gateway change
- [ ] #2 Locate the equivalent of expandAndStreamSensorSources on feature-dev-2.0.0 and rename gateway variant to canonical
- [ ] #3 Verify OTDirect / SAT source-classification additions (if any) are routed correctly — overrides go to canonical, raw side-traffic stays per-source
- [ ] #4 Build 2.0.0 firmware + filesystem successfully
- [ ] #5 evaluate.py --quick shows no new failures
- [ ] #6 Manual MQTT sub: no .../<label>/gateway topics published with bSeparateSources=true; no homeassistant/.../_gateway/config
<!-- AC:END -->
