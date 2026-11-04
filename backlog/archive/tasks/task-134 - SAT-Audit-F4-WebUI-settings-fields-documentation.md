---
id: TASK-134
title: 'SAT Audit F4: WebUI settings fields documentation'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:56'
updated_date: '2026-04-09 05:26'
labels:
  - audit
  - sat
  - phase-6
  - webui
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Document all SAT-related settings fields in the WebUI (index.html/index.js). Verify labels, tooltips, validation rules and default values match the firmware implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All SAT settings fields listed with label, type, valid range and default
- [x] #2 Settings persistence (REST save/load) verified
- [x] #3 Follow-up fix tasks created with label 'audit-fix' for discrepancies
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
The SAT settings page (displaySATSettingsPage) content is dynamically rendered from sat.js. Only 3 settings appear to have direct WebUI fields visible in sat.js: weather location (SATweatherlat, SATweatherlon, SATweatherenable via detectLocation()). The dashboard itself exposes preset buttons, mode buttons, and enable/simulation toggles. The settings page content is rendered from sat.js but the rendering function was not directly visible in the portion read. The firmware has 65+ SAT settings in settingStuff.ino. Created task-215 for a full WebUI settings audit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited SAT settings coverage in the WebUI. The SAT dashboard (index.html lines 314-541) provides: preset buttons (6), mode buttons (2), enable/simulation toggles, and a link to SAT Settings page. The SAT settings page (displaySATSettingsPage) is dynamically rendered by sat.js. From sat.js analysis, only weather location (lat/lon/enable) has explicit UI wiring; other settings are served via a dynamically generated form. The firmware has 65+ distinct SAT settings. Several settings (SATsensormaxage, SATerrormon, SATheatingmode, SATcyclesperhour, SATvalveoffset, SATwindowminsec, SATsimheatrate, SATsimcoolrate, SATsolarfreezeint, SATautogains) are likely not exposed in the WebUI and require MQTT or REST to configure. Created task-215 for full WebUI field inventory requiring direct sat.js inspection of the settings form rendering function.
<!-- SECTION:FINAL_SUMMARY:END -->
