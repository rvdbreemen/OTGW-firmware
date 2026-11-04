---
id: TASK-1279
title: >-
  SAT DHW setpoint slider looks active but does nothing unless 'DHW enabled
  (standalone)' is on
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 09:31
updated_date: '2026-10-22T05:11:00Z'
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
- [x] #1 The DHW card makes the standalone dependency visible (hidden, or with its own enable toggle and a one-line explanation)
- [x] #2 An inactive setpoint (0) shows as off, not 0 C with the thumb at 40
- [x] #3 Slider range, settings clamp and control-code range agree; a test checks the three
- [x] #4 Screenshots before/after
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Effective behaviour today: settings clamp 0..60 (settingStuff.ino:1016), SAT sends SW= only for 30..70 (SATcontrol.ino:4639; sim 3811), so the active range is already 30..60 and anything below 30 is inactive. Make that explicit without changing behaviour: (1) constants SAT_DHW_SETPOINT_MIN 30 / SAT_DHW_SETPOINT_MAX 60 in the SAT header; control and sim checks use them (70 -> 60 is unreachable today because of the clamp); settings clamp max uses the constant (0 stays allowed = off). (2) SAT status JSON (SATcontrol.ino ~2395) also carries dhw_enabled. (3) v2 Hot water card: new row 'Standalone DHW control' toggle bound to dhw_enabled (satPost settings/dhw_enabled, in-flight guard like satDhwEnPending), with a one-line hint 'SAT sends this setpoint to the boiler only while standalone DHW control is on.'; slider disabled while it is off. Slider default range 30..60, boiler MsgID 48 range intersected with 30..60; setpoint below 30 shows 'off' (thumb at min). Settings-page hint for satdhwsetpoint: 'C (below 30 = off, 30-60)'. (4) Test tests/test_sat_dhw_range.py: settings clamp max, control/sim checks, v2.html slider min/max and v2.js fallback all equal the constants; red on HEAD. (5) Evidence: Playwright screenshots of the card before/after on the bench (toggle off: slider disabled + hint; setpoint 0: 'off'). No SAT control behaviour change, so no Sergeant gate. MQTT/HA: no topic change (sat/dhw_enabled already published).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 RED (bench .88.61, alpha.454 firmware / alpha.452 filesystem, satdhwenabled=false, satdhwsetpoint=0, satdhwenable=true): SAT Control view Hot water card shows an enabled slider (min 40 / max 55 from the boiler MsgID 48 range) with the thumb at 40 and the label '0°C', and nothing about the standalone dependency. Evidence %TEMP%\bench-1279\red-card.png, settings-baseline.json.

2026-10-22 review (15 agents): code correct; fixed (1) ch03/h03 manuals still said 40-60 and a 'force switch' (2/2), (2) old firmware + new UI: slider now stays usable when the status has no dhw_enabled field (0/2 votes, fixed anyway: self-review checklist item), (3) source-substring test too weak (split): new tests/webui/sat-dhw-card.test.mjs runs the real satRenderDhw/fetchDhwBounds (6 tests; HEAD 4 fail; 5 mutants incl. inverted disable, dhw_enable instead of dhw_enabled, dropped intersection, off threshold: all red) and test_sat_dhw_range.py pins the whole SW= condition. Also fixed: tests/test_sat_mqtt_state.py broken by TASK-1287 (bug-904). GREEN (bench, alpha.454 build of this diff, firmware via safeboot + filesystem via /update?cmd=100, settings diff empty): Hot water card A off: toggle off, slider disabled, label 'off', hint shown; B click toggle in the browser -> device dhw_enabled true, slider enabled; C slider 50 -> device dhw_setpoint 50, label 50°C. Bench restored (dhwenabled false, setpoint 0; settings diff vs baseline empty). Gates: tests/webui 120/120, evaluate 72/0, all tests/test_sat_*.py + static_integrity + settings_whitelist OK, build.bat SUCCESS. Evidence %TEMP%\bench-1279\red-card.png vs green-off.png, green-on.png, green-on-50.png.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT Hot water card now shows that the DHW setpoint only works with standalone DHW control: a 'Standalone DHW control' toggle on the card (bound to dhw_enabled, now in the SAT status JSON), a one-line hint, and the slider greyed out while it is off. An inactive setpoint (below 30) shows 'off'. The range is named once as SAT_DHW_SETPOINT_MIN/MAX = 30/60 and used by the slider (cut to the boiler's MsgID 48 range), the settings clamp and the SW= condition; behaviour is unchanged because the clamp already capped at 60 and below 30 was already inactive. AC1/AC2/AC4: bench screenshots red vs green incl. a real toggle round trip. AC3: tests/test_sat_dhw_range.py (red on HEAD) + tests/webui/sat-dhw-card.test.mjs (behaviour, 5 mutants red). Not verified: Firefox/Safari; a boiler that reports a MsgID 48 range on the bench during the green run (covered by the fetch test, 40-55 seen in the red run).
<!-- SECTION:FINAL_SUMMARY:END -->
