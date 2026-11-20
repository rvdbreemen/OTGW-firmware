---
id: TASK-2978
title: Formal model of the SAT control-mode state machine with a model checker
status: To Do
assignee:
  - '@otgw-architect'
created_date: '2026-11-20T16:30:00Z'
updated_date: '2026-11-20T16:34:00Z'
labels:
  - tests
  - sat
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1110000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test-pyramid gap analysis 2026-11-20 (Robert). Goal: check the SAT control-mode state machine (PWM, continuous, tolerance band, minimum CH-off; ADR-249, ADR-252, ADR-253) for unreachable states, missing transitions and timer interactions with a model checker, before field testers find them. Properties come from Sergeant D's recorded entries in docs/sat/sergeant-d-corrections.md (cited per property), not from the firmware (SAT ground-truth rules). Tool choice per the reuse rule: compare at least two open-source options on primary sources (for example TLA+ with TLC or Apalache, or SPIN/Promela) and record why. A deviation between the model's properties and the firmware becomes a Sergeant D question (docs/sat/open-questions.md), never a direct code change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tool choice recorded: at least two model checkers compared (licence, maintenance, how the timers are modelled), with the reason
- [ ] #2 Model of the control-mode machine plus properties, each property citing its Sergeant D entry number; committed under docs/sat/ or tools/research/
- [ ] #3 One model-checker run with its output; every violation or ambiguity is queued as a Sergeant D question, not changed in code
<!-- AC:END -->
