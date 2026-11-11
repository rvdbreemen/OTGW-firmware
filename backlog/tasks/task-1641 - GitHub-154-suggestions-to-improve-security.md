---
id: TASK-1641
title: 'GitHub #154: suggestions to improve security'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T23:52:00Z'
updated_date: '2026-11-11T06:01:00Z'
labels:
  - github
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 767000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/154 by 0crap (2026-05). Security suggestions; triage against ADR-056 and the no-HTTPS rule. Needs Robert's go.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): released for triage: weigh each point against ADR-056 and the no-HTTPS trusted-LAN rule, answer on the issue per point, task only for points that fit those decisions.

2026-11-05: triage (sonnet agent, verified by lead in code: otgw1234 networkStuff.ino:302, telnet unconditional :640, u/i keys handleDebug.ino:425-434, 25238 default false MQTTstuff.h:73). Robert's decisions: telnet setting default on -> TASK-1644; opt-in no-portal setting -> TASK-1645; per-device AP passphrase: leave as is; post the reply. Reply posted: https://github.com/rvdbreemen/OTGW-firmware/issues/154#issuecomment-6649451164 (per point: web password and 25238 done, HTTPS/SSH not planned with reasons, points 1 and 4 on the backlog). Issue stays open until 1644/1645 ship.

2026-11-11 GitHub #154 closed as completed with Robert's OK: comment https://github.com/rvdbreemen/OTGW-firmware/issues/154#issuecomment-6787920665 (TASK-1644 telnet setting beta.94, TASK-1645 opt-in no-portal beta.96, both in the public beta).

Robert's OK, 2026-11-11 ~06:3x local, session otgw-firmware-09, AskUserQuestion 'May I close GitHub issues #694, #692 and #154 (...)?' answered verbatim: "Yes, close all three (Recommended)" (option text: 'Post the two comments, then close #694, #692 and #154 as completed. #10 stays open.').
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
GitHub #154 triaged per point against ADR-056/ADR-003/ADR-032 and the code; Robert decided per point; reply posted on the issue (comment 6649451164); follow-up tasks TASK-1644 (telnet setting) and TASK-1645 (opt-in no-portal). Not verified on hardware: nothing changed in firmware.
<!-- SECTION:FINAL_SUMMARY:END -->
