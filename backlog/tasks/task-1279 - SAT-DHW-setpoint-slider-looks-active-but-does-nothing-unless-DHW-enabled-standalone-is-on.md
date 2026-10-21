---
id: TASK-1279
title: >-
  SAT DHW setpoint slider looks active but does nothing unless 'DHW enabled
  (standalone)' is on
status: To Do
assignee: []
created_date: '2026-10-21 09:31'
updated_date: '2026-10-21 09:33'
labels:
  - sat
  - ui
  - dhw
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 408000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-21 (#alpha-testing). The SAT card 'Hot water / DHW setpoint' slider only writes dhw_setpoint, which is used only when dhw_enabled ('DHW enabled (standalone)', in the general settings) is on; with it off (default) the slider has no effect and gives no hint. At the default 0 (= inactive) the thumb sits at the 40 C minimum while the label shows 0 C. Ranges disagree: slider 40-65 (or the boiler's MsgID 48 range), setting clamped 0-60 (settingStuff.ino ~1018), control code checks 30-70. Suggestion: hide the card unless dhw_enabled, or put the enable toggle on the card with a one-line explanation; show 'off' instead of 0 C; align the ranges.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The DHW card makes the standalone dependency visible (hidden, or with its own enable toggle and a one-line explanation)
- [ ] #2 An inactive setpoint (0) shows as off, not 0 C with the thumb at 40
- [ ] #3 Slider range, settings clamp and control-code range agree; a test checks the three
- [ ] #4 Screenshots before/after
<!-- AC:END -->
