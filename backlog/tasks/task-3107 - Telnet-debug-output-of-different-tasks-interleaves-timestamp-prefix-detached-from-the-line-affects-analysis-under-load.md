---
id: TASK-3107
title: >-
  Telnet debug output of different tasks interleaves (timestamp prefix detached
  from the line), affects analysis under load
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-26T16:24:00Z'
updated_date: '2026-11-26T17:47:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan v2 (coordinator 18:5x, replaces the mutex idea): NO lock. (1) New helper _debugLinef_P(fn, line, fmt, ...) in debugStuff.ino: format prefix + body into ONE bounded stack buffer (384 B, body keeps the 319-char limit semantics, truncation marker unchanged) and hand it to debugTelnet.print() ONCE; DebugTf/DebugTln call it (Tln appends CRLF in the same buffer); DebugT stays two-part (16 sites, listed limit). (2) Prefix without function-statics: tz/second cache moves out of the shared statics into per-call locals; cost of ZonedDateTime per line under bMQTTGate to be measured on the bench before the cache question is closed (risk named). (3) A lock is discussed with the coordinator only if the host test still shows split lines. (4) Host test first, red on the old macros: two threads write tagged lines, count lines where prefix and body tags differ. (5) No firmware build until the coordinator bundles it with the next test build; AC1 load run on .63 asked via the coordinator after series 4 (~19:30).
<!-- SECTION:PLAN:END -->
