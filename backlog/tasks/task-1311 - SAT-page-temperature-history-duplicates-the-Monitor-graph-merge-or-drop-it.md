---
id: TASK-1311
title: 'SAT page temperature history duplicates the Monitor graph: merge or drop it'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T07:52:00Z'
updated_date: '2026-10-23T14:51:00Z'
labels:
  - webui
  - sat
  - graph
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 434000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 07:47 (#alpha-testing): the SAT 'Temperature history' chart doubles the Monitor graph; is there a reason to keep it? Facts: the SAT chart (v2.js satHistPush/satDrawHistory, client-side since page load, TASK-986) plots Setpoint (SAT target), Flow, Room, Outside and the PID output; the Monitor graph (TASK-1226/1296) has flow, return, control setpoint, thermostat setpoint, room, outside, modulation and flame/DHW/CH plus device-side history, but not the SAT target or the PID output. Options: (a) drop the SAT chart and add 'SAT target' and 'PID output' series to the Monitor graph (shown when SAT is enabled), with a link from the SAT page; (b) keep a small SAT chart that reuses the Monitor graph renderer and its history. Decide, then implement.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision (merge into the Monitor graph, or keep a reduced SAT chart on the shared renderer) recorded in the task with the reason
- [x] #2 No information is lost: SAT target and PID output are visible in the remaining graph when SAT is enabled (screenshot)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
v2.html: SAT history card title 'Temperature history' -> 'SAT control loop' + hint 'SAT target and PID output since this page was opened. Full boiler history: Monitor > Graph' with the link (button styled as a text link, existing class). v2.js: click -> showPage('monitor',{tab:'mgraph'}). Test: tests/webui/sat-page-order.test.mjs (or a new small test): title text, link opens Monitor with Graph active and does not store montab; legend still lists Setpoint/Flow/Room/Outside/PID. Screenshot after. No build-time risk beyond data assets; build + fs flash + device check.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23: depends on the navigation concept decision in TASK-1312 (maintainer); skipped by the cycle until that decision is recorded, to avoid moving the SAT controls twice.

2026-10-23: navigation concept decided in TASK-1312 (option A: top tabs everywhere, collapsible grouped sections inside a page); this task is unblocked and follows that concept.

2026-10-23 decision (cycle, delegated by the task text 'Decide, then implement'): keep the SAT chart, make its role explicit. Reasons: (a) merging needs the Monitor page to poll sat/status (extra API load on a page that does not need it today) and SAT target/PID are not in the device history ring (ADR-193, 14-byte slots), so after a reload those two series would be empty next to the others unless the firmware ring and API change; (b) sharing the renderer is a refactor with no user-visible gain now (YAGNI). The SAT chart shows what the Monitor graph does not (SAT target, PID output) and sits on the Thermostat view since TASK-1310. Change: title 'SAT control loop' with a one-line hint and a link that opens Monitor > Graph (showPage('monitor',{tab:'mgraph'}), not stored, TASK-1312 rule 2) for the full boiler history. No series removed (AC2).

2026-10-23 16:55 evidence: tests/webui/sat-page-order.test.mjs (f): fix 19 PASS / 0 FAIL incl. title 'SAT control loop', legend keeps Setpoint/Flow/Room/Outside/PID, link opens Monitor with Graph active, montab not stored; HEAD (full data dir copy): FAIL title ('Temperature history live'), FAIL no link, legend PASS on both (no series lost). Regression v2-navigation 50, sat-status-pill 14, evaluate 0 fail. build.bat SUCCESS (littlefs 16:49), bench fs flash, settings 178/178 differing 0, device serves the new title/link. Screenshots docs/evidence/task-1311/ (390 px + 1280).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decision (AC1): keep the SAT chart as 'SAT control loop' with a link to Monitor > Graph instead of merging (merging would make Monitor poll sat/status and the SAT target/PID are not in the device history ring, ADR-193; a shared renderer is a refactor without user gain now). AC2: all five series stay (legend check, screenshot). Evidence: browser test red on HEAD for title/link, green on the fix; device check; screenshots in docs/evidence/task-1311/. Not verified: a real phone.
<!-- SECTION:FINAL_SUMMARY:END -->
