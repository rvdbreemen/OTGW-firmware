---
id: TASK-1860
title: 'OT-Direct: report the PIC command-set version via PR=X and /api/v2/device/info'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T05:42:00Z'
updated_date: '2026-11-12T08:05:00Z'
labels:
  - otdirect
  - api
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 936000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, in session 2026-11-12: besides the PR=A banner 'OpenTherm Gateway <version> OTGW32' (TASK-1844 decision), the PIC command-set version OT-Direct implements (OTD_PIC_COMPAT_VERSION, 6.8 with TASK-1842/1844) gets (A) an extra report under a free PR letter and (B) a field in the REST API. His words: '2, extra commando met vrije commando letter. En B uitbreiding van API.'

Free letters: the PIC 6.8 answers BV for PR=H, J, K, U, X, Y, Z (gateway.asm:2163-2189, docs/pic/pic-firmware-6.8-spec.md section c); OT-Direct uses none of them; pyotgw asks only its fixed list (vars.py OTGW_REPORTS) and ignores others; OTmonitor matches only the letters it knows. Proposed letter: X -> 'PR: X=6.8'. On a PIC board the PIC answers PR=X with BV, which is correct there (no OT-Direct).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PR=X on OT-Direct answers 'PR: X=<OTD_PIC_COMPAT_VERSION>' (e.g. 'PR: X=6.8'); host test on the real PR branch; pyotgw get_reports still runs on all replies
- [x] #2 GET /api/v2/device/info carries the version (e.g. a pic_compat field) on OT-Direct and says it does not apply on a PIC board; host or JSON test on the real emitter
- [x] #3 docs/api (device/info) and the manual's PR= list name the new report and field
- [ ] #4 Bench by the bench-tester on the OT-Direct unit: PR=X and device/info show 6.8
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Constraint (coordinator, 2026-11-12): on a real PIC, PR=X answers BV (gateway.asm:2163-2189: X is one of the letters H/J/K/U/X/Y/Z that the PIC 6.8 refuses). Tools must not rely on PR=X across board types: it exists only on OT-Direct. A client that needs the version on every board reads the PR=A banner (PIC: 'OpenTherm Gateway 6.6'; OT-Direct: 'OpenTherm Gateway 6.8 OTGW32') or /api/v2/device/info. The REST contract change goes into docs/api in the same commit as the code.

Implementation (wt-95rt): OTDirect.ino: otdPicCompatVersion() returns OTD_PIC_COMPAT_VERSION (next to the define); PR=X case in the PR switch prints 'PR: X=' OTD_PIC_COMPAT_VERSION (the PIC 6.8 answers BV for PR=X, gateway.asm:2163-2189). restAPI.ino sendDeviceInfoV2(): field 'otdpiccompat' inside the if (snap->otDirectEnabled) block only. Docs: docs/api/openapi.yaml + docs/api/README.md (example and field table), manual ch10 PR= list (PR=X row, with the BV-on-a-PIC warning).

Evidence AC1/AC2: tests/test_otd_pr_reports_pyotgw.py 7 tests: X1 (PR=X reply from the real PR branch equals the PR=A banner version), DeviceInfoCompatField (field emitted once, only in the OT-Direct block; helper returns the macro; source-level, the chunked emitter is not sliceable), P1 real pyotgw get_reports still green. Fix 7/7 OK; old code (741af9428 OTDirect.ino + restAPI.ino via OTGW_OTD_INO / OTGW_REST_INO) 3 FAIL (X1, field, helper). Gap: the rendered device/info JSON is checked on the bench (AC4).

Gates 2026-11-12: all 72 tests that slice OTDirect.ino / restAPI.ino rerun one at a time in the foreground (an earlier background run was reaped for low memory twice): green; test_dhw_water_meter (382 s) and test_sat_lowload_contract (535 s, 35 tests OK) only exceeded my 240-300 s per-test limit and pass alone. evaluate --quick: 92 checks, 0 failed, 0 warnings.
<!-- SECTION:NOTES:END -->
