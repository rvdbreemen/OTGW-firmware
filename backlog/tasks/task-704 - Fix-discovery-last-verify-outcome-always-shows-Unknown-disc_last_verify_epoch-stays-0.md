---
id: TASK-704
title: >-
  Fix: discovery last-verify outcome always shows Unknown
  (disc_last_verify_epoch stays 0)
status: To Do
assignee: []
created_date: '2026-05-25 20:34'
labels:
  - bug
  - mqtt
  - discovery
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans on #beta-testing (2026-05-25) running beta.21. The disc_last_verify_epoch MQTT stat always publishes 0 and disc_verify_runs stays 0, even after 3+ hours of runtime. The 'Discovery last verify outcome' in the WebUI always shows 'Unknown'. The verify routine appears to never trigger or never write back its result.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 disc_verify_runs increments correctly after a verify cycle completes
- [ ] #2 disc_last_verify_epoch publishes a non-zero value (Unix timestamp) after the first verify run
- [ ] #3 WebUI 'Discovery last verify outcome' shows a meaningful value (not Unknown) after firmware has been running for >30 minutes with MQTT connected
<!-- AC:END -->
