---
id: TASK-1792
title: >-
  Firmware help text: self-tune cadence in settingsHelp.h:96 and the
  SATtypes.h:521 comment (one 0.1 step, at least 3 counted days apart)
status: To Do
assignee: []
created_date: '2026-11-10T23:08:00Z'
labels:
  - firmware
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 887000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1740.3 (docs side fixed there). The i-button help of satcurveselftune (src/OTGW-firmware/settingsHelp.h:96) says 'adjust the curve slope by 0.1, at most once a day, after 3 days of heating at low sun without hot water'; the comment at SATtypes.h:521 says 'by 0.1 at most once a day'. The code decides once a day, but a change empties the 3-counted-day window (SATcontrol.ino:6051-6053, 6082-6083; ADR-205 decision 3), so two steps are at least 3 counted days apart. Check the translated help texts in the language packs too, if they carry this key. Needs a prerelease bump and an esp32-combo build (web UI/firmware lane); not for the docs session.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 settingsHelp.h:96 states the real cadence (and every language pack that holds this help key)
- [ ] #2 SATtypes.h:521 comment matches the code
- [ ] #3 Prerelease bump, esp32-combo build, the help text seen in the web UI on a bench unit (screenshot)
<!-- AC:END -->
