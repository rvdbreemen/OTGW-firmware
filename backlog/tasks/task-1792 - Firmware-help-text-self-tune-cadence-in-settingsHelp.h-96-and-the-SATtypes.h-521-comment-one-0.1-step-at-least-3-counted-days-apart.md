---
id: TASK-1792
title: >-
  Firmware help text: self-tune cadence in settingsHelp.h:96 and the
  SATtypes.h:521 comment (one 0.1 step, at least 3 counted days apart)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T23:08:00Z'
updated_date: '2026-11-14T21:25:00Z'
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
- [x] #1 settingsHelp.h:96 states the real cadence (and every language pack that holds this help key)
- [x] #2 SATtypes.h:521 comment matches the code
- [ ] #3 Prerelease bump, esp32-combo build, the help text seen in the web UI on a bench unit (screenshot)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (92, lane wt-92-webui from origin/dev 753d45581). Code read: the decision runs once a day on a counted day (>= SAT_CT_MIN_CLEAN_HOURS 6 clean hours, SATcurveTune.h:11) when the window holds SAT_CT_WINDOW_DAYS 3 counted days (SATcurveTune.h:10); a change calls satTuneEmptyWindow(t) (SATcontrol.ino:6590, comment :6532 'A change empties the window (3 new counted days)'), so two steps are at least 3 counted days apart. Changed: settingsHelp.h satcurveselftune text and the SATtypes.h bCurveSelfTune comment (line 526 now, was 521). Language packs: none carry help texts; settings-help.json is English only, served from settingsHelp.h (FSexplorer.ino:203), so AC1 needs no translation. New test tests/test_selftune_help_cadence.py reads the real settingsHelp.h, SATtypes.h, SATcurveTune.h constants and satTuneCloseDay: OLD text 2 failed 1 passed, FIX 3 passed. node --test tests/webui/settings-help.test.mjs: 29 pass 0 fail. evaluate.py --quick: 85 passed, 0 failed, 100.0%. Bump beta.250 -> beta.251 (bin/bump-prerelease.sh). Open: AC3 build (coordinator schedules) + screenshot of the help text on a bench unit.
<!-- SECTION:NOTES:END -->
