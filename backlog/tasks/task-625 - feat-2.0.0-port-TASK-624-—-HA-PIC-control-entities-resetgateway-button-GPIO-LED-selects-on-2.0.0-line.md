---
id: TASK-625
title: >-
  feat-2.0.0: port TASK-624 — HA PIC-control entities (resetgateway button +
  GPIO/LED selects) on 2.0.0 line
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-18 07:54'
updated_date: '2026-05-18 08:12'
labels: []
dependencies:
  - TASK-624
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the PR#576 feature (TASK-624) to the feature-dev-2.0.0-otgw32-esp32-sat-support line. The 2.0.0 line keeps HA discovery in MQTTHaDiscovery.cpp (different file/conventions from dev's mqtt_configuratie.cpp) and is at version alpha.36. Apply the same architectural decision as TASK-624: publish the PIC-control discovery configs unconditionally (no isPICEnabled() gate) matching that branch's existing PIC pseudo-ID convention, so there is no infinite-retry. Use a free pseudo-ID on that branch (PR#576 description suggested 244; verify against MQTTHaDiscovery.cpp before choosing). Delivered on a separate branch off feature-dev-2.0.0-otgw32-esp32-sat-support with its own draft PR targeting that branch.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Feature ported into MQTTHaDiscovery.cpp using the 2.0.0 branch's streaming/discovery conventions: resetgateway button + gpioa/gpiob/leda-ledf selects + MQTT set-commands (GA/GB/LA-LF/SC + resetgateway)
- [ ] #2 Discovery configs publish unconditionally (no isPICEnabled() gate) consistent with the 2.0.0 branch's existing PIC pseudo-ID handling; no infinite-retry
- [ ] #3 A free pseudo-ID is chosen and verified unused on the 2.0.0 branch; ESP8266 + ESP32-S3 considerations reviewed
- [ ] #4 Version bumped per policy on the 2.0.0 line (alpha.36 -> alpha.37)
- [ ] #5 build green for the 2.0.0 target; evaluator shows no new failures vs that branch baseline
- [ ] #6 docs/api/MQTT.md (2.0.0) updated with the new entities/commands
- [ ] #7 Delivered on a dedicated branch off feature-dev-2.0.0-otgw32-esp32-sat-support with a draft PR targeting that branch; cross-referenced to TASK-624 / PR#576
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-firmware.h: add OTGWpiccontrolsid=244 + comment
2. MQTTstuff.h: add HaIcon restart/pin/led_outline before _count; declare streamButtonDiscovery + streamSelectDiscovery
3. MQTTHaDiscovery.cpp: add haIconStr restart/pin/led-outline cases; ADD MISSING haEntityCatStr config case (2.0.0 bug); append composeButtonPayload/streamButtonDiscovery + composeSelectPayload/streamSelectDiscovery (port of dev code, 2.0.0 helper API)
4. MQTTstuff.ino: add s_reset + s_cmd_*/s_otgw_* PROGMEM + setcmds rows; resetgateway->resetOTGW() branch in handleMQTT dispatch; OTGWpiccontrolsid into markAllMQTTConfigPending (parity with 249/250/diag200 — NOT publishNonOT, matching 2.0.0 convention); doAutoConfigureMsgid pseudo-ID 244 block WITHOUT isPICEnabled gate
5. docs/api/MQTT.md (2.0.0): set-commands + PIC Control Entities section
6. bin/bump-prerelease.sh (alpha.36->alpha.37)
7. build + evaluate for 2.0.0 target
8. commit, push -u origin claude/port-pr576-2.0.0-GvEv0, draft PR -> feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:PLAN:END -->
