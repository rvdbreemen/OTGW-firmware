# Thermo-nova pin 7beaaf5 versus upstream develop (TASK-1436)

Date: 2026-11-01. Facts come from a fresh treeless clone of https://github.com/Alexwijn/SAT.git (temporary, deleted afterwards)
and from reading both trees with `git show`. Paths are relative to `custom_components/sat`. "Pin" is
`7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (releases/thermo-nova HEAD). "Develop" is `origin/develop`
`7d43560342ca9d854e92b8b5a91bdfbf9cbc2152`.

## 1. Ancestry

Commands run in the clone (P = the pin sha):

```
$ git merge-base --is-ancestor P origin/develop; echo $?
1
$ git merge-base --is-ancestor origin/develop P; echo $?
0
$ git merge-base P origin/develop
7d43560342ca9d854e92b8b5a91bdfbf9cbc2152        (= develop HEAD)
$ git rev-list --count P..origin/develop
0
$ git rev-list --count origin/develop..P
294
$ git log -1 --format='%H %cI %s' origin/develop
7d43560342ca9d854e92b8b5a91bdfbf9cbc2152 2026-03-01T15:10:06+01:00 Make sure the coordinator is all setup during overshoot calculation
$ git log -1 --format='%H %cI %s' P
7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb 2026-03-18T21:23:38+01:00 Improve pressure sensor
```

A second `git fetch origin develop` at the end still gave develop = 7d43560342ca. `origin/releases/thermo-nova` = 7beaaf5.

Verdict:

- Entry 64 is right: develop is an ancestor of the pin, 0 commits ahead, 294 behind.
- Entry 66 is wrong: its interpretation ("the pin is an ancestor of develop", "develop 294 commits ahead") has the direction
  reversed. The pin contains all of develop plus 294 more commits (committer dates 2025-11-23 to 2026-03-18; dates are not
  monotonic along the history, only the ancestry is reliable).
- The thermo-nova line repeatedly merges develop into itself (for example a637d8f and 394caa7, both 2026-03-01, "Merge branch
  'public-develop' into releases/thermo-nova"; also 35c7cb3 and 2a49f7e in January). So develop is the older public line and
  thermo-nova is the newer line built on top of it.
- Consequence for Alexw's remark "this has been updated in the newer version, in the branch you mentioned": the PWM duty
  behaviour he describes (raw flow, flow + 10 reignition) exists in develop's code, but that is the OLDER code. In the pin it
  is gone. The "flow + 10" block ("Forcing setpoint up due to short flame cycles") was removed by c339502 (2025-11-23, "A lot of
  housekeeping, redesigned the dynamic setpoint controller ...", found with `git log -S"short flame cycles"`). The raw-flow
  update was replaced by the held `_effective_on_temperature` during the Dec 2025 / Jan 2026 PWM commits (for example fe9e35f,
  4caeb29; the exact commit was not bisected). Whether Alexw has a newer private tree than GitHub shows is not known; the
  public repo does not show it.

Direction warning for section 2: `git diff 7beaaf5 origin/develop` shows what develop has that the pin lacks, which is what
the pin removed or replaced. Moving the firmware "to develop" would mean going back in time on every point below.

## 2. Per-area differences

`git diff --stat 7beaaf5 origin/develop -- custom_components/sat`: 67 files changed, 4218 insertions, 7884 deletions. The pin
has a package layout (`cycles/`, `device/`, `coordinator/`, `heating_control.py`, `entry_data.py`, `types.py`, `solar_gain.py`,
`temperature/`); develop has flat modules (`flame.py`, `boiler.py`, `coordinator.py`, `minimum_setpoint.py`,
`overshoot_protection.py`, `relative_modulation.py`, `setpoint_adjuster.py`, `util.py`). These are different architectures,
not a small delta.

| Area | Change in develop (relative to the pin) | Pin file:line | Develop file:line | Likely effect on behaviour |
|---|---|---|---|---|
| PWM duty temperature | Develop keeps `_last_boiler_temperature`: EMA (alpha 0.2) while the PWM ON phase is younger than 180 s since the ON start, then the raw flow temperature on every update while status is ON, flame or not. Pin keeps `_effective_on_temperature`: set at flame ignition, EMA (0.3) only inside the first 180 s after flame-on, then held until the next ON. | pwm.py:130-132, 143-149, 166-167, 191, 232 | pwm.py:112-114, 124-132, 165, 180-186 | Develop: the duty follows the live flow temperature through the ON phase, so the duty falls as the boiler heats up. Pin: the duty uses the temperature learned at start and stays stable. Matches Alexw's entry 64 text for develop. |
| PWM reignition (flame + 10) | Develop: flame off and average flame-on time unknown or under 60 s: setpoint forced to flow + 10; flame on for more than 6 s: setpoint follows flow - 3 in 1 degree steps (`SetpointAdjuster`). Pin: no such block ("flow + 10" not found). Flame off in the ON phase: hold return + 18 (default); after 20 s of flame: flow - 1. | heating_control.py:392-408 (flame-off hold), 410-448 (suppression); const.py:151-153 | pwm.py:135-145; setpoint_adjuster.py:6-47 | Develop pushes the setpoint above flow to force re-ignition after short flame cycles. Pin uses a fixed flame-off hold and a post-ignition suppression. Different mechanism, different setpoint on the wire. |
| PWM ON/OFF state machine | Develop: ON when duty on-time is at least 180 s and (off time elapsed or IDLE); OFF when on-time elapsed; max cycles per hour. No wait for flame. Pin: the ON phase waits for ignition (`_waiting_for_flame_on`), applies a 180 s minimum flame-on hold, and syncs the timer to ignition. Develop also has an `automatic_duty_cycle` switch (off = linear on/off split); the pin removed it (commit 85c346e "Remove CONF_AUTOMATIC_DUTY_CYCLE"). | pwm.py:154-215 | pwm.py:157-173, 194-204 | Develop times the ON phase from the command, the pin from the actual flame. With slow ignition the pin does not use up its ON budget while the flame is out. |
| PWM enable/disable policy | Develop enables PWM on `BoilerStatus.OVERSHOOT_HANDLING`, or when the configured minimum setpoint exceeds the calculated setpoint (dynamic minimum v1), or when forced; disables on COOLING_DOWN with requested above the adjusted setpoint. Pin: enable after flow at or above requested + 3 for 60 s with flame on (not DHW, not within 300 s of DHW off, requested above 22); disable on sustained underheat or 300 s saturation; also enable/disable from the classified last cycle. | heating_control.py:262-378; pwm.py:219-227; cycles/const.py:10-11; const.py:19,21,23,34 | climate.py:553-568, 947-956; pwm.py:147-154 | Different entry and exit conditions. Develop reacts to a boiler status, the pin to measured flow versus request over time. |
| PID | Develop `pid.py` has gain versions 1, 2, 3; time-gated integral (minimum 300 s between updates, clamped to the heating curve value); derivative with a double low-pass (0.8, 0.6) and no cap. Pin has only the version 3 formulas; integral reset to 0 while the error is above `DEADBAND`, accumulated every `PID_UPDATE_INTERVAL` otherwise; derivative alpha = dt/(interval + dt) with `DERIVATIVE_RAW_CAP`. The pin output includes the heating curve value; develop output is P+I+D only and climate adds the curve. Version 3 gains are identical on both sides (kp = coefficient x curve / 3 or 4, ki = kp/8400, kd = 0.07 x 8400 x kp). | pid.py:62-93, 127-132, 185-232 | pid.py:84-203, 226-336; climate.py:511 | Same v3 gain formulas but different integral and derivative update rules. Derivative and integral timing not compared line by line. |
| Heating curve | Same curve formula (`4*(t-20) + 0.03*(o-20)^2 - 0.4*(o-20)`). Develop adds autotune of an optimal coefficient (fuzzy steps, 5-sample mean) and a base offset of 20 (underfloor) / 27.2 (radiators). The pin takes the base offset from `HeatingSystem` in types.py (value not re-read here) and has no autotune. | heating_curve.py (whole file); types.py | heating_curve.py (whole file) | Develop can adjust the coefficient by itself; the calling side was not checked. |
| Cycles / classifier | Pin has `cycles/` (tracker, history, classifier) with GOOD, UNDERHEAT, OVERSHOOT, SHORT_CYCLING, UNCERTAIN, a 600 s minimum and p50/p90 flow-error tests against 3 C margins. Develop has no classifier; `flame.py` tracks flame statistics (cycles last hour, 15 min duty ratio, 4 h median). | cycles/classifier.py:15-90; cycles/const.py:3-20 | flame.py (363 lines, not read in full); coordinator.py:205-218 | The pin's last-cycle feedback (enable PWM after OVERSHOOT, disable after UNDERHEAT) has no develop equivalent. |
| Setpoint path (heating_control vs climate) | Pin `heating_control.py` continuous mode: the setpoint follows the request but not below flow minus `flow_setpoint_offset` (2.0) while the flame is on. Develop does this inside `climate.py`: PWM ON uses `minimum_setpoint_value`, PWM OFF uses 10, no PWM uses the calculated setpoint; the calculated setpoint is low-filtered with alpha 0.2. | heating_control.py:450-508 | climate.py:803-836, 940-945, 584-594 | Develop has no flow-based clamp in continuous mode and filters the setpoint. The pin clamps to flow minus offset; `_alpha` is set at climate.py:110 but no use was found by grep. |
| Dynamic minimum setpoint, relative modulation | Develop has `minimum_setpoint.py` (learned minimum from modulation level and flow, stored), `relative_modulation.py` (state machine) and `overshoot_protection.py` (measured overshoot value). Pin: relative modulation is a two-state value inside `heating_control.py`; no dynamic minimum. | heating_control.py:510-523 | minimum_setpoint.py:37-87; relative_modulation.py:19-48; overshoot_protection.py | Develop adapts the PWM-ON minimum setpoint over time; the pin uses a constant. |
| Boiler status | Develop `device_status`: HOT_WATER, PREHEATING, HEATING_UP, AT_SETPOINT, COOLING_DOWN, ANTI_CYCLING, NEAR_SETPOINT, PUMP_STARTING, WAITING_FOR_FLAME, OVERSHOOT_HANDLING, OVERSHOOT_STABILIZED, from flow versus setpoint with `BOILER_DEADBAND` 2 and a temperature tracker. Pin `DeviceStatusEvaluator`: 17 states (IGNITION_SURGE, OVERSHOOT_COOLING, STALLED_IGNITION, ANTI_CYCLING by minimum off time, ...). | device/status.py:34-116; types.py:23-49 | coordinator.py:133-186; boiler.py:33-142; const.py:193-205 | Develop's status drives PWM enabling directly; in the pin PWM enabling does not use it (heating_control.py:262-305). The two state vocabularies do not map by name. |
| Presets, window sensors | Same behaviour class: window open for `window_minimum_open_time` switches to PRESET_ACTIVITY, closing restores the old target. Preset default temperatures are equal (10, 10, 18, 15, 20). No material difference seen; not compared further. | climate.py:604-664, 743-748; const.py:172-178 | climate.py:722-754, 104-108; const.py:144-150 | Likely none. |
| Config defaults | Develop `COLD_SETPOINT` 28.2, pin 22. Develop adds `AUTOMATIC_DUTY_CYCLE`, `DYNAMIC_MINIMUM_SETPOINT(_VERSION)`, `MINIMUM_SETPOINT_ADJUSTMENT_FACTOR` 0.2, `PID_CONTROLLER_VERSION` 3, `SAMPLE_TIME`. Pin adds solar gain options, `FLAME_OFF_SETPOINT_OFFSET` 18, `MODULATION_SUPPRESSION_DELAY` 20, `..._OFFSET` 1.0, `FLOW_SETPOINT_OFFSET` 2.0 and boiler pressure limits. `CYCLES_PER_HOUR` 4 and `DUTY_CYCLE` 13 min are equal. | const.py:34, 112-185 | const.py:25, 100-160 | COLD_SETPOINT 28.2 versus 22 moves the "too cold, send 10" cut-off (develop climate.py:820) and the heater ON threshold (climate.py:981). |
| Coordinator / OpenTherm | Develop: one `coordinator.py` plus `esphome/`, `mqtt/`, `serial/`, `switch/`, `simulator/` at the top level; no `otthing.py`. Pin: `coordinator/` package incl. `mqtt/otthing.py`, `esphome/__init__.py`, and a `device/` tracker. | coordinator/__init__.py; device/__init__.py | coordinator.py:428-431; esphome/__init__.py | Structural. Not compared function by function. |
| Only in the pin | Solar gain compensation (`solar_gain.py`), area/secondary PID overshoot cap, pressure sensor health, el.json translation. | solar_gain.py; climate.py:381-394; const.py:157-161 | none | Develop lacks these entirely. |

Limits: `flame.py`, `area.py`, `config_flow.py` and the coordinator/OpenTherm modules were not read in full; only the diff stat
and the greps above were used for them. Rows marked "not compared" are open, not "no difference".

## 3. Points that would need a Sergeant D decision if the pin moved or the port followed develop

One question per point. A is the stay-on-the-pin answer.

1. Duty temperature. Which temperature should the PWM duty use during the ON phase?
   A: the temperature learned at ignition, frozen after 180 s (pin, pwm.py:130-149, 232).
   B: the live flow temperature after the 180 s start-up filter (develop, pwm.py:124-132).
2. Reignition. When the flame goes out in PWM, how should the port get it back?
   A: hold return + 18 during flame-off, then flow - 1 after 20 s of flame (pin, heating_control.py:392-448).
   B: force the setpoint to flow + 10 when the flame is off and the average flame time is under 60 s, then follow flow - 3 (develop, pwm.py:139-145).
3. PWM ON timing. Should the ON phase wait for the real ignition and keep a 180 s minimum flame-on hold?
   A: yes (pin, pwm.py:154-203). B: no, the ON timer starts at the command (develop, pwm.py:157-173).
4. When PWM starts.
   A: sustained overshoot (flow at least requested + 3 for 60 s) with a 300 s DHW guard (pin, heating_control.py:262-305).
   B: boiler status OVERSHOOT_HANDLING or configured minimum above the calculated setpoint (develop, climate.py:553-568, 947-950).
5. Minimum setpoint in the ON phase. A: constant (pin). B: learned from return temperature and modulation (develop, minimum_setpoint.py:37-87).
6. Setpoint smoothing. A: no 0.2 low-pass on the calculated setpoint (pin, no use found in climate.py). B: low-pass with alpha 0.2 (develop, climate.py:945).
7. COLD_SETPOINT. A: 22 (pin, const.py:34). B: 28.2 (develop, const.py:25).
8. Integral and derivative rule. A: pin (integral reset outside the dead band, derivative alpha from dt, capped) (pid.py:185-232). B: develop (time-gated integral clamped to the curve, fixed double low-pass) (pid.py:84-203). The v3 gains are the same.
9. Cycle feedback. A: keep the cycle classifier that enables PWM after OVERSHOOT and disables it after UNDERHEAT (pin, cycles/classifier.py:15-90, pwm.py:219-227). B: no classifier (develop).
10. The pin itself. A: stay on 7beaaf5, which contains all of develop; read Alexw's statements (entries 64, 65) as describing older develop code. B: pin a newer thermo-nova commit once Alexw names one. Develop is not a candidate: it is an ancestor of the pin, so "moving to develop" means going back 294 commits. Entry 66's "Open" should be restated on this basis; this file does not edit the corrections file.
