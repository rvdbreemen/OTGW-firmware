---
id: TASK-609
title: 'feat-2.0.0: triage pre-existing CI baseline failures on the alpha branch'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 07:26'
updated_date: '2026-05-26 09:19'
labels:
  - ci
  - tech-debt
  - 2.0.0
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 2.0.0 feature line (feature-dev-2.0.0-otgw32-esp32-sat-support, base 201b301b / alpha.34) has multiple red CI checks at baseline, surfaced by docs-only PR #582: 'Run evaluate.py --quick' (verified base Failed:1, Health 95.6%), 'Spec-driven OT v4.2 audit', 'pio run -e esp8266', and 'pio run -e esp32'. None are caused by documentation changes (a 14-line markdown link fix cannot affect firmware compilation or the OT spec audit). These baseline reds prevent CI-green merges on the 2.0.0 line and mask real regressions. Triage each, classify pre-existing-vs-in-progress, and fix or file scoped follow-ups. Work happens in the 2.0.0 worktree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each failing 2.0.0 check (evaluate.py --quick, Spec-driven OT v4.2 audit, pio run -e esp8266, pio run -e esp32) is triaged with documented root cause
- [x] #2 Each is either fixed, or has a tracked follow-up with explicit rationale if intentionally deferred for alpha
- [x] #3 A clean docs-only PR against the 2.0.0 branch shows no baseline reds, OR remaining reds are explicitly accepted-for-alpha and documented in the task
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Triage-only: all 4 baseline CI failures already resolved on HEAD. evaluate.py --quick: 0 failed, 98.6%; OT v4.2 audit workflow removed; pio esp8266+esp32: pass (lib_extra_dirs fix in platformio.ini via PR#605). No code changes needed.
<!-- SECTION:FINAL_SUMMARY:END -->
