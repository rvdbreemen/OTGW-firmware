---
id: TASK-452
title: >-
  Investigate SAT scaffolding flagged as unused: satOvpCalibrate,
  satGetHeatingSystemName, thermal/PWM state
status: To Do
assignee: []
created_date: '2026-04-27 19:21'
labels:
  - sat
  - investigation
  - warnings
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Context

ESP8266 build flags four SAT-related symbols as "defined but not used" / "unused variable":

- `SATcontrol.ino:221` — `static const char* satGetHeatingSystemName()`
- `SATcontrol.ino:242` — `static uint32_t _thermal_learningStartMs = 0;` (Task #21 thermal-drop learning)
- `SATcontrol.ino:254` — `static void satOvpCalibrate()` (full state machine, ~80 lines)
- `SATcontrol.ino:566` — `static uint32_t _pwm_lastSampleMs = 0;` (PWM control mode)

These are not orphan helpers: ADR-076, doc-2 (sat-opv-calibration), feature-completeness matrix and sat-python-cpp-mapping all list `satOvpCalibrate` as the implementation of a documented feature. The compiler's "unused" verdict means the call-site is missing or behind a never-true condition.

## Goals

1. **Per symbol**: locate the *intended* call-site in the SAT control loop / REST/MQTT command path / discovery flow.
2. Decide outcome per item:
   - **Wire up** — feature is real, missing call should be added (links back to feature task).
   - **Mark unused** — feature is parked; annotate with `[[maybe_unused]]` and a `// TASK-NNN: parked, see ADR-XXX` comment so the warning stays silent without losing the code.
   - **Delete** — feature is superseded; link to the superseding ADR/task in the commit message.
3. Coordinate with any open SAT tasks (Task #21 thermal learning, ADR-076 OPV calibration) before touching shared state.

## Acceptance Criteria
<!-- AC:BEGIN -->
See AC list. Decision per symbol must be documented in the task notes with file:line reference and rationale.

## References

- `docs/adr/ADR-076-sat-opv-calibration.md`
- `backlog/docs/sat-feature-completeness-matrix.md`
- `backlog/docs/sat-python-cpp-mapping.md`
- `backlog/tasks/task-5 - Overshoot-Protection-Value-OPV-calibration.md`
- `backlog/tasks/task-97 - SAT-Audit-B6-OPV-calibration-algorithm.md`
- `backlog/tasks/task-224 - SAT-Add-minimum-OPV-calibration-dataset-requirement.md`
<!-- SECTION:DESCRIPTION:END -->

- [ ] #1 Located intended call-site (or absence) for satOvpCalibrate; outcome decided (wire up / park / delete) with file:line citation
- [ ] #2 Located intended call-site (or absence) for satGetHeatingSystemName; outcome decided
- [ ] #3 Located intended call-site (or absence) for _thermal_learningStartMs; outcome decided
- [ ] #4 Located intended call-site (or absence) for _pwm_lastSampleMs; outcome decided
- [ ] #5 Each parked symbol has [[maybe_unused]] or equivalent + comment pointing to owning task/ADR
- [ ] #6 Each wired-up symbol verified by tracing call from setup() / loop / command handler down to the symbol
- [ ] #7 ESP8266 build clean for these four symbols
- [ ] #8 ESP32 build clean for these four symbols
- [ ] #9 Decision per symbol logged in task notes for future reference
<!-- AC:END -->
