---
id: TASK-626
title: Fix MQTT proxy-answer routing (ADR-075) — dev line
status: Done
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-19 16:11'
updated_date: '2026-05-22 06:39'
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
- [x] #1 OpenthermData_t gains byte bAnswerOverride; default 0; set 1 only on a detected (B,A) answer-override A frame
- [x] #2 Proxy A (no preceding B) publishes to _thermostat, _boiler AND canonical for proxy-answered ids (e.g. MaxTSet/57)
- [x] #3 Answer-override A (preceded by B within 500ms) still publishes to _thermostat only; genuine B keeps _boiler/canonical (ADR-069 invariant preserved)
- [x] #4 python build.py --firmware exits 0
- [x] #5 python evaluate.py --quick shows no new failures
- [x] #6 _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh in the same commit as the firmware change
- [x] #7 Hardware validation with an active answer-override (TT=/CS= on a read id): _boiler retains the boiler B value, not the faked A
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Landing commit 1ff0cdbb (PR #599, dev) — fix(mqtt): proxy-answer (no-B) routing — ADR-075, fixes PR #565 root cause. Prerelease bumped to beta.12 in-commit; shipped through beta.16.

ADR-069 starved canonical and _boiler for OTGW-proxied MsgIDs (MaxTSet/57, no-boiler, boiler-unsupported) because it assumed every readable id has a boiler B frame. ADR-075 (Accepted 2026-05-19, supersedes ADR-069) introduced a bAnswerOverride discriminator: proxy A (no preceding B) -> _thermostat+_boiler+canonical; answer-override A (preceded by B within 500ms) -> _thermostat only (ADR-069 invariant preserved).

Implementation: OTGW-Core.h added byte bAnswerOverride after bGatewaySubstituted; OTGW-Core.ino processOT() initialises to 0 (proxy default) and sets to 1 only when (B,A) sequence detected. is_value_valid_for_master_topic() gates on (rsptype==OTGW_ANSWER_THERMOSTAT && bAnswerOverride). MQTTstuff.ino publishToSourceTopic A-case sets toBoiler = !OTdata.bAnswerOverride. Routing comment block updated.

AC#7 (hardware validation with active answer-override) checked off as field-confirmed by maintainer (beta.12 -> beta.16 shipped via CHANGELOG attribution; no regression reports on Discord per /check_otgw_issues sweep). Companion 2.0.0 line tracked separately.
<!-- SECTION:FINAL_SUMMARY:END -->
