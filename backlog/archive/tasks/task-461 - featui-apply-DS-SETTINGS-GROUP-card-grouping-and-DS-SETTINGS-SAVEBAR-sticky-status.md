---
id: TASK-461
title: >-
  feat(ui): apply DS:SETTINGS-GROUP card grouping and DS:SETTINGS-SAVEBAR sticky
  status
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 23:25'
updated_date: '2026-04-27 23:31'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Apply the two optional design-system additions from otgw_design_package/templates/settings.html.template:

1. **DS:SETTINGS-GROUP** — Wrap related .settingDiv rows in a ds-card with a heading. Hardcoded mapping (option A from brainstorm): SETTINGS_GROUPS array maps key prefixes to group ids/titles (System, MQTT, Time/NTP, Behavior, UI, Sensors, S0, OTGW, Outputs, Webhook, Other catch-all). Helpers getSettingsGroupId() and getOrCreateSettingsGroup() keep the change minimal in refreshSettings() — the rowDiv append target switches from #settingsPage directly to a per-group .settings-group-body inside a section.ds-card.settings-group.

2. **DS:SETTINGS-SAVEBAR** — Repurpose existing #settingMessage as a sticky bottom status bar (option D from brainstorm — no save-flow behaviour change). saveSettings() pre-counts changed fields and seeds the bar with "Saving N change(s)..."; sendPostSetting() retains the per-response SUCCESS/FAILED text. CSS: position: sticky; bottom: 0; with #settingMessage:empty hiding the bar when idle.

No firmware changes; pure index.js + components.css.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SETTINGS_GROUPS taxonomy + getSettingsGroupId/getOrCreateSettingsGroup helpers added in index.js
- [x] #2 refreshSettings() appends rowDiv to per-group body instead of #settingsPage directly
- [x] #3 saveSettings() seeds #settingMessage with change count before per-field POSTs
- [x] #4 components.css adds .settings-group / .settings-group-title / .settings-group-body and #settingMessage sticky styles
- [x] #5 Build succeeds (no JS or CSS regressions)
- [ ] #6 Settings page visually shows grouped cards with headings and sticky bottom save status
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DS:SETTINGS-GROUP and DS:SETTINGS-SAVEBAR applied to the basic settings page.

**index.js**:
- Added SETTINGS_GROUPS taxonomy mapping key prefixes to ten group ids (system, mqtt, ntp, behavior, ui, sensors, s0, otgw, outputs, webhook) plus an 'other' catch-all
- Added getSettingsGroupId() and getOrCreateSettingsGroup() helpers
- refreshSettings(): rowDiv now appended to per-group .settings-group-body inside a section.ds-card.settings-group instead of #settingsPage directly
- saveSettings(): pre-counts changed fields and seeds #settingMessage with "Saving N change(s)..."; per-field "Saving changes..." overwrite removed since sendPostSetting() takes over

**components.css**:
- Added .settings-group / .settings-group-title / .settings-group-body rules
- #settingMessage styled as sticky bottom savebar with border-top, box-shadow, rounded top corners; #settingMessage:empty hides when idle

Build clean on ESP32 and ESP8266: 0 warnings, 0 errors. Hardware visual verification still required (AC #6 deferred to user).
<!-- SECTION:FINAL_SUMMARY:END -->
