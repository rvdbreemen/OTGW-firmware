---
id: TASK-626
title: Fix MQTT proxy-answer routing (ADR-075) — dev line
status: In Progress
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-19 16:11'
updated_date: '2026-05-19 16:11'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Accept ADR-075, supersede ADR-069 (done in prior commit)
2. OTGW-Core.h: add byte bAnswerOverride after bGatewaySubstituted
3. OTGW-Core.ino ~4071: init OTdata.bAnswerOverride=false
4. OTGW-Core.ino ~4099: delayedOTdata.bAnswerOverride = bGatewaySubstituted (propagate to promoted A)
5. OTGW-Core.ino:1276: gate canonical block on && OT.bAnswerOverride
6. MQTTstuff.ino A-case: toBoiler = !OTdata.bAnswerOverride; update routing comment block
7. build.py --firmware exit 0; evaluate.py --quick no new failures
8. bin/bump-prerelease.sh; stage version.h + data/version.hash
9. Commit + push claude/review-pr-565-analysis-eFPJE
<!-- SECTION:PLAN:END -->
