---
id: TASK-1590
title: >-
  SATTICK: show the return source (OT MsgID 28 vs PIC board sensor) next to
  rc/rage
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T13:40:00Z'
updated_date: '2026-11-03T13:52:00Z'
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
- [ ] #1 SATTICK shows the return source; host test on the real SATTICK format (line fits the 320 B debug buffer), docs/sat/sat-log-format.md and scripts/sat_log_to_csv.py updated
<!-- AC:END -->
