---
id: TASK-544
title: SAT heating curves invisible on 2.0.0 — diagnose and harden
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-05 08:54'
updated_date: '2026-05-05 15:58'
labels:
  - bug
  - sat
  - webui
  - echarts
  - feat-2.0.0
dependencies: []
references:
  - src/OTGW-firmware/data/sat.js
  - src/OTGW-firmware/data/index.html
  - src/OTGW-firmware/data/components.css
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/helperStuff.ino
  - src/OTGW-firmware/restAPI.ino
  - commit 07991766f5f9d07a3561c5c1225dba29e68a2751
  - commit 5f4db1ae
priority: high
ordinal: 15000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reference curves and operating-point dot in the SAT dashboard 'Heating Curve (Stooklijn)' chart (#sat-curve-chart, expert/diag view) are reported missing on feature-dev-2.0.0-otgw32-esp32-sat-support despite commits 5f4db1ae (initial feature, 2026-04-04) and 07991766 (operating-point dot null-handling fix, 2026-05-02).

REST endpoint /api/v2/sat/status emits all five required fields unconditionally (coefficient, heating_system, target_temp, outside_temp, heating_curve — see SATcontrol.ino:1532-1581) so a data-shape mismatch is refuted.

Three remaining root-cause hypotheses, in descending likelihood:
1. Stale LittleFS image. Working-tree data/version.hash is uncommitted at 1d83577 while HEAD is bb67d36 — if firmware was built+flashed but filesystem partition was not re-uploaded, deployed sat.js may predate 07991766. helperStuff.ino:682-710 already detects this via StatusMessage::LittleFSMismatch but the SAT page does not surface it prominently.
2. Operating-point dot clipped by yAxis.min:10 (sat.js:510) when boiler is idle and state.sat.fHeatingCurveValue=0. Reference curves should still render in this case — diagnostic split.
3. ECharts CDN block. Synchronous <script src=cdn.jsdelivr.net> with SRI in index.html:29-31; if blocked, initCurveChart short-circuits silently on typeof echarts === 'undefined' (sat.js:520) and both SAT charts go blank.

Diagnostic ladder + recommended hardening documented in plan file ~/.claude/plans/van-de-week-hadden-parsed-scroll.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a fresh full filesystem flash with SAT enabled and coefficient=1.5, heating_system=0, target_temp=20.0, the SAT page in expert mode displays exactly 10 gray reference curves c=0.5..5.0 with c=1.5 highlighted orange
- [ ] #2 With no boiler activity (heating_curve=0, outside_temp=0) no orange dot is plotted below the y-axis: either suppressed or clamped to y=10 with a tooltip/label explaining the boiler is idle
- [ ] #3 When outside_temp is changed via POST /api/v2/sat/externaloutdoor the orange dot moves on the X axis within one poll cycle (5s) without manual page reload
- [ ] #4 When deployed LittleFS hash differs from firmware hash, the SAT page shows a visible banner re-using StatusMessage::LittleFSMismatch from helperStuff.ino:704
- [ ] #5 When window.echarts is undefined, #sat-curve-chart shows the text 'Charts unavailable: echarts CDN failed to load' instead of remaining blank
- [ ] #6 No new String instances; no PROGMEM violations; no HTTPS-only assumptions on the firmware HTTP API
- [ ] #7 ./build.sh build --target esp32 completes without new compile warnings; LittleFS image fits the configured partition size from partitions_otgw_esp32.csv
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Phase 1 — Diagnostic ladder (browser, hardware required, ~5 min)

### 1.1 Four console probes on the SAT page (F12)
```js
document.querySelector('.sat-dashboard').className
// must contain 'sat-view-expert' or 'sat-view-diag' (else view filter hides curve)

typeof window.echarts
// must be 'object' (else H3 — CDN failed/blocked)

fetch('/api/v2/sat/status').then(r=>r.json()).then(d=>console.log({
  coefficient:d.coefficient, heating_system:d.heating_system,
  target_temp:d.target_temp, outside_temp:d.outside_temp,
  heating_curve:d.heating_curve
}))
// all five must be numbers; sanity-check ranges

document.getElementById('sat-curve-chart').getBoundingClientRect()
// width and height > 0 (else section collapsed via toggleCurve)
```
Plus: `window.SAT_DEBUG = true; location.reload();` for per-fetch traces (sat.js:40–46).

### 1.2 Visual checks
- Red banner "Flash your littleFS with matching version!" at top of page?
- Canvas completely empty (no axes, no gray curves) vs only the orange dot missing?

### 1.3 Branch on outcome
- Banner visible OR firmware/FS hash mismatch → **H1 stale LittleFS** → Phase 2A
- `typeof echarts === 'undefined'` → **H3 ECharts CDN** → Phase 2C
- Reference curves render but no dot, JSON shows `heating_curve: 0` or `outside_temp` outside [-15, 25] → **H2 dot clipping** → Phase 2B
- Everything looks fine on paper but chart still blank → deeper inspection (cache, JS error stacktrace, version drift)

## Phase 2 — Targeted fix per hypothesis

### 2A — H1 stale LittleFS (no source change)
1. `./build.sh build --filesystem --target esp32` (rebuild LittleFS image)
2. Flash filesystem partition (OTA via `/update` page or serial)
3. Hard reload SAT page (Cmd+Shift+R) → confirm reference curves render
4. Pair with Phase 3 Fix B to prevent recurrence

### 2B — H2 dot clipping (sat.js:467–477)
In `buildCurveOption`, before pushing the `Current` scatter series: if `currentSetpoint < 10` or `> 80`, push empty `data: []` instead of clipped coords. ~5 lines, browser-side only. Optional: `markPoint` label "(idle)" on y-axis to indicate boiler has not yet computed a curve.

### 2C — H3 ECharts CDN (sat.js:520 + 349)
Replace silent `return` in `initCurveChart` and `initChart` with:
```js
container.textContent = 'Charts unavailable: echarts CDN failed to load. Check your network or self-host echarts.';
```
~3 lines per site. Follow-up (separate task): self-host ECharts on LittleFS (~270 KB FS bump, removes WAN dependency).

## Phase 3 — Defensive hardening (root-cause independent)
These prevent future debug rounds regardless of Phase 2 outcome.

### Fix B — LittleFS mismatch banner on SAT page
Reuse existing detector at `helperStuff.ino:682–710` (`StatusMessage::LittleFSMismatch`). In `sat.js:618` (`start()`), fetch `/api/v2/devinfo`, and if mismatch active render a red banner above `.sat-dashboard`. ~15 lines JS, no backend change.

### Fix C — Visible ECharts-unavailable message
Identical to 2C but applied unconditionally — silent-failure → visible-failure is always a win.

### Fix D (optional) — First-poll force rebuild
`sat.js:534–567` (`updateCurveChart`): add `_firstPollDone` flag, force full `setOption(..., true)` on first invocation. ~5 lines, prevents partial-update path from misfiring on a freshly mounted chart.

## Phase 4 — Verify + ship (ESP32-S3)
1. `./build.sh build --filesystem --target esp32`
2. `./build.sh build --target esp32`
3. Flash filesystem first, then firmware (OTA in same order)
4. Hard reload SAT page (Cmd+Shift+R) to bypass JS cache
5. `window.SAT_DEBUG = true; location.reload();` — verify `[SAT] fetch ok …` and `[SAT] updateDashboard …` logs
6. UI in **expert mode**:
   - `#sat-chart` renders with axes (Temperature History)
   - `#sat-curve-chart` shows 10 gray reference curves c=0.5..5.0, c=1.5 highlighted orange
   - Label: `Radiator | Coefficient: 1.5 | Target: 20.0°C`
7. Operating-point trace: `curl -X POST --data '5.0' http://<device>/api/v2/sat/externaloutdoor` → orange dot moves on X axis within 5 s
8. Coefficient switch: `curl -X POST -H 'Content-Type: application/json' -d '{"name":"satcoefficient","value":"3.0"}' http://<device>/api/v2/settings` → orange highlight switches c=1.5 → c=3.0
9. Theme toggle (sat.js:642–678) and view switch simple↔expert↔diag (sat.js:607–608) — chart resizes + re-inits without errors
10. Commit on feature-dev-2.0.0-otgw32-esp32-sat-support referencing TASK-544
11. Push (standing permission per CLAUDE.md push policy)
12. `backlog task edit 544 --check-ac N` for each AC, set status Done, write final summary

## Key files (with anchors)
- `src/OTGW-firmware/data/sat.js` — `initCurveChart` (518), `updateCurveChart` (534), `buildCurveOption` (~410), `_approxChanged` (528), `start` (618)
- `src/OTGW-firmware/data/index.html` — SAT panel (319+), echarts CDN tag (29–31), curve-chart container (365–373)
- `src/OTGW-firmware/data/components.css` — view-mode filter (1366–1368)
- `src/OTGW-firmware/SATcontrol.ino` — `/api/v2/sat/status` JSON emit (1532–1581), `satCalcHeatingCurve` (385)
- `src/OTGW-firmware/restAPI.ino` — endpoint dispatch (736–752), `externaloutdoor` POST (783–797)
- `src/OTGW-firmware/helperStuff.ino` — `StatusMessage::LittleFSMismatch` detector (682–710)

## Constraints
- ADR-004: no `String` class (browser JS unaffected)
- HTTP-only firmware API; ECharts over HTTPS via CDN is acceptable in browsers
- Browser support: Chrome/Firefox/Safari latest + 2 versions back

## Phasing & status discipline
- Phase 1 + 2 require hardware (device + browser).
- Phase 3 Fix B + Fix C are root-cause independent and can be prepared offline; pushing without hardware verification is acceptable for these because they degrade silent failures into visible ones (cannot make working code worse).
- Phase 3 Fix A (2B clamp) requires hardware verification before push because it changes plotted data.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-05: Plan written into task. Maintainer reports hardware unavailable today, so Phase 1 (browser diagnostic ladder) is deferred. Phase 3 Fix B (LittleFS-mismatch banner) and Fix C (ECharts-unavailable message) are root-cause independent and can be prepared offline if requested — both convert silent failures to visible ones, low risk to push without hardware verification. Status stays "To Do" until maintainer confirms next move.

Full design rationale and Phase 1/2 hypothesis evidence in plan file at ~/.claude/plans/van-de-week-hadden-parsed-scroll.md.

[2026-05-05] Implemented the offline-safe hardening slice for TASK-544:
- SAT page now renders a visible in-page LittleFS mismatch banner by reusing the existing device-time status message path (`getStatusMessageText()` via `statusMessageText`).
- `sat-chart` and `sat-curve-chart` now show "Charts unavailable: echarts CDN failed to load" instead of failing silently when `window.echarts` is unavailable.
- Heating-curve operating-point dot now suppresses itself when the point would fall outside the chart bounds (e.g. idle `heating_curve=0` below y-axis min).
- Local validation: `./build.sh --target esp32` succeeded (firmware + LittleFS), `.build-venv/bin/python evaluate.py --quick` passed with no FAILs.
- Remaining blocker: AC #1 and AC #3 still need browser/device verification on actual SAT hardware, so task stays In Progress.
<!-- SECTION:NOTES:END -->
