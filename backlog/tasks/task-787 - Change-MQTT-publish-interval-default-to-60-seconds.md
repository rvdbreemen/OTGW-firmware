---
id: TASK-787
title: Change MQTT publish interval default to 60 seconds
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 16:53'
updated_date: '2026-05-31 17:22'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Change the default MQTT publish interval to 60 seconds and update the user-facing documentation so the new default is visible for release-relevant notes and configuration guidance.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fresh/default configuration uses a 60 second MQTT publish interval.
- [x] #2 Documentation that describes MQTT publishing or defaults notes the 60 second default.
- [x] #3 Relevant validation is run and recorded in the task notes.
- [x] #4 During upgrade to release 1.6.1, an existing backend MQTT publish interval setting of 0 seconds is migrated to 60 seconds.
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev
Coding agent: Codex
Implementation: added backend MQTTonChangePublishing / bOnChangePublishing default true, changed the backend MQTT interval default to 60 seconds, persisted the new setting in settings.ini, and added readSettings migration that saves MQTTinterval=60 when on-change publishing is true and the loaded interval is 0. MQTT gate checks now use the backend flag so MQTTonChangePublishing=false preserves legacy every-message publishing.
Documentation: updated README, changelog, breaking-change notes, MQTT API docs, REST debug docs/OpenAPI, and ADR-006/ADR-052 to call out the 60 second default and v1.6.1 0-to-60 migration.
Validation: .\.venv\Scripts\python.exe build.py --no-color passed and built both firmware and LittleFS artifacts. .\.venv\Scripts\python.exe evaluate.py --quick --no-color passed 36/36 checks. git diff --check passed for task-owned files. Build cleanup logged a Windows .tmp access-denied warning after successful artifact generation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented backend MQTT on-change publishing defaults for v1.6.1.

Changes:
- Added persisted MQTTonChangePublishing / bOnChangePublishing with a backend default of true.
- Changed fresh backend MQTTinterval default to 60 seconds.
- Added settings-load migration: when MQTTonChangePublishing is true or missing and MQTTinterval loads as 0, readSettings updates the interval to 60 and immediately saves settings.ini.
- Kept legacy every-message publishing available through MQTTonChangePublishing=false; runtime MQTTinterval updates keep the backend flag aligned with 0/nonzero interval saves.
- Updated README, changelog, breaking-change notes, MQTT/API docs, OpenAPI debug schema/example, and ADR-006/ADR-052.

Validation:
- .\.venv\Scripts\python.exe build.py --no-color (firmware + LittleFS)
- .\.venv\Scripts\python.exe evaluate.py --quick --no-color
- git diff --check for task-owned files

Note: the combined build completed successfully but Windows could not remove one locked .tmp generated file during cleanup; artifacts were still produced.
<!-- SECTION:FINAL_SUMMARY:END -->
