---
id: TASK-1604
title: >-
  Outbound fetches off the loop task: weather fetch in a one-shot task, and the
  PIC refresh stops after a failed update check (TASK-1471 R1, R2)
status: To Do
assignee: []
created_date: '2026-11-04T15:53:00Z'
labels:
  - network
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 745000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1471 audit (docs/audits/2026-11-04-outbound-connections.md). R1: the SAT weather HTTP GET runs on the loop task (SATweather.ino:343-375); with internet blocked it stalls the loop 5 s or more per attempt (dropped SYN, 5 s connect default) and longer when DNS is blocked, at +300/360/480/720/1200 s after boot. R2: refreshpic() (OTGW-Core.ino ~6936-6946) continues with a GET when checkforupdatepic() returned an empty string after a failed HEAD (latest != version), adding up to ~20 s of blocked loop per click.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 R1: the weather fetch runs outside the loop task (one-shot FreeRTOS task like webhook.ino:336, or the async HTTP pattern) with a connect timeout <= 2 s; the loop never blocks on it (host/source test + bench max_loop_gap_ms with internet blocked equal to baseline)
- [ ] #2 R2: refreshpic() returns without a GET when the update check gave no version (host or source test red on old code)
- [ ] #3 Bench .63 with a scoped MikroTik drop rule for outbound internet: no loop-stall line from weather or PIC paths, no reboot, settings unchanged
<!-- AC:END -->
