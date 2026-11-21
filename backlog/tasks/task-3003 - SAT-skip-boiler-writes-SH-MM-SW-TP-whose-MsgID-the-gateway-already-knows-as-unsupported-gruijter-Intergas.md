---
id: TASK-3003
title: >-
  SAT: skip boiler writes (SH=, MM=, SW=, TP=) whose MsgID the gateway already
  knows as unsupported (gruijter, Intergas)
status: To Do
assignee: []
created_date: '2026-11-21T12:41:00Z'
labels:
  - sat
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1127000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing 2026-11-21 (msg 1573678885206364363), beta.408, Intergas + PIC: unsupported_msgids = 15R,28R,49R,57R,99R,117R,118R,119R,121R,122R,14W,57W. SAT still sends SH= (57W) after each restart and max-setpoint change, the boiler refuses, sat/sh_rejected stays true (a permanent SAT problem in HA/Homey); MM= (14W) silently does nothing. SAT already checks isBoilerMsgIdUnsupportedRead(25/28) (SATcontrol.ino:2172, 2213, 3061) but none of its writes check the unsupported list. His proposal: one helper for SAT's boiler commands that skips a known-unsupported MsgID (log once), keep sh_rejected false then, maybe publish sat/skipped_cmds. CONFLICT: entry 252 (Robert, 2026-11-03): a rejected SH= is observed and published and SAT keeps sending it by the normal rules. Skipping writes changes how SAT controls the boiler, so it needs Robert's release and, for the control part, Sergeant D's view (sat-ground-truth rules).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's decision recorded (keep entry 252, or skip known-unsupported writes, or only change how sh_rejected is shown)
- [ ] #2 If a behaviour change is chosen: Sergeant D's view recorded as an entry before the code changes
- [ ] #3 Host test on the real SAT command path red/green, and a check on gruijter's next capture
<!-- AC:END -->
