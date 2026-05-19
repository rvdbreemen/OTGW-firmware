---
id: TASK-626
title: Fix MQTT proxy-answer routing (ADR-075) — dev line
status: To Do
assignee: []
created_date: '2026-05-19 16:11'
labels:
  - mqtt
  - routing
  - adr-075
  - proxy-answer
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-069 starved canonical and _boiler for OTGW-proxied MsgIDs (MaxTSet/57, no-boiler, boiler-unsupported) because it assumed every readable id has a boiler B frame. ADR-075 (Accepted 2026-05-19, supersedes ADR-069) adds a bAnswerOverride discriminator: proxy A -> _thermostat+_boiler+canonical; answer-override A -> _thermostat only (ADR-069 invariant preserved). Implements the 1.5.x/dev line per ADR-075 'Implementation primitives — 1.5.x / dev line'. Originating report: PR #565.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OpenthermData_t gains byte bAnswerOverride; default 0; set 1 only on a detected (B,A) answer-override A frame
- [ ] #2 Proxy A (no preceding B) publishes to _thermostat, _boiler AND canonical for proxy-answered ids (e.g. MaxTSet/57)
- [ ] #3 Answer-override A (preceded by B within 500ms) still publishes to _thermostat only; genuine B keeps _boiler/canonical (ADR-069 invariant preserved)
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
- [ ] #6 _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh in the same commit as the firmware change
- [ ] #7 Hardware validation with an active answer-override (TT=/CS= on a read id): _boiler retains the boiler B value, not the faked A
<!-- AC:END -->
