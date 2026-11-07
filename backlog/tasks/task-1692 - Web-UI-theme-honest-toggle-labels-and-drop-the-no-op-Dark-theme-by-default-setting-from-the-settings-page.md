---
id: TASK-1692
title: >-
  Web UI theme: honest toggle labels and drop the no-op 'Dark theme by default'
  setting from the settings page
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T11:52:00Z'
updated_date: '2026-11-07T13:18:00Z'
labels:
  - webui
  - accessibility
dependencies: []
priority: low
ordinal: 810000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-07 (Robert asked which UI decisions about dark/light labels can be made). Facts: (1) the v2 theme comes only from the toggle (localStorage otgw-theme, per browser) and otherwise from the OS (prefers-color-scheme), v2.html:13-21; the setting darktheme (settings.bDarkTheme, restAPI.ino:4212) is not read by the v2 UI at all, only listed in SET_META (v2.js:2098 'Dark theme by default') with help 'Use the dark theme for the web UI.' (settingsHelp.h:66), so the setting promises something that does not happen. (2) The toggle shows the theme it switches TO ('☀ Light' while dark) and also sets aria-pressed; WAI-ARIA APG (Button pattern) says a toggle button's label must not change with its state, so a changing label plus aria-pressed tells a screen reader two different things. (3) Wording mixes 'mode' (aria-label 'Toggle dark mode') and 'theme' (setting, help). Decisions taken by Claude on Robert's request (he may override): D1 keep the action label ('☀ Light' / '🌙 Dark' = what a click gives), drop aria-pressed, set aria-label and title to 'Switch to light theme' / 'Switch to dark theme'. D2 say 'theme' everywhere. D3 hide darktheme from the v2 settings page; keep the key in the settings file and REST for compatibility (1.x import, the classic UI history); precedence is the toggle choice in this browser, else the OS setting. Not chosen: making darktheme work (needs the device to inject it before first paint or causes a flash of the wrong theme; YAGNI while the OS preference covers it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Toggle: no aria-pressed; aria-label and title name the action; visible label unchanged; web UI test asserts it
- [x] #2 darktheme no longer shown on the settings page; REST and settings file still carry it; the settings-help and SET_META tests pass
- [x] #3 'theme' used in every user-facing string about it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented D1-D3: v2.js applyTheme() keeps the action label, drops aria-pressed, sets aria-label and title 'Switch to light/dark theme'; v2.html button the same at first paint (was aria-label 'Toggle dark mode' aria-pressed). darktheme removed from SET_META and its settingsHelp.h entry, and added to SET_HIDE_ROW (a key without SET_META still renders under a humanized label via catFor(), so removing the meta alone was not enough; the hide list keeps it in setData). REST (restAPI.ino F("darktheme")) and the settings file (settingStuff.ino) still carry it.

Evidence: tests/webui/theme-toggle-labels.test.mjs (real v2.html/v2.js in headless Chrome): on the fix 4/4 pass (2 of 2 runs); with OTGW_DATA_DIR = origin/dev v2.js/v2.html: D1 (aria-pressed/labels), D3 (darktheme row found by opening every rail category) and D2 ('dark mode' in v2.html) fail, 2 of 2 runs; the REST/settings-file guard passes on both. settings-help.test.mjs 28/28; v2-navigation, v2-settings-dirty-survives-refetch, settings-error-toast pass; settings-reboot-badge timed out once under load (120 s), then 2/2 pass. evaluate.py --quick 0 FAIL. Build and OTA bench pending (one build with TASK-1686).
<!-- SECTION:NOTES:END -->
