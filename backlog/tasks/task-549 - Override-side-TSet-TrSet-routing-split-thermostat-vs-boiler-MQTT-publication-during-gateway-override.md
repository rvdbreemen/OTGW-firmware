---
id: TASK-549
title: >-
  Override-side TSet/TrSet routing: split thermostat vs boiler MQTT publication
  during gateway override
status: To Do
assignee: []
created_date: '2026-05-06 23:02'
labels:
  - mqtt
  - routing
  - override
  - ha-discovery
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When a gateway override (CS=, TC=, TT=) is active, the thermostat-originated message (T) is tagged «<ignored>» (skipthis=true in OTGW-Core.ino:4046-4051) and never published to MQTT. Only the gateway-substituted R message reaches the canonical topic, which means /otgw-pic/value/TSet shows the override value but the original thermostat-side TSet is lost.

Observed (CS=27.37 active, thermostat asks 23.00):
- T10011700 TSet=23.00 → logged as <ignored>, not published
- R10011B5F TSet=27.37 → published to /otgw-pic/value/TSet only (no /thermostat or /boiler subtopic, per resolveSourceIndex in MQTTstuff.ino:1185)

User expectation (Robert, beta-testing 1.5.0-beta.16):
- /thermostat/TSet = 23.00 (real thermostat request)
- /boiler/TSet = 27.37 (what was actually sent to boiler = override when active, =thermostat when no override)

This bundles three related concerns:
1. TSet routing: stop discarding the thermostat-side value during override; publish T value to /thermostat subtopic and R/canonical value to /boiler subtopic.
2. HA discovery for nested (per-source) sensors: verify the discovery payload covers /thermostat/* and /boiler/* subtopics so users do not have to hand-configure sensors. Today it appears flat / canonical-only.
3. Override visibility: confirm whether gateway override-active is exposed somewhere (eg /gateway/CS or a state flag) so a user can correlate the boiler-side value with the active override.

Files in scope: src/OTGW-firmware/OTGW-Core.ino (skipthis logic ~L4035-4060), src/OTGW-firmware/MQTTstuff.ino (resolveSourceIndex ~L1185, publishToSourceTopic ~L1209, mqttSourceKeys ~L442). Related ADR: ADR-066 (Write-Ack echo gate) and ADR-065 (otgw-pic/ subtree as public API) — any new subtopic shape must respect those.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Thermostat-originated T message for write-data IDs (eg TSet=1, TrSet=16, MaxRelModLevelSetting=14, TdhwSet=56) publishes its value to /otgw-pic/value/<id>/thermostat even when the gateway substitutes a different value via R
- [ ] #2 Boiler-side value (R when gateway override is active, T pass-through when no override) publishes to /otgw-pic/value/<id>/boiler
- [ ] #3 Canonical /otgw-pic/value/<id> continues to publish the value actually sent to the boiler (back-compat with ADR-065)
- [ ] #4 HA discovery payload (when bSeparateSources=true) registers entities for the /thermostat and /boiler subtopics so they appear in HA without manual sensor configuration
- [ ] #5 Override visibility: a user can determine from MQTT alone whether the boiler value equals the thermostat value or a gateway override (either via existing CS/TT/TC topics or a new gateway-state topic — design choice documented in Final Summary)
- [ ] #6 Verified on hardware with an active CS=<value> override: thermostat topic shows the thermostat's request, boiler topic shows the override value, both update independently
- [ ] #7 No regression to canonical /otgw-pic/value/<id> consumers (existing HA installations keep working)
<!-- AC:END -->
