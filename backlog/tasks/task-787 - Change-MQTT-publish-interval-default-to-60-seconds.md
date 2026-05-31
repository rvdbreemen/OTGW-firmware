---
id: TASK-787
title: Change MQTT publish interval default to 60 seconds
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 16:53'
updated_date: '2026-05-31 16:54'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Change the default MQTT publish interval to 60 seconds and update the user-facing documentation so the new default is visible for release-relevant notes and configuration guidance.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Fresh/default configuration uses a 60 second MQTT publish interval.
- [ ] #2 Documentation that describes MQTT publishing or defaults notes the 60 second default.
- [ ] #3 Relevant validation is run and recorded in the task notes.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the MQTT publish interval default and all user-facing documentation that states or implies the default.
2. Change the fresh/default configuration value to 60 seconds and update the relevant docs/release guidance to call out the new default.
3. Run focused validation for the changed assets, then record the evidence in TASK-787.
4. Check the acceptance criteria, add a final summary, mark TASK-787 Done if complete, and commit/push only the task-owned files.
<!-- SECTION:PLAN:END -->
