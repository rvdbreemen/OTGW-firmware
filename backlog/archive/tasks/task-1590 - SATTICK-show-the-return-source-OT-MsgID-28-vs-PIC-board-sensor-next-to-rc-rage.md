---
id: TASK-1590
title: >-
  SATTICK: show the return source (OT MsgID 28 vs PIC board sensor) next to
  rc/rage
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T13:40:00Z'
updated_date: '2026-11-03T14:00:00Z'
labels:
  - sat
  - logging
dependencies: []
priority: low
ordinal: 735000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the peer analysis of gruijter's beta.49 capture: SATTICK prints rc=U rage=4294967295 for the whole run because his boiler answers MsgID 28 Unknown-DataID; the return comes from the PIC board sensor (PR=E, picBoardTretValid) and satReturnFresh() is true, but the log shows only the OT feed class (TASK-1412.5). Add a small source field (e.g. rsrc=OT|BRD|-) so the log is not misleading; no control change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SATTICK shows the return source; host test on the real SATTICK format (line fits the 320 B debug buffer), docs/sat/sat-log-format.md and scripts/sat_log_to_csv.py updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead): satReturnSourceText() (SATcontrol.ino) = OT when the MsgID 28 feed is fresh, BRD when picBoardTretValid(), else '-', same order as satReturnFresh(); rsrc= after rage in SATTICK and the dhw=1 line. Worst case 293 of 319 B. Tests: flow_return_validity 22 (old 3 FAIL), log_to_csv 8 (old 3 red), no_return_ignition 19, sim_boiler_model 8; evaluate 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SATTICK shows the return source (rsrc=OT|BRD|-), so a boiler that answers MsgID 28 Unknown-DataID with a working board sensor no longer looks like a missing return in the log. Host tests red on the old code, green on the fix; logging only.
<!-- SECTION:FINAL_SUMMARY:END -->
