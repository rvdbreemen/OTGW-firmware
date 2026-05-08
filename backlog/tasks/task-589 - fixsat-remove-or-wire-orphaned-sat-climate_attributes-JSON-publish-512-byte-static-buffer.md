---
id: TASK-589
title: >-
  fix(sat): remove or wire orphaned sat/climate_attributes JSON publish
  (512-byte static buffer)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-08 17:12'
updated_date: '2026-05-08 21:25'
labels:
  - sat
  - mqtt
  - ha-discovery
  - ram
dependencies: []
references:
  - 'src/OTGW-firmware/SATcontrol.ino:2451-2526'
  - src/OTGW-firmware/MQTTHaDiscovery.cpp
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SATcontrol.ino:2451-2526 publishes a 512-byte JSON blob on `sat/climate_attributes` on every SAT cycle. No HA auto-discovery entry registers this topic as a `json_attributes_topic`. The payload is consumed by nobody.

This is both an ADR-101 violation (JSON on a value-adjacent topic) and a RAM concern: the 512-byte `static char` buffer is permanently allocated. A comment says "Task #72 — Publishes sat/climate_attributes for HA json_attributes_topic" but the discovery side was never wired up.

JSON payload fields: optimal_coefficient, boiler_flame_timing, error_pid, kp, ki, kd, dt, max_output, min_output, curve_value, output, output_limited, setpoint, heating_curve_version, pwm_output, target_override.

Resolution options:
1. Wire to MQTTHaDiscovery.cpp as json_attributes_topic for the sat/climate entity (preferred — all 16 fields become HA attributes without 16 separate topics).
2. Remove entirely if the data is no longer useful.
3. Expose individual fields as flat scalar topics (ADR-101 purist approach — but 16 extra topics).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The 512-byte static char buffer in SATcontrol.ino is either removed or justified by a wired discovery entry
- [ ] #2 If kept: MQTTHaDiscovery.cpp has a json_attributes_topic entry pointing to sat/climate_attributes for the climate entity
- [ ] #3 If removed: the entire publish block at SATcontrol.ino:2451-2526 is deleted along with any related discovery config
- [ ] #4 No orphaned MQTT publish on sat/climate_attributes remains
- [ ] #5 Build passes and evaluator shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add json_attributes_topic field to streamClimateDiscovery (climateIdx==0) in mqtt_configuratie.cpp, pointing to <mqttPubTopic>/sat/climate_attributes.\n2. Build verify with python build.py --firmware.\n3. Run python evaluate.py --quick.\n4. Bump prerelease, commit, push.
<!-- SECTION:PLAN:END -->
