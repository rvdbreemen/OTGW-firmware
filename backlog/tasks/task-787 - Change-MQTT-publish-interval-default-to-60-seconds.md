---
id: TASK-787
title: Change MQTT publish interval default to 60 seconds
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 16:53'
updated_date: '2026-05-31 17:03'
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
1. Add a backend MQTT boolean setting named mqttonchangepublishing / MQTTonChangePublishing, defaulting to true in the settings object and persisted in settings.ini.
2. Parse the new setting from settings.ini so an explicit false value overrides the default, while a missing field remains true for upgrades.
3. After settings load, if on-change publishing is true and MQTTinterval is 0, migrate MQTTinterval to 60 and immediately write settings.ini so the upgrade is durable.
4. Keep the change backend-owned; do not depend on the synthetic UI checkbox for migration behavior.
5. Update backend/API documentation and release guidance to note the 60 second default and the 0-to-60 upgrade behavior.
6. Run focused validation for settings/default behavior and documentation consistency, then record the evidence in TASK-787.
7. Check acceptance criteria, add a final summary, mark TASK-787 Done if complete, and commit/push only task-owned files.
<!-- SECTION:PLAN:END -->
