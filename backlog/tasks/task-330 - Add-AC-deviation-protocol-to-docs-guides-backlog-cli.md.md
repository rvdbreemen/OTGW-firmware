---
id: TASK-330
title: Add AC deviation protocol to docs/guides/backlog-cli.md
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 17:12'
updated_date: '2026-04-19 17:22'
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
- [x] #1 docs/guides/backlog-cli.md adds a 'AC Deviation Protocol' section
- [x] #2 Protocol: if you cannot meet an AC, append-notes must explain why + what you did instead + whether a follow-up task is needed, and must request user sign-off for deviations on binding-rule ACs
- [x] #3 Points to examples of correctly-deviated tasks (TASK-296, TASK-294, TASK-308) as prior art
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/guides/backlog-cli.md: new AC Deviation Protocol section (4 bullets + prior-art examples) under the existing Acceptance Criteria section. Covers recording the deviation, deciding whether to check the AC, requesting user sign-off for ADR-080 binding-rule deviations, and putting the deviation in the final summary so it surfaces in the PR description. Prior-art cites TASK-296, TASK-294, TASK-308 as correctly-deviated examples. Rule of thumb closes the section: if a reviewer can't tell a year later which ACs were literal vs negotiated, the documentation is not good enough.
<!-- SECTION:FINAL_SUMMARY:END -->
