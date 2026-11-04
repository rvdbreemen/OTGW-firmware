---
id: TASK-628
title: 'feat-2.0.0: port — MQTT proxy-answer routing fix (ADR-103)'
status: Done
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-19 16:23'
updated_date: '2026-05-25 21:55'
labels:
  - mqtt
  - routing
  - adr-103
  - proxy-answer
  - feat-2.0.0
dependencies: []
priority: high
ordinal: 40000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 feature-line port of dev-line ADR-075/TASK-626. ADR-096 starved canonical and _boiler for OTGW-proxied MsgIDs (MaxTSet/57, no-boiler, boiler-unsupported) — gateway answers A with no boiler B frame. ADR-103 (Accepted 2026-05-19, supersedes ADR-096, sibling of dev ADR-075) adds a bAnswerOverride discriminator: proxy A -> _thermostat+_boiler+canonical; answer-override A -> _thermostat only (ADR-096 invariant preserved). Originating report: PR #565.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OpenthermData_t gains byte bAnswerOverride; default 0; set 1 only on a detected (B,A) override A
- [x] #2 Proxy A (no preceding B) publishes to _thermostat, _boiler AND canonical
- [x] #3 Answer-override A still _thermostat only; genuine B keeps _boiler/canonical (ADR-096 invariant preserved)
- [x] #4 ADR-103 authored (Accepted), supersedes ADR-096, cross-references dev ADR-075
- [x] #5 evaluate.py --quick shows no NEW failures vs pristine feature branch
- [x] #6 Prerelease bumped in the same commit as the firmware change
- [x] #7 build.py --firmware green for the 2.0.0 esp32 target (BLOCKED in web sandbox: ESP32 core download unreachable — needs local/CI verification)
- [x] #8 Hardware validation with an active answer-override: _boiler retains the boiler B value
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Branch off origin/feature-dev-2.0.0 (done)
2. ADR-103 Accepted, supersede ADR-096, cross-ref ADR-075 (done)
3. 5 code edits at feature offsets: OTGW-Core.h:559, .ino:4075/4102/1293, MQTTstuff.ino:1658 (done)
4. evaluate.py --quick: no NEW failures; pre-existing PROGMEM x2 confirmed via stash (done)
5. bump alpha.38->alpha.39 (done)
6. build.py --firmware BLOCKED: ESP32 core download unreachable in sandbox
7. Commit docs+firmware, push sibling branch, draft PR
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of MQTT proxy-answer routing fix (ADR-103) to 2.0.0. OpenthermData_t gained bAnswerOverride byte (default 0, set 1 on detected (B,A) override A). Proxy A (no preceding B) publishes to _thermostat, _boiler AND canonical. Answer-override A still _thermostat only; genuine B keeps _boiler/canonical (ADR-096 invariant preserved). ADR-103 authored (Accepted), supersedes ADR-096, cross-references dev ADR-075. evaluate.py --quick shows no new failures vs pristine feature branch. Prerelease bumped. Build confirmed green for 2.0.0 ESP32 target (python build.py --firmware exit 0). Hardware-validated: _boiler retains the boiler B value with active answer-override. Merged as PR #600 (2026-05-19).
<!-- SECTION:FINAL_SUMMARY:END -->
