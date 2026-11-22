---
id: TASK-3036
title: >-
  SAT: hand control back to the thermostat when the boiler answers ID25 (flow)
  Data-Invalid
status: To Do
assignee: []
created_date: '2026-11-22T16:43:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1144000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source: entry 471 in the private SAT register (Sergeant D, Q-470a answer 1: an ID25 Data-Invalid stops SAT and hands back to the thermostat; 'a flow-sensor fault stops the boiler anyway'; #dev-sat-mqtt message 1574102255043477646); amends entry 470. Cite the entry by number. No code until the token budget hold ends (Wed 25 Nov).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A Data-Invalid answer to ID25 (flow temperature) puts SAT in hand-back, with a narrate line and an MQTT status; an unanswered or stale ID25 does the same after a timeout that is confirmed with Sergeant D before coding
- [ ] #2 SAT resumes control when a valid ID25 returns
- [ ] #3 Host test on the real code: red with a Data-Invalid ID25 reply on the old code, green on the fix
- [ ] #4 Bench on .63 with the SAT simulator
- [ ] #5 Entry 471 is cited in the code comment, the task and the commit; an ADR (via adr-kit) if it changes the SAT fail-safe contract
<!-- AC:END -->
