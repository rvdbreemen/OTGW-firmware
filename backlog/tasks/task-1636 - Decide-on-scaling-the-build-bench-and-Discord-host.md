---
id: TASK-1636
title: Decide on scaling the build, bench and Discord host
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:46:00Z'
updated_date: '2026-11-05T04:33:00Z'
labels:
  - process
  - agentic
dependencies:
  - TASK-1633
priority: low
ordinal: 763000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Scenario C of docs/process/agentic-setup.md. Options: a second bench unit, a self-hosted build runner so the build lock does not hang on one workstation, and an always-on small host for discord-mcp and the webhook. Only worth it when the TASK-1633 figures show the bench or the machine is the real bottleneck.
Decision task: present the options with costs and the measured figures to Robert (AskUserQuestion); create implementation tasks for what he picks.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Follow-up tasks exist for each chosen option, or the task is closed as 'not now' with the reason
- [x] #2 Robert decided, with the TASK-1633 figures in front of him; decision recorded in the notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. Options put to Robert (AskUserQuestion) with the TASK-1633 figures: not now / always-on Discord host / second bench or build runner. Decision: no scaling now. Reason: the bench and build path produced 8.7 prerelease tags per day (peak 38) and Sergeant D answers 80% of timed questions within 2.4 h; the wait sits in the open task stock. Follow-up created: TASK-1647 (measure waits for maintainer decisions and field evidence).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decided by Robert on 2026-11-05 with the TASK-1633 figures in front of him: no second bench, no build runner and no always-on Discord host now, because neither the bench/build path (8.7 tags per day) nor Sergeant D (80% of timed answers within 2.4 h) is the bottleneck. AC1: decision and figures in the notes. AC2: closed as not now; follow-up TASK-1647 measures the waits that are not measured yet (maintainer decisions, field evidence).
<!-- SECTION:FINAL_SUMMARY:END -->
