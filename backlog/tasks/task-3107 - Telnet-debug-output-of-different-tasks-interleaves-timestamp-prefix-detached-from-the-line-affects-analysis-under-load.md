---
id: TASK-3107
title: >-
  Telnet debug output of different tasks interleaves (timestamp prefix detached
  from the line), affects analysis under load
status: To Do
assignee: []
created_date: '2026-11-26T16:24:00Z'
labels:
  - debug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1031000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-3104 sweep on .63 (beta.453+564ac47), bench-tester 17:18 2026-11-26. Under REST load the telnet log holds lines whose timestamp prefix sits at the end of another task's line (example: the prefix of '[restgate] seq=225' at the end of a processAPI/webConnCapExceeded line). async_tcp (processAPI debug lines) and the loop task (restGateLogTick) write to telnet at the same time; the prefix (_debugBOL) and the body (Debugf) are separate writes (debugStuff.h:87), so a line from another task can land in between. Effect: tools that parse the prefix miscount (series 2 first analysis: 1087 of 1758 lines intact, false 'loop slip'). Not a firmware behaviour risk; it affects every telnet analysis under load. Evidence: OTGW-field-data/bench-63/2026-11-26_rest-study/sweep_ble_off/ telnet.raw on the NAS (runs/sweep2), analysis_series2_ble_off_v2_by_seq.txt.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A reproduction with the load tool (sweep_tools/loadtest_rotation.py, offered 8) shows detached prefixes in telnet.raw on the old code, counted (intact lines / all seq)
- [ ] #2 The writer paths are named with file:line (which tasks write telnet, and where prefix and body are split)
- [ ] #3 A fix with a per-line lock or a single writer, only if cheap (no blocking of async_tcp, no String in hot paths, ADR-180), or a written reason why not
- [ ] #4 The same reproduction on the fix shows 0 detached prefixes; a host test where possible
<!-- AC:END -->
