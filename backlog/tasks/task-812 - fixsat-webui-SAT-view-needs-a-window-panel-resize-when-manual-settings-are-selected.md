---
id: TASK-812
title: >-
  fix(sat-webui): SAT view needs a window/panel resize when manual settings are
  selected
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 16:28'
updated_date: '2026-06-02 17:02'
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
- [x] #3 python build.py green (fw+fs); evaluate.py --quick no new failures
- [ ] #4 Field-confirmed by @sergeantd on OTGW32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-02 (loop) — CANDIDATE FIX applied (agent inv812, CSS-only, NOT yet committed/built/field-confirmed).

ROOT CAUSE (proven via CSS scoping): SAT Settings rows are .settingDiv built by buildSatSettings (index.js:4265-4334) inside .sat-settings-group-body. The MAIN settings page wraps the same .settingDiv inside .settings-group-body and applies float-neutralisation + responsive overrides at components.css:769-798. The SAT body has NO equivalent override, so SAT settings rows fall back to the legacy float layout: .settingDiv .settings-field-container = fixed width:320px float:left (components.css:707-710); input min-width:240px (components.css:711-716) and that selector OMITS select, so a <select> is uncapped. A select with a long option (e.g. manual sensor address rendered '(manual)', index.js:3982-3986) grows to its widest option and, with the 320px floated label, forces the row wider than a narrow .sat-settings-group-body -> horizontal overflow/clipping, float never clears into panel. Matches 'everything sizes correctly EXCEPT this case'.

FIX (components.css, +42 lines after 1571): new .sat-settings-group-body > .settingDiv block mirrors main-page float-neutralisation WITHOUT subgrid (SAT body is padding+border-top, not a grid; subgrid would silently break). float:none; width:auto; min-width:0; flex-wrap; label wraps (overflow-wrap:anywhere); input AND select get min-width:0; max-width:100%. KISS, additive, scoped to SAT settings body. Verified DOM structure + selectors against index.js:4265-4304 (main thread).

HONESTY BOUNDARY: exact control @sergeantd hit is UNCONFIRMED. No reveal-on-select logic in any owned file or index.js; 'manual settings' is his phrasing, '(manual)' is only an option-label suffix not a panel-revealing toggle. This is a proven layout-asymmetry fix + best match to the symptom class, NOT a screenshot-confirmed repro.

DISCRIMINATOR the screenshot must resolve: is George on the SAT SETTINGS page (this fix applies) or the SAT DASHBOARD (this fix does nothing; a dashboard no-resize would more likely be an ECharts .resize() gap, already handled sat.js:722 switchView + sat.js:700 toggleCurve). Screenshot must show which page + which control.

AC#1 (repro from screenshot) + AC#4 (field-confirm) PENDING @sergeantd. AC#2 NOT confirmed yet. buglog bug-033 logged.

2026-06-02 (loop) — committed 6cb41f51, pushed origin/feature-dev-2.0.0-otgw32-esp32-sat-support. Build green both targets (esp8266 + esp32, fw+fs SUCCESS); evaluate.py --quick 61 passed / 0 failed / 2 pre-existing warnings. Bumped alpha.140->141. AC#3 checked. AC#1/#2/#4 pending @sergeantd screenshot (which page + control) to confirm repro + visual fix.
<!-- SECTION:NOTES:END -->
