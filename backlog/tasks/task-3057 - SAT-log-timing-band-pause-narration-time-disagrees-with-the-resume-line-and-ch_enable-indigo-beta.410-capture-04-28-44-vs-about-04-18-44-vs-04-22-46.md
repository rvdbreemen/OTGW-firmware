---
id: TASK-3057
title: >-
  SAT log timing: band-pause narration time disagrees with the resume line and
  ch_enable (indigo beta.410 capture, 04:28:44 vs about 04:18:44 vs 04:22:46)
status: To Do
assignee: []
created_date: '2026-11-25T11:43:00Z'
labels:
  - bug
  - sat
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1158000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while analysing indigo_light's beta.410 capture (24 Nov 21:44 to 25 Nov 12:04, OTGW-field-data/indigo_light/2026-11-25_beta.410_capture/analysis/facts.md, section tolerance band). The band-pause narration 'Heating paused ... at least 0.1 above their target' is logged at 04:28:44, while the matching resume line 'Heating resumes ... CH was off N s' at 04:48:14 implies the pause began about 04:18:44, and the MQTT ch_enable topic went off at 04:22:46. Three clocks, three times: cause not found (log timing, narration queue delay, or a different event). No behaviour problem seen, no SAT behaviour question for Sergeant D. To do: reproduce on a band pause with the live log, compare the narration timestamp with the CH=0 send and ch_enable publish.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce the three times on a bench band pause and name which timestamp is wrong or delayed
- [ ] #2 Fix the timestamp source or document why they differ (host test or log line)
<!-- AC:END -->
