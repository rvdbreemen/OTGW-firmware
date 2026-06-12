---
id: TASK-763
title: >-
  fix(webui): settings label-width normalize poisoned by WiFi-scan panel
  containers
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 17:38'
updated_date: '2026-05-29 18:05'
labels:
  - webui
  - settings
  - regression
  - field-report
milestone: 2.0.0
dependencies: []
references:
  - 'src/OTGW-firmware/data/index.js:6312'
  - 'src/OTGW-firmware/data/index.js:8210'
  - 'src/OTGW-firmware/data/components.css:636'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Regression from 1a01bb82 (normalize label width). normalizeSettingsLabelWidth() (index.js) measures `.settings-group-body .settings-field-container` and writes the max as a px value into --settings-label-w, which all settings-group grids use for grid column 1. buildWifiScanPanel() (index.js:8210/8216) reuses the .settings-field-container class on a heading <div> and a full-width <p> info paragraph, both appended into the network group's .settings-group-body. The broad selector measures that wide paragraph, so --settings-label-w is set to a huge px -> grid col 1 blows out on EVERY settings card -> cards exceed the 520px max-width and inputs are flung to the far right (both themes). Worked in dev testing because the WiFi scan panel / network group was absent there.

Fix: scope the measurement to real row labels only -- `.settings-group-body .settingDiv > .settings-field-container` (direct label child of a settingDiv). This excludes the wifi-scan heading/paragraph (children of #wifi-scan-panel, not a .settingDiv) and the nested fixed-ip labels (inside .fixed-ip-row), leaving only the actual setting-row labels that the column should size to.

2.0.0-only. Field report via @sergeantd alpha.92 screenshot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 normalizeSettingsLabelWidth() only measures direct .settingDiv label children (.settings-group-body .settingDiv > .settings-field-container); the WiFi-scan heading/paragraph and fixed-ip nested labels are excluded
- [x] #2 Settings cards render at the compact smartphone width (<=520px) with label/input subgrid alignment in both light and dark themes
- [x] #3 --settings-label-w resolves to the widest real row label, not the WiFi-scan paragraph width
- [x] #4 python build.py green; python evaluate.py --quick no new failures (incl. design-system drift gate)
- [x] #5 Field-validated by @sergeantd on OTGW32: MQTT/NTP/WiFi settings cards no longer span full width with inputs flung right
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Two-part fix (verified in a headless Playwright harness against the real CSS):
1. JS (index.js normalizeSettingsLabelWidth): scope the measurement selector to `.settings-group-body .settingDiv > .settings-field-container` so only real row labels feed --settings-label-w.
2. CSS (components.css): add `.settings-group-body > #wifi-scan-panel { grid-column: 1 / -1; }` so the WiFi-scan panel spans both columns (like .fixed-ip-section) and its full-width heading/paragraph stop inflating the label column's max-content track.

JS-only was insufficient: the panel still inflated the grid track, stretching real labels (measured 720px). CSS-only still fed the wide <p> (495px) to the var. Both together: --settings-label-w resolves to 276px (widest real label), MQTT + Network cards both render at 520px, inputs sit inside the card (right=513, width=200)."
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Empirically verified with a headless Playwright harness using the real components.css + faithful DOM: combined fix -> labelVar 276px, cards 520px, wifiPanelGridColumn '1 / -1', input within card. Bumped alpha.93 -> alpha.94. Build green (ESP32+ESP8266+LittleFS). evaluate --quick 0 fail, design-system drift gate pass, 98.6%. AC#1-4 done; AC#5 needs @sergeantd field check.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in 2.0.0-alpha.94 (commit 9f24724a, pushed to origin/feature-dev-2.0.0). Settings cards no longer blow out to full width. Two-part fix: (1) normalizeSettingsLabelWidth() now measures only real row labels (.settings-group-body .settingDiv > .settings-field-container); (2) #wifi-scan-panel spans both grid columns (grid-column: 1 / -1) so its full-width heading/paragraph no longer inflate the label column track. Verified in a headless Playwright harness against the real CSS: --settings-label-w=276px, cards capped at 520px, inputs inside the card. Build green, evaluator green incl. design-system drift gate. Posted to #dev-sat-mqtt for @sergeantd field validation (closed per maintainer instruction: shipped + announced; remaining AC#5 is hardware field-check).
<!-- SECTION:FINAL_SUMMARY:END -->
