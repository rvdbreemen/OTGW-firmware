---
id: TASK-914
title: 'feat(webui): v2 Home concept C mission control + concept chips (TASK-908 P1c)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 04:34'
updated_date: '2026-06-24 05:38'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Extract design-c; renderC strip chart + ticker + cells; wire A/B/C chips via localStorage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Strip chart+ticker live; chips switch A/B/C; both builds green
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
renderC mission control: live strip chart (sample buffer -> flow/return/setpoint/mod), 8 metric cells, raw-frame ticker. Verified LIVE on OTGW32: CH SETPOINT 45.0 from real TSet, setpoint line at correct axis height, ticker scrolls live frames. Committed in 58afbeea (referenced dup TASK-913).
<!-- SECTION:FINAL_SUMMARY:END -->
