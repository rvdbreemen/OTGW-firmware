---
id: TASK-330
title: Add AC deviation protocol to docs/guides/backlog-cli.md
status: To Do
assignee: []
created_date: '2026-04-19 17:12'
labels:
  - documentation
  - process
  - review-2026-04-18-followup
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Triggered by the 2026-04-18 review retrospective. Multiple tasks (TASK-296 String FAIL to WARN, TASK-294 CSRF AC2 deferred to manual test, TASK-308 _parse_image_header helper deferred, TASK-304 gzip deferred then resumed) had to deviate from their ACs. Each deviation was documented via --append-notes and sometimes a follow-up task, but there is no written protocol for when that is acceptable. Without one, future contributors may silently ship partial Dones.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 docs/guides/backlog-cli.md adds a 'AC Deviation Protocol' section
- [ ] #2 Protocol: if you cannot meet an AC, append-notes must explain why + what you did instead + whether a follow-up task is needed, and must request user sign-off for deviations on binding-rule ACs
- [ ] #3 Points to examples of correctly-deviated tasks (TASK-296, TASK-294, TASK-308) as prior art
<!-- AC:END -->
