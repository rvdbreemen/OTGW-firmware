---
id: TASK-812
title: >-
  fix(sat-webui): SAT view needs a window/panel resize when manual settings are
  selected
status: To Do
assignee: []
created_date: '2026-06-02 16:28'
updated_date: '2026-06-02 16:28'
labels:
  - sat
  - webui
  - field-report
  - needs-repro
  - 2.0.0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report @sergeantd (OTGW32). 2026-05-30: "Alpha99 findings: It needs a window resizing when the manual settings are selected. Otherwise the window size is correct now" (i.e. sizing is fine EXCEPT when manual settings are selected). 2026-06-02 11:26-13:27 he confirmed it is STILL PRESENT on 2.0.0-alpha.139+c880a02 (replied to his original window-resize finding). When the manual settings option is selected in the SAT view, the panel/window does not resize to fit the revealed manual controls (layout too small / overflow). Likely a SAT settings/dashboard layout issue in data/sat.js or data/index.html + components.css (the manual-mode controls toggle). NEEDS from @sergeantd: a screenshot of the mis-sized state + which exact control toggles it (SAT control mode = manual? a manual-setpoint panel?), to repro. Verify on current build. 2.0.0-only.

NOTE: this bug was mistakenly dismissed as fixed during a 2026-06-02 /backlog_discord triage (the "Otherwise the window size is correct now" line was misread as a self-confirmed fix). It is not fixed. Filing now to correct that miss.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause identified: which SAT control/setting toggles the manual view and why the container does not resize (with repro from @sergeantd screenshot)
- [ ] #2 When manual settings are selected, the SAT panel resizes to fit the manual controls with no overflow/clipping, on the browsers we support
- [ ] #3 python build.py green (fw+fs); evaluate.py --quick no new failures
- [ ] #4 Field-confirmed by @sergeantd on OTGW32
<!-- AC:END -->
