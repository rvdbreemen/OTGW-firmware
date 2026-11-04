---
id: TASK-125
title: 'SAT Audit E4: ADR for OPV calibration approach'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:55'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-5
  - adr
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR documenting the OPV calibration method: measurement procedure, calculation formula, storage and application. Document why this approach was chosen over alternatives.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR describes context, alternative calibration methods and rationale
- [x] #2 ADR published in backlog/decisions/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created docs/adr/ADR-070-sat-opv-calibration.md documenting:
- What OPV measures: max flow temp at minimum modulation (MM=0)
- Why active calibration is needed (no OT protocol field for this)
- Alternatives rejected: fixed floor, MaxCapacity proxy, manual entry, continuous learning
- State machine: STARTING -> WARMING (3min) -> MEASURING (20min, sample every 10s) -> DONE/FAILED
- Recovery: CS=0 + MM=100 on failure
- Storage in settings.sat.fOvpValue (persistent, LittleFS)
<!-- SECTION:FINAL_SUMMARY:END -->
