---
id: TASK-557
title: >-
  feat-2.0.0: port TASK-556 — flip discovery topic shape to sibling-suffix +
  draft ADR-098 (supersedes ADR-097)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 11:05'
updated_date: '2026-05-07 21:56'
labels:
  - mqtt
  - discovery
  - ha-integration
  - bug
  - 2.0.0
  - supersedes-adr-097
dependencies: []
priority: high
ordinal: 22000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 mirror of dev TASK-556 (ADR-071). Same root cause: HA's discovery TOPIC_MATCHER regex (homeassistant/components/mqtt/discovery.py:63-66) rejects nested discovery topics because object_id character class [a-zA-Z0-9_-]+ excludes the forward slash. ADR-097 (the 2.0.0-side mirror of ADR-070) inherited the same flawed carve-out and must be superseded by ADR-098 in the 2.0.0 worktree.

Two-step delivery on this branch:
1. Draft ADR-098 in docs/adr/ following the supersession protocol (mirror ADR-071 from dev, adapt references to 2.0.0 file paths and ADR numbers).
2. Apply the same code change to mqtt_configuratie.cpp buildSensorDiscoveryTopic on the 2.0.0 branch (path/line offsets may differ vs dev).

Coordinated with dev TASK-556. Both worktrees adopt the discovery sibling-suffix shape on the same calendar day, mirroring the ADR-070/ADR-097 deployment pattern.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-098 drafted in docs/adr/ as Proposed; references TOPIC_MATCHER regex empirical evidence from dev ADR-071
- [x] #2 ADR-098 reviewed and approved by user; status flipped from Proposed to Accepted before implementation begins
- [x] #3 ADR-097 status line updated to 'Superseded by ADR-098, YYYY-MM-DD'; body of ADR-097 unchanged (immutability protocol)
- [x] #4 buildSensorDiscoveryTopic in mqtt_configuratie.cpp on the 2.0.0 branch changed: source-variant branch format string is '%s/sensor/%s/%s_%s/config' (sibling-suffix)
- [x] #5 Canonical-branch format string is unchanged
- [x] #6 ADR-097 Enforcement carve-out comment is removed or updated to reference ADR-098
- [x] #7 ADR-098 Enforcement block forbid_pattern matches the OLD nested format and would catch any regression
- [x] #8 Build for ESP8266 target exits 0 with no new warnings
- [x] #9 Build for ESP32-S3 target exits 0 with no new warnings
- [ ] #10 Field test on a beta unit (ESP8266 or ESP32-S3) with bSeparateSources=true confirms HA registers source-variant entities where they did NOT register before the change
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-07: ADR-098 drafted at docs/adr/ADR-098-mqtt-discovery-topic-sibling-suffix-shape.md (Status: Proposed; awaiting user review per project policy — never self-accept).

Code site identified for the planned change (note: differs from task description, which referenced mqtt_configuratie.cpp; that file does not exist in the 2.0.0 worktree):
- src/OTGW-firmware/MQTTHaDiscovery.cpp:2320-2336 — `buildSensorDiscoveryTopic`
- Line 2329 currently uses `PSTR("%s/sensor/%s/%s/%s/config")` for the source-variant branch (the format being retired by ADR-098).
- Line 2332 uses `PSTR("%s/sensor/%s/%s/config")` for the canonical branch (unchanged by ADR-098).

No code touched. AC #1 marked checked. Implementation gated on user accepting ADR-098.

Implementation phase started (ADR-098 Accepted by user 2026-05-07).
Code site: MQTTHaDiscovery.cpp:2329 (sibling agent identified line in TASK-557 prep notes).
Pushing held until user explicit approval per 2.0.0 push policy.

Implementation complete (commit b537beff, local on feature-dev-2.0.0). MQTTHaDiscovery.cpp:2329 flipped to sibling-suffix format. ADR-097 superseded by ADR-098. Both ESP8266 (Flash 80.3% / RAM 85.2%) and ESP32-S3 (Flash 98.0% / RAM 31.9%) builds clean. Evaluator 59/2/0 unchanged. ADR-098 forbid_pattern verified — no nested format remaining in codebase. Push held for explicit user approval per 2.0.0 push policy.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports dev TASK-556 / ADR-071 to the 2.0.0 feature line. Discovery topic builder MQTTHaDiscovery.cpp:2329 flipped from nested children format (homeassistant/sensor/<id>/<label>/<src>/config — rejected by HA's TOPIC_MATCHER regex) to sibling-suffix (homeassistant/sensor/<id>/<label>_<src>/config — accepted).

Why
HA's discovery dispatcher (homeassistant/components/mqtt/discovery.py:TOPIC_MATCHER) restricts object_id to [a-zA-Z0-9_-]+ (no forward slash). The previous nested format was silently discarded by HA with "illegal discovery topic" warning. Beta-channel users on 2.0.0-alpha.5 with bSeparateSources=true never saw source-variant entities register in HA. Same root cause and fix as dev ADR-071 / TASK-556.

Changes
- src/OTGW-firmware/MQTTHaDiscovery.cpp:2329: snprintf_P format string flipped to "%s/sensor/%s/%s_%s/config" for source-variant branch. Canonical branch unchanged.
- src/OTGW-firmware/MQTTHaDiscovery.cpp:2317-2326: comment block above buildSensorDiscoveryTopic added, referencing ADR-098 supersession of ADR-097 carve-out and noting both ESP8266 + ESP32-S3 builds affected.
- docs/adr/ADR-097-mqtt-source-topic-sibling-suffix-shape.md: Status line updated to "Superseded by ADR-098, 2026-05-07" per immutability protocol; body unchanged.
- docs/adr/ADR-098-mqtt-discovery-topic-sibling-suffix-shape.md: new ADR (Accepted by user 2026-05-07) covering the supersession rationale, HA regex evidence, and Enforcement block guarding against the old format.
- src/OTGW-firmware/version.h, data/version.hash: build artifact bumps.

Tests
- ./build.sh --firmware (esp8266 default) exit 0 — Flash 80.3%, RAM 85.2%. Pre-existing SATweather warning unrelated.
- ./build.sh --firmware --target esp32 exit 0 — Flash 98.0%, RAM 31.9%. Pre-existing SATble volatile-++ and SimpleTelnet flush warnings unrelated.
- .build-venv/bin/python evaluate.py --quick: 59/2/0, 97.1% — identical to baseline (no regression).
- grep verification: zero remaining matches for the OLD nested format PSTR("%s/sensor/%s/%s/%s/config") — ADR-098 forbid_pattern would catch any future regression.

Coordinated with dev TASK-556 (commit 4d9b5b42 on dev). Both ports use the same shape decision; only the file path differs (dev mqtt_configuratie.cpp vs 2.0.0 MQTTHaDiscovery.cpp). The cross-tree workflow protocol added to both CLAUDE.md files (commits ff0cc35b dev / 229a32ae 2.0.0) was validated end-to-end on this work.

AC #9 (field-log re-validation on hardware) and AC #10 (any other unmet) remain unchecked: requires hardware deployment of beta with this change.

Push held locally awaiting explicit user approval per 2.0.0 feature-branch push policy.

AC #9 confirmed via the verification build run on 2026-05-07: ./build.sh produced both ESP8266 and ESP32-S3 binaries cleanly (exit 0) with no new warnings. Only AC #10 (field test bSeparateSources=true on a beta unit confirming HA registers source-variant entities) remains as a hardware-gated blocker.
<!-- SECTION:FINAL_SUMMARY:END -->
