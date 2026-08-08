---
id: TASK-1073
title: 'Fix: /api/v2/simulate reports active on boards where replay cannot run'
status: To Do
assignee: []
created_date: '2026-08-08 19:12'
labels:
  - bug
  - api
  - otdirect
dependencies: []
priority: medium
ordinal: 260000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
setOTGWSimulationEnabled() (restAPI.ino:345) flips state.debug.bOTGWSimulation with no capability check, and sendSimulationStatus() (restAPI.ino:333) reports active straight from that flag. The replay pump itself sits behind '#if HAS_PIC' and behind 'if (isOTDirectEnabled()) return;' at OTGW-Core.ino:5069, so on a PIC-less OTGW32 build, or on a combo running in OT-Direct mode, POST /api/v2/simulate/start returns active:true while not one fixture frame is ever replayed. Verified on the bench S3 2026-08-08: fixture readable at /otgw_simulation.log, endpoint reported active, 400s telnet capture contained zero processOT lines. The failure is silent and looks exactly like success, which is how it cost a full capture cycle to diagnose. This is the reporting half of TASK-1071 and does not depend on the replay work landing: refusing the request, or reporting why replay is inert, is a few lines and stops the API lying today.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 POST /api/v2/simulate/start on a board where the replay pump cannot run does not report active:true
- [ ] #2 The response says why replay is unavailable (no PIC path on this build, or the board is in OT-Direct mode), not a bare error
- [ ] #3 GET /api/v2/simulate reflects the same reality: it never reports active on a board where nothing is replayed
- [ ] #4 Behaviour on a PIC board in PIC mode is unchanged: start still enables replay and reports active
- [ ] #5 Build green for the esp32 targets and python evaluate.py --quick shows no new failures
<!-- AC:END -->
