---
id: TASK-328
title: Create docs/guides/pr-checklist.md and link from CLAUDE.md
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 17:11'
updated_date: '2026-04-19 17:20'
labels:
  - documentation
  - process
  - review-2026-04-18-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Triggered by 2026-04-18 review retrospective: every batch committed 'builds clean' but 'builds clean' was the lowest bar. Several fixes (CSRF, OTA Serial, weather fetch, gzip serving, a11y buttons) have runtime behaviour that only real hardware can validate. Without a checklist, those checks get skipped and regressions surface as user reports weeks later. docs/guides/pr-checklist.md captures the real pre-merge bar in one greppable file.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/guides/pr-checklist.md created with sections: build gate (ESP8266+ESP32 clean), lint gate (evaluate.py --quick), test gate (tests/test_*.py), hardware/browser smoke-tests keyed by change type (frontend, MQTT, auth, OTA)
- [x] #2 CLAUDE.md Doing tasks section links to pr-checklist.md with one-line rationale
- [x] #3 Referenced from Test Plan section in docs/guides/backlog-cli.md so firmware tasks flow into the checklist naturally
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/guides/pr-checklist.md created (114 lines, 6.3 KB) with the real pre-merge bar: baseline gates (build, evaluate.py, stdlib unittests), change-type gates (frontend/MQTT/auth/OTA/OT/build/settings/ADR), backlog hygiene, and a when-to-skip rationale so the checklist stays a gate not a ritual. CLAUDE.md Task Management section now points at pr-checklist.md right where contributors read the task workflow, so it is impossible to miss before marking Done. backlog-cli.md Test Plan section cross-references pr-checklist.md so task-level test plans feed naturally into the PR-level checklist.
<!-- SECTION:FINAL_SUMMARY:END -->
