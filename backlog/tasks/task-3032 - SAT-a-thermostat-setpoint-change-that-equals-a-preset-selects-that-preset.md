---
id: TASK-3032
title: 'SAT: a thermostat setpoint change that equals a preset selects that preset'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-22T09:14:00Z'
updated_date: '2026-11-26T20:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1141000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: entry 465 in the private SAT register (Sergeant D, 2026-11-22, amends entry 25), from gruijter's request (#alpha-testing 1573989200657653801: his Homey app always shows preset 'none'). Today satAdoptThermostatSetpoint() (SATcontrol.ino:2445-2468) clears the preset and skips matching on purpose; the MQTT/web UI path matches within 0.05 C (satHandleTargetTemp, SATcontrol.ino:3895-3925). Change: on a thermostat-driven target change, if the value equals a preset (within 0.05 C) select that preset; otherwise keep today's behaviour (preset cleared). A later 'preset off' must not restore an old pre-preset temperature over the thermostat's value (fPreCustomTemp handling).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Thermostat change to a preset value selects that preset (MQTT sat/preset and HA climate show it)
- [ ] #2 Thermostat change to a non-preset value still clears the preset
- [ ] #3 'Preset off' after a thermostat-driven match keeps the thermostat value (no stale restore)
- [ ] #4 Host test on the real satAdoptThermostatSetpoint() red on the old code, green on the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) Add one static helper satPresetForValue(float) in SATcontrol.ino (same preset table and 0.05 C tolerance as satHandleTargetTemp, SATcontrol.ino:4078-4105 on origin/dev) and let satHandleTargetTemp use it, so the MQTT/web path and the thermostat path cannot drift apart. 2) satAdoptThermostatSetpoint() (SATcontrol.ino:2498): keep the order (preset NONE, pre-custom 0, updateSetting SATtargettemp so the 5-30 C clamp, control reset and deferred persist stay), then if the adopted value matches a preset set eActivePreset to it, keep fPreCustomTemp 0 and the target at the thermostat value, publish the preset topic once (final state only, no NONE then preset flap); no match: today behaviour. 3) AC3 follows from pre-custom 0: preset off restores nothing (satHandlePreset none only restores when fPreCustomTemp>0, :2468) so the thermostat value stays; verified in the test, not assumed. 4) No extra persist code: satPresetStateFormat/periodic compare (:2416) writes the changed preset. TC= echo handling (satTCNoteAdopted) unchanged: no TC= back. 5) Tests red-first: extend tests/test_sat_follow_thermostat.py (it compiles the real satAdoptThermostatSetpoint) with a preset table in the harness: match selects preset (AC1), non-match clears (AC2), preset off after match keeps value (AC3), tolerance edges 0.04/0.06, two presets with equal value picks the first as satHandleTargetTemp does; change test_adoption_clears_preset_without_restoring_pre_custom_temp, which asserts the old behaviour for 23.0, to a non-preset value. Red on origin/dev code, green on the fix. 6) Entries read against the register (grep preset/setpoint/thermostat/target, 405 public + private to 487): 465 (the decision, amends 25 for the equal-value case only), 25 (clear on non-match stays), 2/14/16/169 (follow + push: no TC= back, unchanged), 223 (a preset chosen while the window is open wins: the window switch is ended by updateSetting, same as today), 308 (restore after reboot: the matched preset is persisted by the existing compare), 469+ none found with preset in the title. No conflict found; not yet read: private entries 469-500 bodies. 7) Open: gruijter wants Homey to show the preset; release 2.0.0 per Robert 2026-11-22. No build/push/flash; otgw-webui reviews before code lands.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22: Robert in #alpha-testing (1573992000523341935): put it in the backlog and prioritize for the 2.0.0 release.

Register check (coordinator request, 2026-11-26): all 32 private entries 469-500 (origin/main of the register, 5921af5) were scanned in full body text for preset, fPreCustom, satAdoptThermostat, thermostat setpoint, target temperature, setpoint change: 0 hit lines in every one. None touches presets or a thermostat setpoint change. This is a scripted keyword scan of the full bodies, not a line-by-line human read of all 32 entries. Entry 465 itself is the only one: it fixes the 0.05 C tolerance by reference ("within 0.05 C, as the MQTT/web UI path does, SATcontrol.ino:3895-3925 at origin/dev"), so the tolerance is Sergeant D's decision as recorded, taken from that path. Not fixed by the register or by entry 465: which preset wins when two presets have equal values (satHandleTargetTemp picks the first in code order comfort, eco, away, sleep, activity, home; the allowed ranges overlap, e.g. 15-22 C can hold comfort and eco): implementation choice (ours), same as the existing MQTT/web path, to be carried into the register later. Also ours: the target stays the thermostat value (within 0.05 C of the preset) instead of snapping to the preset value.
<!-- SECTION:NOTES:END -->
