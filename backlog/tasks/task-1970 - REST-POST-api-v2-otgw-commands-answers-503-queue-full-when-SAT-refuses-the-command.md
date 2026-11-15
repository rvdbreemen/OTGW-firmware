---
id: TASK-1970
title: >-
  REST POST /api/v2/otgw/commands answers 503 queue full when SAT refuses the
  command
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T11:44:00Z'
updated_date: '2026-11-15T12:31:00Z'
labels:
  - rest
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .63 (beta.306+041400e, 2026-11-15, TASK-1943 bench): POST /api/v2/otgw/commands {"command":"CS=40"} with SAT enabled answers 503 'Command not queued (queue full)' while telnet shows 'satNoteExter(4459): CmdQueue: CS=40 refused, SAT is enabled ...'. restAPI.ino:373 maps every not-queued result to 'queue full', so a client cannot tell a SAT refusal (permanent while SAT controls) from a full queue (retry later).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A SAT refusal gets its own answer: 409 with a clear message (refused: SAT is in control); 503 stays only for a really full queue
- [x] #2 Host test on the real REST command path: SAT refusal -> 409, full queue -> 503, accepted -> as today; red on the old code, green on the fix
- [ ] #3 openapi.yaml and docs/api/README.md describe the 409; evaluate.py --quick no new FAIL; esp32-combo build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 fix: handleCommandSubmit() (restAPI.ino) checks the ADR-179 rule itself before queuing (satOwnsControlSetpoint() && satBlocksExternalCommand(), the same predicate addCommandToQueue() and the OT-Direct handler use), counts it with satNoteExternalRefused() and answers 409 'Command refused: SAT is in control of the boiler (disable SAT to send it)'; 503 stays for a full queue / ring. Side effect, wanted: on OT-Direct the command used to get 202 and was refused later on the loop task, now the client sees the 409 too. MQTT and the other callers are unchanged. Evidence: tests/test_rest_command_sat_refusal.py compiles the REAL handleCommandSubmit + otgwCommandFormatOk + satBlocksExternalCommand with a queue double that refuses like addCommandToQueue: fix 4/4 OK (CS=/MM= with SAT -> 409, not queued, counted; HW=/PR= with SAT -> 202; CS= without SAT -> 202; full queue -> 503); against origin's sources (OTGW_SRC_DIR) FAILED: 'satcs status=503 queued=1 noted=1 msg=Command not queued (queue full)'. test_rest_command_format, test_sat_command_delivery PASS. Web UI (v2.js sendCmd) shows the firmware's error message, so the 409 text reaches the user. openapi.yaml: 409 and 503 on POST /v2/otgw/commands (503 was undocumented too); docs/api/README.md. evaluate 0 FAIL.

2026-11-15 bench (09 on .63, bench build 2.0.0-beta.304+ffd0a64, relayed by the coordinator): PASS: CS=40 with SAT enabled -> 409 with the message, HW=1 -> 202, SAT off -> CS=40 202. The .61 (OT-Direct) part follows after ~14:56.
<!-- SECTION:NOTES:END -->
