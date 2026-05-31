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
- [ ] #4 During upgrade to release 1.6.1, an existing backend MQTT publish interval setting of 0 seconds is migrated to 60 seconds.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the backend MQTT publish interval default, persisted settings load/upgrade path, and all user-facing documentation that states or implies the default.
2. Change the backend fresh/default configuration value to 60 seconds.
3. Add release-1.6.1 migration behavior so an existing persisted value of 0 seconds is updated to 60 seconds during settings upgrade, without altering other explicit nonzero values.
4. Update relevant documentation/release guidance to call out the 60 second default and the 0-to-60 upgrade behavior.
5. Run focused validation for settings/default behavior and documentation consistency, then record the evidence in TASK-787.
6. Check the acceptance criteria, add a final summary, mark TASK-787 Done if complete, and commit/push only the task-owned files.
<!-- SECTION:PLAN:END -->
