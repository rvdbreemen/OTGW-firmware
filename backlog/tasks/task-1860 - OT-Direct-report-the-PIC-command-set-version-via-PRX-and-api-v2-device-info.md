---
id: TASK-1860
title: 'OT-Direct: report the PIC command-set version via PR=X and /api/v2/device/info'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T05:42:00Z'
updated_date: '2026-11-12T05:48:00Z'
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
- [ ] #1 PR=X on OT-Direct answers 'PR: X=<OTD_PIC_COMPAT_VERSION>' (e.g. 'PR: X=6.8'); host test on the real PR branch; pyotgw get_reports still runs on all replies
- [ ] #2 GET /api/v2/device/info carries the version (e.g. a pic_compat field) on OT-Direct and says it does not apply on a PIC board; host or JSON test on the real emitter
- [ ] #3 docs/api (device/info) and the manual's PR= list name the new report and field
- [ ] #4 Bench by the bench-tester on the OT-Direct unit: PR=X and device/info show 6.8
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Constraint (coordinator, 2026-11-12): on a real PIC, PR=X answers BV (gateway.asm:2163-2189: X is one of the letters H/J/K/U/X/Y/Z that the PIC 6.8 refuses). Tools must not rely on PR=X across board types: it exists only on OT-Direct. A client that needs the version on every board reads the PR=A banner (PIC: 'OpenTherm Gateway 6.6'; OT-Direct: 'OpenTherm Gateway 6.8 OTGW32') or /api/v2/device/info. The REST contract change goes into docs/api in the same commit as the code.
<!-- SECTION:NOTES:END -->
