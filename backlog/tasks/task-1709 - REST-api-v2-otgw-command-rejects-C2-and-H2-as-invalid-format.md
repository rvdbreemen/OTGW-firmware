---
id: TASK-1709
title: REST /api/v2/otgw/command rejects C2= and H2= as invalid format
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T19:01:00Z'
updated_date: '2026-11-07T19:24:00Z'
labels:
  - rest
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 820000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
handleCommandSubmit (restAPI.ino) requires two letters before '=', so the real PIC commands C2= (CH2 setpoint) and H2= (CH2 enable) get HTTP 400 'Invalid command format (expected LL=value)'. Found on the TASK-1695 bench (.61, beta.127). Robert 2026-11-07: task + fix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 REST POST /api/v2/otgw/commands and /command/{cmd} accept a letter followed by a letter or digit, e.g. C2=40 and H2=1 (host test red on old code, green on fix)
- [x] #2 Malformed prefixes (non-alphanumeric, digit first) are still rejected with 400 (test)
- [x] #3 Bench: REST C2=40 with SAT off is queued (202), with SAT on refused by the TASK-1695 gate (ext_cmd_refused +1)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix: inline otgwCommandFormatOk() in OTGW-firmware.h (letter, then letter or digit, then '='), used by restAPI.ino handleCommandSubmit; error text now 'expected XX=value'. tests/test_rest_command_format.py compiles the real predicate (20 cases: C2=40, H2=1, c2=40 ... accepted; 2C=40, 1A=0, C=40, non-ASCII, ... rejected) and checks the handler calls it. Old code: FAILED (predicate missing, handler still two-letter); fix: 2 OK. test_sat_command_delivery 22 OK; evaluate --quick 85/0. beta.128.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Done 2026-11-07, beta.128 445ba137e (origin/dev, zip #alpha msg 1568707678815916154). AC1: tests/test_rest_command_format.py compiles the real otgwCommandFormatOk (C2=40, H2=1, c2=40 accepted) and checks handleCommandSubmit calls it: old code FAILED, fix OK. AC2: same test, 2C=40, 1A=0, C=40, non-ASCII, CS40 rejected; bench 2C=40 -> 400. AC3: bench .61 SAT off: C2=40/H2=1 -> 202, override MsgID 8 = 10240 (40.0); SAT on: both refused, ext_cmd_refused 1,2. Cleared with C2=0/H2=0; after SAT back on only SAT's own MsgID 1/14 overrides remain. evaluate --quick 85/0, test_sat_command_delivery 22 OK. Log scratchpad b103/bench1709.log.
<!-- SECTION:FINAL_SUMMARY:END -->
