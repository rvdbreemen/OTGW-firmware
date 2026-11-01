---
id: TASK-1393
title: >-
  Command queue: dedup by 2-letter prefix merges different TP= reads; idea:
  write commands priority over periodic reads (maarten_41762)
status: In Progress
assignee: []
created_date: '2026-10-25T15:21:00Z'
updated_date: '2026-11-01T22:30:00Z'
labels:
  - mqtt
  - pic
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 532000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563931557000126665, ground-source heat pump): he polls TSP 6 and 7 with TP= every minute over MQTT and sends HW=0/1 at most once a day; worried a read could overwrite a pending write; idea: an option giving MQTT write commands priority over reads for ~60 s. He asked not to act yet. Code (HEAD): addCommandToQueue dedups on the first two letters (OTGW-Core.ino:3751-3757), so HW= and TP= use separate slots and a TP= cannot overwrite a pending HW=; but TP=6 and TP=7 sent close together share one slot and the second replaces the first if it was not sent yet; a full queue (CMDQUEUE_MAX) drops a new command, which could drop an HW=. PIC side (whether a TP read in progress delays or aborts other commands) is a question for Schelte Bron.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides whether to act (label needs-maintainer-ok)
- [ ] #2 If acted on: different TP= indexes are not merged, and a write is never dropped for a read (host test red on HEAD)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.

Audit 2026-11-01: moved To Do -> In Progress. TP= merging is fixed under TASK-1439 (f3b1351e4): TP= is append-only (OTGW-Core.ino:3843-3845), host test D4 in test/host/test_cmd_queue_drop.cpp:161-166. Still open: a full queue still drops a new command, including an HW= write ('Reached max queue'), and write-before-read priority is not built.

Lead triage 2026-11-01 (Robert: lead decides): TP= half of AC#2 is covered by TASK-1439 (f3b1351e4) and TASK-1315 (PM=/TP= unique per value, beta.3 by session b3b450). Write-priority half split to TASK-1555. Close this task with the TASK-1315 evidence once beta.3 is on origin/dev; AC#1 (Robert decides whether to act) = decided: TP= via 1315, write priority as its own task.
<!-- SECTION:NOTES:END -->
