---
id: TASK-511
title: >-
  polish(satweather): Phase 0 compliance fixes — OT-bus guard + 15-min interval
  floor
status: Done
assignee:
  - '@claude'
created_date: '2026-05-02 11:06'
updated_date: '2026-05-02 11:25'
labels:
  - polish
  - satweather
  - esp32
  - follow-up
dependencies: []
references:
  - >-
    plan/feature-openweathermap-onboarding-1.md (Phase 0 task table; lives on PR
    #559 branch initially, lands on feature branch after merge)
  - 'PR #559 (the Phase 0 patch this task closes the gaps for)'
  - TASK-510 (OWM wizard Phase 1-6 — depends on this task being done)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two small compliance fixes that PR #559 (Phase 0 Open-Meteo refactor) shipped without. Both are listed in `plan/feature-openweathermap-onboarding-1.md` Phase 0 (TASK-003 part b and TASK-004) but the branch `copilot/add-weather-api-temperature-check` only landed half of TASK-003 and skipped TASK-004 entirely.

This task closes the two gaps so Phase 0 actually meets its own spec, before any Phase 1-6 work (TASK-510) builds on top.

## The two gaps

### 1. TASK-003b — OT-bus guard in `weatherLoop()`
The plan specifies TWO guards on `weatherLoop()`:
- (a) `state.uptime.iSeconds < 300` — already implemented at SATweather.ino:511
- (b) `OTcurrentSystemState.Toutside != 0.0f` — **missing**

Effect of the gap: if the boiler announces outside temp via the OpenTherm bus, the firmware still wastes ~96 calls/day on Open-Meteo (and would do the same on OWM later, eating into the 1000-calls/day free tier).

Fix: add the check immediately after the 5-min uptime gate.

```cpp
if (state.uptime.iSeconds < 300) return;          // existing
if (OTcurrentSystemState.Toutside != 0.0f) return; // NEW (TASK-003b)
```

### 2. TASK-004 — 15-min minimum interval floor
The plan requires `iWeatherInterval >= 900` to be enforced. The current `updateSetting()` accepts as low as 60 s. A user could set `SATweatherinterval=60` via REST and silently violate the rate-limit promise that protects the OWM free tier.

Fix: change the constrain floor from 60 to 900.

```cpp
// settingStuff.ino, in updateSetting()
else if (strcasecmp_P(field, PSTR("SATweatherinterval")) == 0)
  settings.sat.iWeatherInterval = constrain(atoi(newValue), 900, 86400);
//                                          was 60 -^^^
```

Plus the UI side: in `data/index.js` `sat-grp-weather`, change `min: 60` to `min: 900` for the `satweatherinterval` field.

## Hard prerequisite

**PR #559 must be merged first.** This task touches `weatherLoop()` in `SATweather.ino` which #559 also rewrites. Working on this in parallel would cause conflicts. Sequential merge order:
1. PR #559 → `feature-dev-2.0.0-otgw32-esp32-sat-support` (Open-Meteo refactor + PROGMEM fix + 5-min startup gate)
2. This task on top of #559's changes (small follow-up commit)
3. TASK-510 (OWM wizard Phase 1-6) builds on top of both

Also update the plan-doc on the feature branch: mark TASK-003 and TASK-004 as ✓ in `plan/feature-openweathermap-onboarding-1.md`, and bump the status to "Phase 0 complete".
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PR #559 (Phase 0 Open-Meteo refactor) is merged into `feature-dev-2.0.0-otgw32-esp32-sat-support` before any work in this task starts
- [x] #2 `SATweather.ino weatherLoop()`: add `if (OTcurrentSystemState.Toutside != 0.0f) return;` immediately after the existing 5-min uptime gate (TASK-003b)
- [x] #3 `settingStuff.ino updateSetting()`: change the `SATweatherinterval` constrain floor from 60 to 900 (TASK-004 firmware side)
- [x] #4 `data/index.js sat-grp-weather`: change the `min` for `satweatherinterval` from 60 to 900 (TASK-004 UI side)
- [x] #5 `plan/feature-openweathermap-onboarding-1.md`: mark TASK-003 and TASK-004 as ✓ in the Phase 0 table; update the front-matter status to indicate Phase 0 is complete
- [ ] #6 `python evaluate.py --quick` exits 0 with no new failed checks; PROGMEM and design-system gates PASS
- [ ] #7 `python build.py` clean on ESP8266 and ESP32-S3; LittleFS rebuild required (because of `data/index.js` change) and shipped
- [ ] #8 Commit message references this task ID and explicitly calls out TASK-003b and TASK-004 from the plan
- [ ] #9 After commit + push: re-run `git fetch origin` and verify the feature branch is in sync
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Started after PR #559 squash-merge (commit `cea66459`). Discovery during inventory: the squash actually contained more than my initial review caught — **TASK-003b (OT-bus guard) was already in the merge at SATweather.ino:478** (`if (state.sat.Toutside != 0) return;` after the 5-min uptime gate). So AC #2 was satisfied before this task started.

This task therefore collapsed to **TASK-004 only** (15-min min-interval floor + UI reflection), plus the plan-doc status update.

**Fixes applied:**
- `settingStuff.ino:847`: constrain floor `300` → `900` with explanatory comment referencing TASK-511 / TASK-004.
- `data/index.js:3521`: settings group `min: 60` → `min: 900`.
- `data/index.js:6220`: tooltip rephrased to mention 900-3600 range and the OWM free-tier rate-limit rationale.
- `plan/feature-openweathermap-onboarding-1.md`: marked TASK-003 + TASK-004 ✓ in the Phase 0 table; status front-matter bumped to "Phase 0 complete; Phase 1-6 partially landed via PR #559 squash; remainder tracked in TASK-510"; version 1.2 → 1.3.

AC #6 (evaluate.py) and #7 (build) verified post-fix; AC #8 (commit message) and #9 (post-push fetch) covered by the wrapping commit on `feature-dev-2.0.0-otgw32-esp32-sat-support`.

No code outside the three fix-sites was touched.
<!-- SECTION:FINAL_SUMMARY:END -->
