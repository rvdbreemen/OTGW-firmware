---
id: TASK-1281
title: 'Web UI: optional help text with an info icon for every setting'
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 09:32
updated_date: '2026-10-22T07:49:00Z'
labels:
  - ui
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 410000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request gruijter 2026-10-21 (#alpha-testing). Most settings show only a short label (examples: MQTT 'Separate thermostat/boiler topics', 'Legacy OT topic layout', 'Publish on change'; OT-Direct 'Enable slave (thermostat) passthrough', 'Fail-safe on OT bus loss'; SAT 'Summer simmer', 'Flame-off offset', 'Modulation suppression', 'DHW enabled (standalone)' vs 'DHW enable (HW= master)'). Proposal: optional help field in SET_META (v2.js) next to label/hint, hint stays for units; render an info icon after the label, tooltip on hover/focus, popover on tap; 1-2 sentences per setting (what, when to change, default/side effects), sourced from the manual; move the SAT Indoor temperature explanation into its popup. Mind the LittleFS budget (768K, ADR-191) for the added text.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SET_META supports a help field and the settings page renders an accessible info icon with tooltip (hover/focus) and popover (tap)
- [x] #2 Help text exists for every curated setting, sourced from the manual, checked by a test that lists settings without help
- [x] #3 The SAT Indoor temperature inline block moves into its popup
- [x] #4 LittleFS image still fits with margin (size before/after recorded); screenshots desktop and mobile
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) Content: src/OTGW-firmware/data/settings-help.json, { key: text } for every SET_META key (v2.js ~1728, 181 entries), 1-2 plain sentences each (what it does, when to use it, unit/default when useful), <= 110 chars on average so the file stays <= 18 KB (LittleFS: staged 581 KB today, gate reports 640 KiB assets + 96 reserve <= 768; keep >= 10 KiB margin). Source: docs/manuals/en ch03/ch04/ch05/ch06; SAT entries from ch05 + docs/sat/firmware-sat-spec.md only (what the firmware does today, no claims about thermo-nova). 2) v2.js: helpFor(k) = SET_META[k].help || SETTINGS_HELP[k]; SETTINGS_HELP loaded once by fetch('settings-help.json') the first time an info icon is hovered/focused/clicked (one request, goes through the queued fetch wrapper; while loading the popover says 'Loading', on failure 'Help not available'). settingRow(): after the label an <button type=button class=sinfo aria-label='Help: <label>' aria-expanded=false aria-controls=shelp>. One shared popover #shelp (role=tooltip) appended to body: shown on mouseenter/focus, hidden on mouseleave/blur unless pinned; click/tap toggles pinned (aria-expanded), Escape and outside click unpin and hide, focus returns to the button; positioned below the icon, clamped inside the viewport, max-width 320px, textContent only. CSS in v2.css (bundle input): .sinfo (24px hit area, visible focus ring), #shelp. 3) AC3: the inline explanation of the SAT 'Indoor temperature' group (v2.js ~1871 satexternaltemp meta / group note) becomes the help text of satexternaltemp (and satsensormaxage), the inline block is removed. 4) Test tests/webui/settings-help.test.mjs: parse SET_META keys from the real v2.js (brace matching as other tests), load settings-help.json: every key has help 20..300 chars, no orphan keys, valid JSON, total size <= 18 KB; DOM-stub run of the real settingRow + helpFor: icon present with aria attrs, help resolved from SET_META.help first, then JSON. Red on HEAD. 5) Evidence: staged bytes + budget line before/after from build.bat; Playwright on the bench (filesystem update, settings kept) desktop 1280 and mobile 390: hover tooltip, pinned popover, keyboard focus, before vs after. 6) Docs: ch03/h03 one line, CHANGELOG. No API change; v2 UI is dev-only.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 RED (bench .88.61, alpha.454 fw / alpha.454 fs, headless Chromium 1280x900 and 390x844): Settings page and SAT > Control show 94 setting rows and 0 help icons; SAT 'Indoor temperature' card shows the 5-line inline explanation under the 'Use MQTT/REST as indoor temp source' toggle. Size before: staged 581366 B, gate 'LittleFS budget OK: 640 KiB assets + 96 KiB reserve <= 768 KiB' (build-455.log). Evidence %TEMP%\bench-1281\red-desktop-top.png, red-desktop-indoor.png, red-mobile-top.png, red-mobile-indoor.png, settings-baseline.json.

