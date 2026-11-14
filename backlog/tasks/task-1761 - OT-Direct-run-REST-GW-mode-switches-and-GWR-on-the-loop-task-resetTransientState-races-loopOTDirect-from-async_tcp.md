---
id: TASK-1761
title: >-
  OT-Direct: run REST/GW= mode switches and GW=R on the loop task
  (resetTransientState races loopOTDirect from async_tcp)
status: To Do
assignee: []
created_date: '2026-11-09T18:51:00Z'
updated_date: '2026-11-14T23:28:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis 2026-11-15 (d3, not implemented). Deferring only GW=x / GW=R to the loop task (the TASK-1702 otdModeSettingPending path; setOTDirectMode already raises it through updateSetting('OTDmode'), and the loop compares with the running mode) trades the race for an ordering bug: commands after the GW= in the same burst (a REST/MQTT sequence, the boot otgwcommands such as 'PS=0;GW=1', a CS=/HW=/PS=1 right after GW=R) still run at once on async_tcp, and the later reset on the loop task wipes them. The PIC runs commands strictly in order. A correct fix moves every OT-Direct command from async_tcp to the loop task in arrival order (a small text ring drained by loopOTDirect, the reply synthesized there), which changes the command path that 95 is reworking (TASK-1859, PIC parsers). Also in scope then: GW=R's otMaster/otSlave restart with delay(100) runs on async_tcp today. Needs a design decision and coordination with 95 before code.
<!-- SECTION:NOTES:END -->
