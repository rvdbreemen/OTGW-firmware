---
id: TASK-1634
title: Move SAT research scripts out of scripts/
status: To Do
assignee: []
created_date: '2026-11-04T22:44:00Z'
labels:
  - process
  - agentic
  - sat
dependencies: []
priority: low
ordinal: 761000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F7. 45 of 84 files in scripts/ are sat_* studies and simulations (study 2..25, hp studies), next to operational tooling (discord-post-build.sh, ota_flash.py). Many are referenced from docs/sat/ (sim-hp-lowload/round*.md, verification plan, crosswalk, resolution index). Moving them is only worth it when every reference moves along.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Research scripts live under tools/research/sat/ (or next to their docs/sat/sim-* folder); operational SAT tools (sat_boiler_emulator.py, sat_log_to_csv.py, verify_*) stay
- [ ] #2 Every reference in docs/, tests/ and scripts/ is updated; grep for the old paths finds nothing
- [ ] #3 python scripts/run_tests.py --no-host passes
<!-- AC:END -->
