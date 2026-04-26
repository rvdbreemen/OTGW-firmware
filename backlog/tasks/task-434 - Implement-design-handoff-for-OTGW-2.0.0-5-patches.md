---
id: TASK-434
title: Implement design handoff for OTGW 2.0.0 (5 patches)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-26 22:19'
labels:
  - enhancement
  - ui
  - design
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Land the 5 design patches from `design_handoff_otgw_2_0_0/` on the
feature-dev-2.0.0-otgw32-esp32-sat-support branch.

Source: design_handoff_otgw_2_0_0/README.md plus per-patch PATCH.md.

Five independent patches against `src/OTGW-firmware/data/`:

01. Status color tokens — consolidate 3 greens / 2 reds into `--status-*`
    tokens in ds-tokens.css with body.dark overrides; sweep callers in
    index.css and index_dark.css.
02. SAT panel theme corrections — new sat.css; rename .sat-card
    -> .sat-tile and .sat-card-interactive -> .sat-tile.is-target;
    rename .sat-badge / sat-badge-disabled -> .sat-state-pill is-*;
    add <link href="sat.css">.
03. OTMonitor home page refresh — new otmonitor.css; <link>; add
    data-state attributes to flame/CH/DHW/fault rows; add is-fault
    class to the fault row.
04. ECharts self-host — generate custom ECharts 5.4.3 line+bar build,
    pin via echarts.VERSION; new echarts-theme.js helper; swap CDN
    script tag in index.html; refactor sat.js + graph.js chart-init
    sites to merge otgwChartTheme().
05. DHW slider live fill — new sat-slider.js; <script>; add
    `document.dispatchEvent(new Event('sat:rendered'))` after every
    SAT panel re-render in sat.js so the live-fill survives WS-driven
    re-renders.

User chose Option A: all five patches in a single session, including
the Node-toolchain custom ECharts build for patch 04.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Patch 01: ds-tokens.css ships --status-ok/-error/-warn/-info/-neutral plus -*-glow tokens with body.dark overrides; getComputedStyle returns the documented values in both themes
- [ ] #2 Patch 01: every legacy hex green (#4caf50, #27ae60) and red (#f44336, #e74c3c) in index.css and index_dark.css is replaced by the matching --status-* token; simulation purple, gradients and inline SVG fills are left intact
- [ ] #3 Patch 02: data/sat.css exists and is linked AFTER index_dark.css in index.html; SAT panel renders with new .sat-tile / .sat-state-pill / .sat-tile.is-target class names; theme toggle swaps every SAT element together
- [ ] #4 Patch 03: data/otmonitor.css exists and is linked; flame/CH/DHW/fault rows carry data-state attributes; fault row uses is-fault to flip to --status-error; home page cards show neutral surfaces instead of light-blue tint
- [ ] #5 Patch 04: data/echarts.min.js exists, is a custom build pinned to the version in data/echarts.VERSION, and contains only line+bar chart types; data/echarts-theme.js exists; CDN <script> in index.html is replaced by local one; sat.js and graph.js merge otgwChartTheme() into their option objects and refresh on theme toggle
- [ ] #6 Patch 05: data/sat-slider.js exists and is linked AFTER sat.js; sat.js dispatches document.dispatchEvent(new Event('sat:rendered')) after every SAT panel re-render; dragging the DHW slider updates --sat-slider-pct and the °C readout in real time, and survives a re-render
- [ ] #7 python build.py exits 0 for both ESP8266 and ESP32 targets after every patch
- [ ] #8 LittleFS image stays under the partition limit (1.5 MB on ESP32, 1.98 MB on ESP8266) after Patch 04 lands the 180 KB ECharts bundle
- [ ] #9 evaluate.py --quick passes (no PROGMEM regressions or unsafe-pattern flags introduced by webui changes)
- [ ] #10 Five separate commits land on feature-dev-2.0.0-otgw32-esp32-sat-support, one per patch, each with a Patch NN reference in the commit subject so a single patch can be reverted
<!-- AC:END -->