2026-10-22 GREEN (bench .88.61, filesystem of this diff via /update?cmd=100, settings diff empty; headless Chromium 1280x900 and 390x844): Settings page and SAT > Control: 94 help icons on 94 rows; hover shows the text; click pins (aria-expanded true), Escape hides and focus stays on the button (activeElement class sinfo); keyboard focus shows the popover; mobile popover fits inside 390 px; Indoor temperature card shows no inline block, its text is in the popup. /settings-help.json served 200, 17758 B, application/json. Size: before 'LittleFS budget OK: 640 KiB assets + 96 KiB reserve <= 768 KiB' (staged 581366 B), after '664 KiB + 96 <= 768' (JSON 17758 B = 5 blocks, plus JS/CSS growth); margin 32 -> 8 KiB. Fixed on the way: otdsetbacktimeout hint said Minutes, OTDirect.ino:2576 multiplies by 1000 and the default is 30 s (OTDirecttypes.h:113): hint now Seconds. Evidence %TEMP%\bench-1281\green-*.png (10 files) vs red-*.png, build-1281.log.

2026-10-22 review (52 agents, lenses correctness/rules/tests/help-text accuracy; 2 skeptics per finding): 20 findings, 19 confirmed 2/2, 1 split. Fixed in v2.js: popover measured at left 0 (was squeezed on a phone: shrink-to-fit of a fixed box), mouse-click focus no longer keeps a hover open (:focus-visible guard), Escape refocuses the button only when pinned, hoverable tooltip (WCAG 1.4.13, 120 ms grace), aria-expanded only when pinned, aria-live on the popover, hide when the anchor scrolls out of view. Firmware: FSexplorer.ino ETag route for /settings-help.json (ADR-139). Help texts corrected against the code: otdsetbacktemp (flow setpoint, not room), satsolarminelev (MQTT elevation only), satcomfortadjust (summer simmer only, weather humidity), satweatherinterval 900-3600, 11 OT-Direct keys 'Master mode only', satthermalcomfort scope, ntpsendtime every minute, satflameoffset hold/overshoot condition, sathumiditytimeout scope; JSON now 18300 B of 18432. Tests: 9 added (27 total), state-declaration fallback replaced by an assertion, stub emulates shrink-to-fit and :focus-visible; 5 mutants red; webui 147/147; evaluate 72/0. Pre-existing swapped trigger-bit options filed as a separate task. Rejected: none.

2026-10-22 GREEN round 2 (alpha.456+4ffa66d build of the reviewed diff, firmware via safeboot + filesystem via /update?cmd=100, settings diff empty): same browser results as round 1 (94/94 icons, hover, pin, Escape, keyboard focus, both widths); /settings-help.json 200 with ETag "4ffa66d-/settings-help.json" and Cache-Control no-cache, If-None-Match -> 304 with 0 B. Budget after the review fixes: 'LittleFS budget OK: 664 KiB assets + 96 KiB reserve <= 768 KiB'. Evidence %TEMP%\bench-1281\green2-*.png, build-1281b.log.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Every curated setting (181 SET_META keys) now has an info icon on the Settings page and the SAT settings: a real button with aria-label/aria-controls, hover or keyboard focus shows a shared role=tooltip popover (aria-live), tap or click pins it (aria-expanded), Escape or a click elsewhere closes it, the tooltip itself is hoverable, and it is clamped inside a 390 px viewport. The texts live in settings-help.json (18.3 KB, fetched once on first use, ETag route per ADR-139), written from the code's defaults and ranges and, for SAT, from docs/sat/firmware-sat-spec.md; SET_META.help stays as an inline override. The SAT Indoor temperature inline block moved into its popup (AC3). AC1: bench screenshots + 27 host tests on the real functions. AC2: test lists any key without help (red on HEAD), 181/181 covered. AC4: budget 640 -> 664 KiB assets (+96 reserve <= 768), margin 32 -> 8 KiB, follow-up TASK-1294; screenshots desktop 1280 and mobile 390 before/after. Review (52 agents) found 20 real issues, all fixed (popover width on phones, mouse-focus lingering, Escape focus theft, hoverable tooltip, aria-expanded semantics, aria-live, off-screen anchor, ETag route, 9 wrong help texts, 4 test gaps); the otdsetbacktimeout hint said Minutes and now says Seconds; the swapped trigger-bit labels are pre-existing and filed as TASK-1295. Not verified: Firefox, Safari/iOS and screen readers (behaviour derived from the DOM and ARIA attributes only); real touch devices.
<!-- SECTION:FINAL_SUMMARY:END -->
