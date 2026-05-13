---
id: TASK-600
title: >-
  Remove broken OpenTherm v4.2 spec audit CI workflow (mqttha.cfg no longer in
  repo)
status: To Do
assignee: []
created_date: '2026-05-13 17:39'
labels:
  - ci
  - cleanup
dependencies: []
references:
  - .github/workflows/opentherm-v42-spec-audit.yml
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
`.github/workflows/opentherm-v42-spec-audit.yml` runs `python tools/opentherm_v42_spec_audit.py --check` on every PR to `main|dev|dev-*|copilot-*|release/**`. The audit script requires `src/OTGW-firmware/data/mqttha.cfg`, which was deleted by archived TASK-272 when MQTT autodiscovery moved from LittleFS to a PROGMEM index (CHANGELOG: "Legacy mqttha.cfg template archive pipeline ... streaming HA discovery from v1.4.x supersedes it"). Every PR run since then exits 2 with "missing required file: ...mqttha.cfg" — the gate is a no-op that produces a perpetual red status.

Action: delete the workflow YAML. The audit script (`tools/opentherm_v42_spec_audit.py`) stays in place — it is harmless to keep on disk and someone may revive it later when the data model is reinstated, but it should not be wired into CI in its current state.

Surfaced while watching PR #570 — the audit fired and failed on each of the PR's three commits, masking real CI signal.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Delete .github/workflows/opentherm-v42-spec-audit.yml from the repo.
- [ ] #2 Do not delete tools/opentherm_v42_spec_audit.py (kept for future revival).
- [ ] #3 Confirm no other workflow / branch-protection / README badge references this workflow file before deletion.
- [ ] #4 After push, the next CI run on the branch no longer schedules a 'Spec-driven OT v4.2 audit' check.
<!-- AC:END -->
