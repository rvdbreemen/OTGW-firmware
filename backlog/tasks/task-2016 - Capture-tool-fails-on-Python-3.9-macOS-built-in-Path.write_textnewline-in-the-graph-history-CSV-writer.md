---
id: TASK-2016
title: >-
  Capture tool fails on Python 3.9 (macOS built-in): Path.write_text(newline=)
  in the graph-history CSV writer
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T08:34:00Z'
updated_date: '2026-11-18T08:44:00Z'
labels:
  - tools
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1034000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 92 in indigo_light's macOS capture 20261118-081844 (#beta-testing msg 1572521874263515248): script.error.log 'Graph history at start failed: TypeError: write_text() got an unexpected keyword argument newline' (and at stop). scripts/otgw_capture.py:2621 calls Path.write_text(..., newline=''), which needs Python 3.10; the tool supports 3.9+ and macOS ships 3.9.6. The .json was kept, the CSV was not. Tools only, no src, no bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The graph-history CSV is written on Python 3.9; a test runs the writer under a real 3.9 interpreter, red on the old tool, green on the fix
- [x] #2 A test fails when the shipped tool uses 3.10+ syntax or a 3.10+ keyword (write_text/read_text newline=, zip strict=)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 (otgw-docs): reproduced on a real Python 3.9.25 (uv python install 3.9): Path.write_text(..., newline='') -> TypeError: write_text() got an unexpected keyword argument 'newline'. The capture suite under 3.9 on origin/dev: FAILED (failures=1, errors=2), all three in the graph-history CSV path (test_capture_device_data FetchCase x2 TypeError, DeviceDataThreadCase 0 != 1 CSV files). Fix: open(path, 'w', encoding='utf-8', newline='') at otgw_capture.py:2621. Sweep for other 3.10+ use: every scripts/*.py compiles under 3.9 (py_compile) and otgw_capture imports under 3.9; grep for zip strict=, bit_count, kw_only/slots, match, X | None found nothing else in the shipped tool (build.py:1290 ships otgw_capture.py plus the vendored paho wheel). scripts/sat_corrections_index.py:161 also uses write_text(newline=) but is a repo maintenance script run by agents on 3.12, not shipped: left as is. New tests/test_capture_py39_compat.py: 3.9 grammar parse, AST scan for 3.10+ keywords, and the graph-history tests under a 3.9 interpreter (OTGW_PY39 or uv; skipped without one). Old tool: 2 of 3 red ('otgw_capture.py:2621 write_text(newline=...)' and the TypeError); fix: 3/3 OK. Full capture suite on the fix: Python 3.9 OK (skipped=2), Python 3.12 OK (skipped=2).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: the CSV writer uses open(newline=''); under a real Python 3.9.25 the capture suite went from FAILED (failures=1, errors=2, the graph-history CSV path) to OK. AC2: tests/test_capture_py39_compat.py (3.9 grammar, AST scan for 3.10+ keywords, graph-history tests under 3.9) is 2 of 3 red on the old tool and 3/3 green on the fix; the 3.9 run is skipped where no 3.9 interpreter exists. Not verified on a Mac: the next macOS capture (TASK-1544 AC2 rerun) shows it.
<!-- SECTION:FINAL_SUMMARY:END -->
