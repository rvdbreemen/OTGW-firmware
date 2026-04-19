---
id: TASK-320
title: '[ARCH-L3] Move heating_mode string-payload dispatch from MQTT to SATcontrol'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:25'
updated_date: '2026-04-19 07:24'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.ino:607-614 has inner strcasecmp_P for eco/comfort payload, a two-level string-token discriminator. Belongs in SATcontrol.ino next to the heating-mode enum.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satHandleHeatingMode(const char*) added in SATcontrol.ino
- [x] #2 MQTTstuff callback reduced to a one-line delegation
- [x] #3 Behavior unchanged for eco/comfort and numeric payloads
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added satHandleHeatingMode(const char*) in SATcontrol.ino alongside satHandleControlMode, with forward declaration in OTGW-firmware.h. MQTTstuff.ino:606 and restAPI.ino:928 now delegate with a single function call instead of duplicating the eco/comfort mapping. MQTT/REST transport layers stay free of SAT-specific string tokens; behaviour unchanged for eco/comfort and bare numeric payloads.
<!-- SECTION:FINAL_SUMMARY:END -->
