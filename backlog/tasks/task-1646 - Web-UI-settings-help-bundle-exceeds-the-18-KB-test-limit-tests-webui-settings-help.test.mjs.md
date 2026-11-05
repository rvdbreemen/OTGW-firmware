---
id: TASK-1646
title: >-
  Web UI settings help bundle exceeds the 18 KB test limit
  (tests/webui/settings-help.test.mjs)
status: To Do
assignee: []
created_date: '2026-11-05T04:23:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 771000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1644 lane 2026-11-05: tests/webui/settings-help.test.mjs caps the help text at 18432 B; origin/dev is already 18682 B (red before 1644), with 1644 18814 B. Decide: trim help texts or raise the limit with a measured reason (first-load size).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Test green on dev with a recorded reason (trimmed texts or new limit with the measured bundle size)
<!-- AC:END -->
