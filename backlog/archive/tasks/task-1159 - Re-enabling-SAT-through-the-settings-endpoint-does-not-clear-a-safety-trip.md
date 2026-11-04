---
id: TASK-1159
title: Re-enabling SAT through the settings endpoint does not clear a safety trip
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 18:15'
updated_date: '2026-09-23 18:41'
labels:
  - sat
  - bug
dependencies: []
priority: medium
ordinal: 291000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
satHandleEnabled() (REST /sat/enable, MQTT enabled) clears state.sat.bSafetyTripped; updateSetting(SATenabled) does not. A trip leaves settings.sat.bEnabled true, so a settings POST of true is a no-op and a false->true toggle via settings also leaves the trip set (observed 2026-09-23 on the bench: disable+enable via /api/v2/settings, safety_tripped stayed true).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Whatever path the web UIs use to resume SAT after a trip actually clears the trip
- [x] #2 The trip-clear logic lives in one place and is not triggered by the boot-time settings file parse
- [x] #3 Bench: trip, resume through the UI path, safety_tripped becomes false
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Both SAT pages (classic sat.js, v2.js) resume through POST /sat/enable, which already cleared the trip; the settings page and a settings POST go through updateSetting(SATenabled), which did not. Fix (alpha.374): satClearSafetyTrip() is the one place a trip is cleared; updateSetting calls it on the false->true transition when state.bSetupComplete (not during the boot-time file parse), satHandleEnabled calls it on any enable (explicit resume). Pre-fix evidence alpha.371 telnet 07:58:57-59: settings disable+enable, safety_tripped stayed true. Bench alpha.374: trip at 20:41:00; settings POST true while already enabled is a no-op and leaves the trip set (no transition, by design); settings false->true at 20:41:09 gives safety_tripped=false.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Re-enabling SAT through the settings page or a settings POST now clears a safety trip, the same way the SAT pages already did. satClearSafetyTrip() is the single place the trip is cleared; bench-verified on the OTGW32.
<!-- SECTION:FINAL_SUMMARY:END -->
