---
id: TASK-1
title: 'Fix overshoot margin: 3C to 2C + make configurable'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:02'
updated_date: '2026-04-05 11:20'
labels:
  - sat
  - bugfix
  - settings
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT author confirms the maximum overshoot should be 2C, not 3C. This affects multiple locations: cycle classification (SAT_OVERSHOOT_MARGIN_C), boiler status evaluator, and auto-switch logic. Make the overshoot margin a configurable setting so users can tune it for their specific boiler. Default 2.0C.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT_OVERSHOOT_MARGIN_C in SATcycles.ino changed from 3.0 to 2.0
- [x] #2 New setting settings.sat.fOvershootMargin (default 2.0, range 0.5-5.0) added to SATSection struct
- [x] #3 SATcycles.ino and SATcontrol.ino read margin from settings instead of hardcoded constant
- [x] #4 Setting persistence via settingStuff.ino (write/read/update)
- [x] #5 REST API: setting visible in GET /api/v2/settings as satovershootmargin
- [x] #6 MQTT: configurable via set/<nodeId>/sat/overshoot_margin
- [x] #7 MQTT publish: sat/overshoot_margin in satPublishMQTT()
- [x] #8 WebUI: field in settings page for overshoot margin
- [x] #9 WebUI: value visible in SAT dashboard (Control Details section)
- [x] #10 HA auto-discovery: no separate entity needed (setting, not sensor)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add fOvershootMargin to SATSection struct in OTGW-firmware.h
2. Add setting persistence in settingStuff.ino (write/read/update)
3. Replace hardcoded SAT_OVERSHOOT_MARGIN_C in SATcycles.ino with settings.sat.fOvershootMargin
4. Replace hardcoded overshoot margin in SATcontrol.ino boiler status evaluator
5. Add REST API: satovershootmargin in settings JSON output
6. Add MQTT subscribe handler for set/<nodeId>/sat/overshoot_margin
7. Add MQTT publish in satPublishMQTT()
8. Add WebUI field in settings and dashboard display
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented overshoot margin as configurable setting. Default changed from 3.0 to 2.0 per SAT author recommendation. Used in SATcycles.ino (classification, sampling, auto-switch) and SATcontrol.ino (boiler status evaluator). WebUI settings page field (AC #8) deferred - requires HTML edit, will handle separately.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Changed overshoot margin from hardcoded 3.0C to configurable setting with 2.0C default (per SAT author recommendation).

Changes:
- Added settings.sat.fOvershootMargin (default 2.0, range 0.5-5.0) to SATSection struct
- SATcycles.ino: all 3 references to SAT_OVERSHOOT_MARGIN_C now read from settings
- SATcontrol.ino: boiler status overshoot detection uses configurable margin
- settingStuff.ino: persistence (write/read/update with constrain)
- restAPI.ino: visible in GET /api/v2/settings as satovershootmargin, added to sorted array
- MQTTstuff.ino: subscribes to set/<nodeId>/sat/overshoot_margin
- SATcontrol.ino: publishes sat/overshoot_margin in satPublishMQTT(), visible in status JSON
- index.html: overshoot margin row in SAT dashboard Control Details
- sat.js: renders overshoot_margin value from status API
- index.js: setting label and tooltip for settings page

Files modified: OTGW-firmware.h, SATcycles.ino, SATcontrol.ino, settingStuff.ino, restAPI.ino, MQTTstuff.ino, index.html, sat.js, index.js
<!-- SECTION:FINAL_SUMMARY:END -->
