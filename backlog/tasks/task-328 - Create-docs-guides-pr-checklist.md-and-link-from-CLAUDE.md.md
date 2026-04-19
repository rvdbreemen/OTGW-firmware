---
id: TASK-328
title: Create docs/guides/pr-checklist.md and link from CLAUDE.md
status: To Do
assignee: []
created_date: '2026-04-19 17:11'
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
- [ ] #1 docs/guides/pr-checklist.md created with sections: build gate (ESP8266+ESP32 clean), lint gate (evaluate.py --quick), test gate (tests/test_*.py), hardware/browser smoke-tests keyed by change type (frontend, MQTT, auth, OTA)
- [ ] #2 CLAUDE.md Doing tasks section links to pr-checklist.md with one-line rationale
- [ ] #3 Referenced from Test Plan section in docs/guides/backlog-cli.md so firmware tasks flow into the checklist naturally
<!-- AC:END -->
