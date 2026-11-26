---
id: TASK-3003
title: >-
  SAT: skip boiler writes (SH=, MM=, SW=, TP=) whose MsgID the gateway already
  knows as unsupported (gruijter, Intergas)
status: To Do
assignee: []
created_date: '2026-11-21T12:41:00Z'
updated_date: '2026-11-26T22:14:00Z'
labels:
  - sat
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
- [x] #1 Robert's decision recorded (keep entry 252, or skip known-unsupported writes, or only change how sh_rejected is shown)
- [ ] #2 If a behaviour change is chosen: Sergeant D's view recorded as an entry before the code changes
- [ ] #3 Host test on the real SAT command path red/green, and a check on gruijter's next capture
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-26 AC1 decision (Robert, interactive session, relayed by the coordinator): 'Alleen de weergave van sh_rejected aanpassen'. That is the third option of AC1: only change how sh_rejected is shown. Label needs-maintainer-ok removed.

2026-11-26 AC2 not needed: no SAT behaviour change (Robert 2026-11-26). SAT keeps sending SH= and the other boiler writes as entry 252 says, so there is no question for Sergeant D and no new entry. The AC text stays as written.

2026-11-26 owner: otgw-webui (SAT owner) plans it. Scope: sat/sh_rejected must no longer show a permanent problem for a MsgID the boiler always reports as unsupported (gruijter: 57W on Intergas). Read SATcontrol.ino:2172, 2213 and 3061 and every place where sh_rejected is set and published; check the HA and Homey consumers of that topic. Milestone 2.0.0 stays, size small. AC3 stays open: host test red/green on the real path plus a check on gruijter's next capture.
<!-- SECTION:NOTES:END -->
