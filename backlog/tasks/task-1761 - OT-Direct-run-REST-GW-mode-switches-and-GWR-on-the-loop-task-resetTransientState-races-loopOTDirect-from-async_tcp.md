---
id: TASK-1761
title: >-
  OT-Direct: run REST/GW= mode switches and GW=R on the loop task
  (resetTransientState races loopOTDirect from async_tcp)
status: To Do
assignee: []
created_date: '2026-11-09T18:51:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 858000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1752 review 2026-11-09 (code reading): POST /api/v2/otdirect/mode and POST of GW=x/GW=R go addCommandToQueue -> handleOTDirectCommand -> setOTDirectMode/resetTransientState on the async_tcp task (restAPI.ino:534-538, OTGW-Core.ino:3851-3856), while loopOTDirect runs on the loop task. TASK-1752 made the command ring safe (consumer-side flush), but the rest of resetTransientState (schedule table, overrides, write caches, otCurrentMode, otRemoteOverride) is still written unsynchronised from two tasks, and one already-peeked frame can still go out after the switch. Proposed: route these through the TASK-1702 pending-mode path (otdModeSettingPending) so the switch runs on the loop task behind !otMasterRequestActive; keep the GW: reply semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test shows a REST mode switch / GW=R is applied on the loop task only, after any in-flight master request completes (red on current code)
- [ ] #2 No frame peeked under the old mode is sent after the switch; existing OTD tests green
<!-- AC:END -->
