---
id: TASK-766
title: >-
  fix(webui): keep settings subgrid columns inside card after label-width
  normalization
status: Done
assignee:
  - '@codex'
created_date: '2026-05-29 19:43'
updated_date: '2026-05-29 21:39'
labels:
  - webui
  - settings
  - regression
  - field-report
milestone: 2.0.0
dependencies: []
references:
  - >-
    backlog/tasks/task-763 -
    fixwebui-settings-label-width-normalize-poisoned-by-WiFi-scan-panel-containers.md
  - src/OTGW-firmware/FSexplorer.ino
  - src/OTGW-firmware/data/components.css
  - tests/webui_settings_layout.spec.js
  - tests/test_webui_asset_versioning.py
documentation:
  - docs/c4/c4-component-web-interface.md
  - docs/c4/c4-code-web-assets.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-763. Field screenshot from OTGW 2.0.0-alpha.98 shows Settings cards capped visually, but their two-column settings rows overflow far beyond the card: labels remain on the left while inputs are flung to the right.

Investigation showed the current Settings CSS/JS source keeps rows inside the card in a browser harness. The remaining failure path is stale asset caching: index.html is ETag-revalidated and shows the fresh firmware/filesystem version, while stylesheet links were unversioned and could leave the browser using an older components.css. The Web UI must version the local CSS assets with the filesystem hash and keep Settings card rows contained across desktop and compact widths.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Settings group row grids stay within the .settings-group card width; no .settingDiv, label column, input column, or WiFi scan panel overflows the card on the Settings page.
- [x] #2 Rows with an input plus a button, including the SSID Reset WiFi row, keep both controls in column 2 inside the card.
- [x] #3 WiFi scan heading, explanatory text, Scan button, scan status, and result select span the card content width without participating in label-column sizing.
- [x] #4 A browser/layout regression check covers the reported wide-desktop failure mode and a compact/mobile-width Settings card in light and dark themes.
- [x] #5 Relevant Web UI checks pass, including the design-system drift gate; firmware build impact is assessed.
- [x] #6 Local stylesheet links emitted from index.html include ?v=<filesystem hash> so stale components.css cannot pair with fresh index.html after an upgrade.
- [x] #7 Versioned stylesheet requests use the same Cache-Control behavior as versioned local JS assets: long cache when v matches the filesystem hash, no-cache otherwise.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Reproduce the missed case in tests by adding a realistic long Settings label in a lower group while asserting the visible System/Network/MQTT cards remain contained at wide desktop and compact widths.
2. Change Settings label-width normalization from one page-level --settings-label-w to per .settings-group-body widths, measured only from direct row labels in that group.
3. Add a card-relative cap and allow long labels to wrap inside their label column, so a single long label cannot force any settings row outside the card.
4. Keep WiFi scan and fixed-IP sections full-width and outside label-column sizing.
5. Run the focused Playwright layout test, the WebUI asset-versioning test, and evaluate.py --quick; assess whether a firmware build is needed for the touched files.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Investigation disproved the remaining label-width feedback-loop hypothesis: a Playwright harness using the real ds-tokens.css, components.css, and index.js keeps Settings rows, inputs, buttons, and WiFi scan controls inside the card at 1600px and 390px in light/dark themes. The field symptom matches stale cached CSS: index.html is ETag-revalidated and shows the fresh firmware/filesystem version, but stylesheet links were emitted without ?v=<filesystem hash> and could leave an older components.css active.

Patched FSexplorer.ino so streamed index.html versions ds-tokens.css and components.css with the filesystem hash. Added explicit CSS handlers with the same version-aware Cache-Control behavior already used by local JS assets. Added tests/test_webui_asset_versioning.py for the serving-path regression and tests/webui_settings_layout.spec.js for the card-containment regression.

Verification: python tests/test_webui_asset_versioning.py passed; npx playwright test ./tests/webui_settings_layout.spec.js --reporter=line passed 4/4; python evaluate.py --quick --no-color passed with only the existing ESP abstraction baseline warning; python build.py --firmware --target all --no-merged --no-zip --no-color built ESP8266 and ESP32 successfully.

2026-05-29 field re-open: user reports the Settings overflow is still present on OTGW 2.0.0-alpha.98+8633148, and the provided HTML already has versioned CSS links (?v=8633148). That disproves the stale stylesheet-cache-only root cause from the previous closure. Current investigation points at normalizeSettingsLabelWidth() using one page-level --settings-label-w across all settings groups: a long real Settings label from a lower group (for example SAT manufacturer/options) can set a huge global label width, which then stretches System/Network/MQTT rows in the screenshot. Existing Playwright coverage used only short labels, so it missed the field failure.

2026-05-29 fix implemented: live read-only Playwright measurement against http://192.168.88.64 reproduced the screenshot exactly. CSS.supports(subgrid)=true, #settingsPage had page-level --settings-label-w=1573.390625px, and the System card grid resolved to 1573.39px 19px with rows right edge at 1621 while the card right edge was 536. A probe measurement identified the source label as SAT Manufacturer (...) at 1573px natural width. The cache hypothesis was not sufficient because the live page already used versioned CSS links.

Changed normalizeSettingsLabelWidth() to clear the page-level variable and set --settings-label-w per .settings-group-body, measuring only direct row labels for that group and capping the label column to 45% of the card body with a 128px lower bound. Updated components.css to cap grid columns, scope settings row subgrid styles to direct children, allow long labels to wrap, keep fixed-IP rows contained, and keep the WiFi scan panel full-width without participating in label sizing. Expanded tests/webui_settings_layout.spec.js with live-like DHCP rows, the long SAT Manufacturer label, and scrollWidth containment checks so the reported failure mode is covered.

Verification: npx playwright test ./tests/webui_settings_layout.spec.js --reporter=line passed 4/4; python tests/test_webui_asset_versioning.py passed; python evaluate.py --quick --no-color passed with 0 failures and the existing ESP abstraction baseline warning; python tests/test_evaluate.py passed 47 tests; python tests/test_build.py passed 9 tests. Build verification: python build.py --target all --no-merged --no-zip --no-color built ESP8266 firmware/filesystem successfully, then ESP32 failed from a stale/orphaned PlatformIO .pio build lock (.sconsign/dependency file missing). After stopping the orphaned build processes and removing only the repo-local .pio/build cache, python build.py --target esp32 --no-merged --no-zip --no-color built ESP32 firmware/filesystem successfully.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Per-group settings label-width measurement with hidden clone probe per real .settingDiv label (skips fixed-ip section); width capped at 45% of available group width. components.css pins grid-template-columns to minmax(0, min(var(--settings-label-w), 45%)) and constrains field/input containers incl. dark theme. User-verified fix. Committed 28c2b6f4 (alpha.101).
<!-- SECTION:FINAL_SUMMARY:END -->
