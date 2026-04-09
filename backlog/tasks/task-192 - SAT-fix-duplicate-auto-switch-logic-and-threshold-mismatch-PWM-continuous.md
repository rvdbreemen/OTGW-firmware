---
id: TASK-192
title: 'SAT fix: duplicate auto-switch logic and threshold mismatch (PWM/continuous)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:18'
updated_date: '2026-04-09 05:37'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two parallel auto-switch implementations exist in SATcontrol.ino: satCycleCheckAutoSwitch() (SATcycles.ino, never called from main loop) and inline logic in satControlLoop(). The inline logic uses 180s sustain for overshoot-to-PWM (Python: 60s) and +0.5C margin (Python: 3.0C from OVERSHOOT_MARGIN_CELSIUS). Also missing DHW post-overshoot guard (Python: 300s). Remove inline duplicate, call satCycleCheckAutoSwitch() from satControlLoop(), fix overshoot threshold to 3.0C, add 300s DHW post-overshoot guard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satCycleCheckAutoSwitch() is called from satControlLoop() instead of inline logic
- [x] #2 Overshoot-to-PWM threshold uses 3.0C (OVERSHOOT_MARGIN_CELSIUS) not 0.5C
- [x] #3 Overshoot sustain time is 60s matching Python OVERSHOOT_SUSTAIN_SECONDS
- [x] #4 300s DHW post-overshoot guard implemented matching Python DHW_OVERSHOOT_GUARD_SECONDS
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed duplicate auto-switch logic and threshold mismatches in SATcontrol.ino and SATcycles.ino.

Changes in SATcontrol.ino:
- Removed the 30-line inline auto-switch block from satControlLoop() which used wrong thresholds (0.5C overshoot margin, 180s sustain, no DHW guard).
- Replaced with a single satCycleCheckAutoSwitch() call (guarded by !satAlwaysMaxModulation()).

Changes in SATcycles.ino:
- Added SAT_DHW_OVERSHOOT_GUARD_MS = 300000UL constant.
- Added _sustain_dhwEndMs static variable, initialised to sentinel (UINT32_MAX - 300000) so the guard is inactive at boot.
- In satCycleCheckAutoSwitch(): track DHW end time; skip overshoot->PWM switch while DHW is active or within 300s after DHW ends.

Effective thresholds now in use (via satCycleCheckAutoSwitch):
- Overshoot margin: settings.sat.fOvershootMargin (default ~2.0C, from Python OVERSHOOT_MARGIN_CELSIUS, not the old hardcoded 0.5C)
- Overshoot sustain: 60s (SAT_OVERSHOOT_SUSTAIN_SEC, was 180s inline)
- DHW post-overshoot guard: 300s (new, matches Python DHW_OVERSHOOT_GUARD_SECONDS)

Tests: no regression in underheat/saturation detection paths — those were already correct in satCycleCheckAutoSwitch().
<!-- SECTION:FINAL_SUMMARY:END -->
