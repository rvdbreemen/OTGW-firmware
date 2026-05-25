---
id: TASK-623
title: >-
  feat-2.0.0: ADR-102 CI gate — forbid OT-bus liveness write to base namespace
  topic
status: Done
assignee:
  - '@claude'
created_date: '2026-05-18 05:01'
updated_date: '2026-05-25 22:28'
labels:
  - adr
  - mqtt
  - tech-debt
  - 2.0.0
dependencies: []
ordinal: 39000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-102 declares a binding code-level pattern (forbid_pattern regex 'sendMQTT(MQTTPubNamespace, CONLINEOFFLINE...') but the 2.0.0 branch has no bin/adr-judge, so the declarative Enforcement block is not mechanically enforced by repo tooling. Per ADR-080, a binding pattern-level ADR must ship an evaluate.py/test gate or carry a 'Gate pending' note. This task closes the gate gap: add an evaluate.py check (or tests/test_*.py) that flags reintroduction of the base-topic OT-bus liveness write, then replace the pending-gate note in ADR-102 Consequences with a link to the check. Sibling of dev ADR-074 / PR #583 (same gap on dev).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 evaluate.py (or tests/test_*.py) flags 'sendMQTT(MQTTPubNamespace, CONLINEOFFLINE' under src/OTGW-firmware/** as a failure
- [x] #2 Check is registered in the always-run quick pipeline (evaluate.py --quick)
- [x] #3 ADR-102 Consequences 'Gate pending' note replaced with a link to the concrete check (ADR-080 gate exit criteria option 1)
- [x] #4 python evaluate.py --quick shows no new false positives from the new check
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added check_adr102_otbus_liveness_topic() to evaluate.py — flags sendMQTT(MQTTPubNamespace, CONLINEOFFLINE) in src/OTGW-firmware/** as FAIL. Registered in the always-run essential block (before check_adr_gates), runs in both --quick and full mode. Updated ADR-102 Consequences: replaced 'Gate pending — tracked as TASK-623' note with a reference to the concrete check (ADR-080 exit criteria met). python evaluate.py --quick: 0 failures, 0 false positives, health 98.6%.
<!-- SECTION:FINAL_SUMMARY:END -->
