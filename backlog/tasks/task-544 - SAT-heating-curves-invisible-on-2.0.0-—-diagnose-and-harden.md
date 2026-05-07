---
id: TASK-544
title: SAT heating curves invisible on 2.0.0 — diagnose and harden
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-05 08:54'
updated_date: '2026-05-07 19:21'
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
- [x] #2 With no boiler activity (heating_curve=0, outside_temp=0) no orange dot is plotted below the y-axis: either suppressed or clamped to y=10 with a tooltip/label explaining the boiler is idle
- [ ] #3 When outside_temp is changed via POST /api/v2/sat/externaloutdoor the orange dot moves on the X axis within one poll cycle (5s) without manual page reload
- [x] #4 When deployed LittleFS hash differs from firmware hash, the SAT page shows a visible banner re-using StatusMessage::LittleFSMismatch from helperStuff.ino:704
- [x] #5 When window.echarts is undefined, #sat-curve-chart shows the text 'Charts unavailable: echarts CDN failed to load' instead of remaining blank
- [x] #6 No new String instances; no PROGMEM violations; no HTTPS-only assumptions on the firmware HTTP API
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

2026-05-06 (claude): Verified hardening ACs by code review against the live tree. The plumbing called out in the implementation plan is fully landed:

- AC #4 (LittleFSMismatch banner): end-to-end path is helperStuff.ino:704 sets state.statusMessage=LittleFSMismatch -> restAPI.ino:2238 emits getStatusMessageText() under "message" in /api/v2/device/time -> data/index.js:4168 stores statusMessageText and calls SAT.renderStatusBanner() on every poll -> data/sat.js:95-108 renderStatusBanner() recognises the LittleFS string via isLittleFSMismatchMessage() and toggles div#sat-fs-mismatch-banner (index.html:308). Banner is also re-rendered on each devtime poll, so SAT page picks up the mismatch as soon as it is detected on-device.
- AC #5 (echarts CDN fallback): both initChart() (sat.js:402) and initCurveChart() (sat.js:575) gate on typeof echarts === 'undefined' and call showChartUnavailable(container) which writes ECHARTS_UNAVAILABLE_TEXT and adds the .chart-unavailable class. components.css:453 styles it. Applies to #sat-curve-chart per AC and to #sat-chart as bonus.
- AC #2 (no orange dot below y-axis when boiler idle): buildCurrentPointData() (sat.js:122) returns [] for null/non-finite inputs, for outside_temp outside [CURVE_X_MIN, CURVE_X_MAX]=[-15,25], and for currentSetpoint outside [CURVE_Y_MIN, CURVE_Y_MAX]=[10,80]. With heating_curve=0 the setpoint < 10 path returns [] -> the dot series receives no data -> nothing is plotted. Suppression branch satisfied.
- AC #6 (no String/PROGMEM/HTTPS violations): python3 evaluate.py --quick on the 2.0.0 worktree -> 0 failed, 2 warnings unrelated to SAT (ADR-062 mqtt_configuratie.cpp absence, sendMQTTheapdiag buffer arithmetic check pattern miss), 97.1% health.

Still needs on-hardware/in-browser validation:
- AC #1: visual confirmation that 10 grey reference curves c=0.5..5.0 render with c=1.5 highlighted orange after a fresh full-FS flash with SAT enabled.
- AC #3: visual confirmation that POSTing /api/v2/sat/externaloutdoor moves the orange dot within one 5s poll without page reload.
- AC #7: ./build.sh build --target esp32 clean compile + LittleFS partition fit check. Not run in this session because the 2.0.0 worktree carries unrelated uncommitted edits (TASK-409, MQTTstuff.ino) and a build run would not isolate SAT-only verification. Run after those commit or stash.

No source changes were made in this session; the verification is read-only.

---
**Field evidence — SergeantD on alpha.3+ca845dd, 2026-05-07 (Discord)**

Screenshot of SAT page submitted via Discord shows:
- TEMPERATURE HISTORY panel: empty dark gray frame, no data plotted.
- Heating Curve (Stooklijn) panel: empty dark gray frame, no curves drawn.

**Diagnostic for AC #1**: Heating Curve panel is blank, but the static reference curves (10 grey lines for c=0.5..5.0, plus the highlighted active-c) are computed in JS from a fixed formula and do NOT depend on OT bus, MQTT, or any live data. They should render even on a cold device with no thermostat/boiler attached.

Possible causes (all narrow the bug to chart initialisation, not data binding):
1. echarts loaded but `setOption()` never called for the curve chart, OR called with empty `series` array.
2. `echarts.init()` skipped because container layout is unresolved at init time.
3. JS error mid-init that hid the AC #5 fallback ("Charts unavailable: echarts CDN failed to load") but didn't draw anything either.

The Temperature History panel being blank is partly expected on his test setup: device is in OT-Direct mode with `online=0` and no boiler attached, so Tboiler / Tret / Tdhw genuinely have no data. However, BLE room temps and the `OT=24.8` outside override should still plot a trace; if those are also missing on the new build, that's a separate data-binding gap.

**Action**: asked SergeantD on Discord (2026-05-07) to flash alpha.8+1efc2f8 and re-screenshot the SAT expert-mode page. If the grey reference curves still don't render on alpha.8, AC #1 remains unfit and we need a fresh init-trace.

---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 5** (gated by tester re-screenshot, not blocked by Ship 1-3). Cannot proceed until SergeantD or another tester submits an alpha.8+ SAT-page screenshot. data/sat.js:576 initCurveChart with echarts.init at :583 and reference curves at buildCurveOption :470 — read these only if curves remain blank on alpha.8.

---
**Update 2026-05-07 (alpha.8 field evidence from SergeantD):**

The chart-init bug is resolved between alpha.3 and alpha.8 — the panel now renders grid, axes, "Outside °C" / "Flow °C" labels, AND the active orange dot at the current outside/flow point. echarts is initialised and the active series is constructed. AC #5 (echarts CDN fallback) is now moot because echarts is loading.

What still fails: the 10 grey reference curves c=0.5..5.0 are missing from the rendered chart. Diagnosis narrowed: it's a series-construction defect in buildCurveOption() at data/sat.js:470, NOT a chart-init failure.

**Status superseded by TASK-566** which targets the new (narrower) defect. AC #1 of THIS task should be considered resolved for the chart-init aspect; the c-curves aspect carried forward to TASK-566.
<!-- SECTION:NOTES:END -->
