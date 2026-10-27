---
id: TASK-1411
title: >-
  SAT: publish low-load remediation, executable verification plan and
  control-algorithm comparison
status: Done
assignee:
  - '@codex'
created_date: '2026-10-27T23:06:00Z'
updated_date: '2026-10-27T23:28:00Z'
labels:
  - sat
  - review
  - verification
  - research
milestone: 2.0.0
dependencies: []
references:
  - >-
    D:/Users/Robert/Documents/GitHub/RvdB/OTGW-other-projects/OT-Spec/OT
    Application Functional Specification v1-1f.pdf
  - https://otgw.tclcode.com/background.html#custom
documentation:
  - docs/sat/open-questions.md
  - docs/sat/sergeant-d-corrections.md
  - docs/sat/sat-port-audit-2026-10-24.md
  - docs/sat/sat-low-load-remediation-plan-2026-10-28.md
  - docs/sat/sat-low-load-verification-plan-2026-10-28.md
  - docs/sat/sat-control-algorithm-research-2026-10-28.md
  - docs/sat/sat-low-temperature-control-comparison-2026-10-28.md
modified_files:
  - docs/sat/sat-low-load-remediation-plan-2026-10-28.md
  - docs/sat/sat-low-load-verification-plan-2026-10-28.md
  - docs/sat/sat-control-algorithm-research-2026-10-28.md
  - docs/sat/sat-low-temperature-control-comparison-2026-10-28.md
  - scripts/verify_sat_low_load.py
  - scripts/inspect_sat_reference.py
  - docs/sat/evidence/2026-10-28-low-load-baseline.json
  - docs/sat/evidence/2026-10-28-sat-reference.json
priority: high
type: docs
ordinal: 549000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 2026-10-27/28 review found low-load interoperability and timing gaps, inconsistent SAT decision/task records and insufficient separation of host/replay evidence from physical boiler or heat-pump validation. The user requests a detailed plan covering every session finding, a runnable test/verification plan, and primary-source online research comparing the pinned SAT control model with related boiler and low-temperature heat-pump strategies. This task delivers planning and research artifacts; it does not implement firmware policy changes or treat proposed decisions as approved.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every issue raised in the session maps to a remediation work package, an existing or proposed backlog task, dependencies, decision status and an objective acceptance test.
- [x] #2 The verification plan contains executable commands for available checks, distinguishes checks that must be added from existing checks, defines reproducible hardware/field procedures and measurable pass/fail gates, and records the current baseline without claiming physical validation.
- [x] #3 The research explains the pinned SAT algorithm from opened file:line sources and critically compares weather compensation, PI/PID/time-proportional control, minimum-output cycling and predictive heat-pump control using primary sources with explicit limits on comparative claims.
- [x] #4 The documents receive an independent consistency/evidence review; links, referenced commands and issue coverage are checked, and no existing SAT decision or firmware behaviour is silently changed.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Reconcile every session finding with current source, existing task status, SAT decision entries and governing/advisory ADRs.
2. Research the pinned SAT equations and compare primary-source boiler/TPI, weather-compensated, heat-pump-native and predictive approaches; distinguish evidence from hypotheses and preserve the pin.
3. Publish a complete issue-to-work-package remediation plan with dependencies, approval gates and measurable acceptance criteria; reuse existing tasks and label proposed follow-ups.
4. Publish a verification protocol and stdlib runner for existing suites plus real-code temporal contract probes; expose current known failures and unexecuted physical gates rather than treating baseline success as release acceptance.
5. Run the artifacts, check links and coverage, obtain an independent refuting review, record limitations and validate the documentation change under the project checklist. Commit only this task's files and push when finalized.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Delivered four linked documents, a 42-item issue register with existing/proposed task mapping and approval dependencies, primary-source algorithm/commercial/research comparison, a stdlib port verification runner and actual pinned-PWM trace adapter. Independent refuting reviews checked source math/ancestry, paper/manual scope, evidence budgets, all42 issue mappings and release semantics. Fixed automatic-gain wording, existing ADR-196 OT-authority scope, source-study temperature normalization, phase/CH and empty-observation oracle gaps, executing-runner provenance, relight minimum wording and gross-duty denominator. Preserved existing decision log, pin and firmware behaviour.

Verification: python build.py SUCCESS for default esp32-combo firmware + filesystem + safeboot (after installing the two already-pinned requirements-build.txt minifiers); generated version.h/data/version.hash restored to their previously clean tracked state. python evaluate.py --quick:78 passed,0 warnings,0 failed,7 informational. tests/test_evaluate.py passed; tests/test_build.py:13 passed. Documentation local links/fences and42 unique issue IDs checked.

Durable baseline docs/sat/evidence/2026-10-28-low-load-baseline.json:133 existing tests,0 ordinary failures/errors/skips,1 expected failure; LL-001..004 candidate contracts all fail current firmware; acceptance correctly blocked with23 blockers. Release contracts-only negative control exits1/blocked. Source and executing-runner SHA256 values independently rechecked. Reference adapter reproduces ultra-low OFF and relight traces from actual pinned PWM source; empty/wrong-CH and missing/wrong-pin negative controls reject. No device, boiler or heat pump was operated; physical acceptance remains unverified.

Fresh GitHub fixed-SHA comparisons in both directions prove recorded develop7d43560342ca9d854e92b8b5a91bdfbf9cbc2152 is ancestor294 commits behind release pin7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb. Corrected the planning/research interpretation without changing original historical entries. This task completes planning/research/tooling only; existing firmware remediation and human approvals remain open.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Published a complete SAT low-load remediation programme covering all42 session findings, with existing task mappings, decision/ADR boundaries, delivery dependencies and measurable verification contracts. Added detailed pinned-algorithm research and a primary-source comparison of weather compensation, TPI/PID, native heat-pump control and predictive supervision. Added stdlib real-source port/reference runners and durable blocked-baseline evidence. Full esp32-combo firmware/filesystem/safeboot build, quick evaluation, build/evaluation unit suites, link/coverage checks and independent reviews passed for this planning/tooling deliverable. Existing133 SAT tests retain1 expected failure;4 candidate contracts reproduce outstanding firmware defects and release acceptance remains blocked. No firmware behaviour, historical decision or upstream pin was changed; actual equipment/field validation remains required.
<!-- SECTION:FINAL_SUMMARY:END -->
