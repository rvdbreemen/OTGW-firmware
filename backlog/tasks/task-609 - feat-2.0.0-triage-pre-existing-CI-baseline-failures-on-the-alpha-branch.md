---
id: TASK-609
title: 'feat-2.0.0: triage pre-existing CI baseline failures on the alpha branch'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 07:26'
updated_date: '2026-05-26 09:15'
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
Triaged all four pre-existing CI baseline failures on the 2.0.0 feature line (baseline alpha.34 / PR #582, 2026-05-16). All four are already green on current HEAD (alpha.70+86f392d); no code change required.

Findings per check:

1. evaluate.py --quick — was Failed:1, Health 95.6% at alpha.34. Now 0 Failed, 1 warning (informational: ADR-062 instrumentation check skipped because mqtt_configuratie.cpp not present), Health 98.6%. Fixed by intervening 2.0.0 work since alpha.34. Verified locally and on PR #605 CI (most recent merged 2.0.0 PR).

2. Spec-driven OT v4.2 audit — workflow no longer exists in .github/workflows/ on the 2.0.0 branch. Removed in an earlier CI cleanup (the directory currently contains only build.yml, claude.yml, claude-code-review.yml, dependency-scan.yml, evaluate.yml, trigger-copilot-agent.yml). Check cannot be red because the job no longer runs.

3. pio run -e esp8266 — was failing with 'SimpleTelnet.h: No such file or directory' on PR #582. Root cause: vendored library not on PlatformIO lib path. Fixed: platformio.ini [env] block sets lib_extra_dirs = src/libraries, picking up src/libraries/SimpleTelnet/. PR #605 esp8266 pass (3m44s on esp32, 1m28s on esp8266).

4. pio run -e esp32 — same lib_extra_dirs fix covers both targets. PR #605 esp32 pass.

Local verification (2026-05-26, HEAD = alpha.70+86f392d):
- python build.py --firmware exit 0 (Arduino CLI build, project's standard gate)
- python evaluate.py --quick: 61 passed / 1 warning / 0 failed / 7 info, Health 98.6%
- gh pr checks 605 (last merged 2.0.0 PR): evaluate.py pass, pio esp8266 pass, pio esp32 pass, claude-review pass

No follow-ups deferred — all four baseline reds are demonstrably green on HEAD.
<!-- SECTION:FINAL_SUMMARY:END -->
