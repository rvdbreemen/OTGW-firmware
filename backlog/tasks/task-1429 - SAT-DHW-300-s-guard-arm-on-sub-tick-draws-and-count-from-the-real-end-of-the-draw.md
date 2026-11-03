---
id: TASK-1429
title: >-
  SAT DHW 300 s guard: arm on sub-tick draws and count from the real end of the
  draw
status: Done
assignee: []
created_date: '2026-10-28T18:16:00Z'
updated_date: '2026-11-03T01:00:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 593000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1420 review. satCycleNoteDhwTick() arms the guard only on control ticks that see the boiler DHW-active bit: a draw shorter than one tick (seen only by satPwmNoteDhwFlameEdge) never arms it, and the guard counts from the last DHW tick instead of the real end of the draw, so it can lift up to one interval earlier than thermo-nova hot_water_off_since + 300 s (heating_control.py:272-274, :313-315; entry 108).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: a draw between two ticks arms the guard; the guard ends 300 s after the DHW bit dropped; red on HEAD, green on fix
- [x] #2 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: guard only armed by satCycleNoteDhwTick (SATcycles.ino:212), flame-edge path (SATcontrol.ino:1117) does not. Entry 108 decided. Agent-doable.

2026-11-03 (agent + lead integration): new satCycleNoteDhwBit(bool, ms) in SATcycles.ino, called every loop pass from satControlLoopRun before the timer gate; the pass that sees the DHW bit drop stamps the real end of the draw, so a draw between two ticks arms the guard and the 300 s counts from its end. Entries 108/131; thermo-nova device/__init__.py:223-224, :99-100, heating_control.py:272-274, :313-315, const.py:19. tests/test_sat_overshoot_sustain.py 46/46: test_sub_tick_draw_arms_the_guard (HEAD switches at 120000, fix 390000), test_guard_counts_from_the_real_end_of_the_draw (HEAD 400000, fix 430000), structural check. ot_loss/restart_quiet stubs added. AC2 build with the batch.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
beta.30 28117fb02: the DHW 300 s guard arms on a draw between two control ticks and counts from the real end of the draw (satCycleNoteDhwBit on every loop pass); Sergeant D entries 108/131, thermo-nova device/__init__.py:223-224, heating_control.py:272-274/:313-315. Evidence: tests/test_sat_overshoot_sustain.py 46/46 (two behavioural tests fail on old code); full tests 100/100; build beta.31 green. Not verified on hardware (a short draw on a real boiler).
<!-- SECTION:FINAL_SUMMARY:END -->
