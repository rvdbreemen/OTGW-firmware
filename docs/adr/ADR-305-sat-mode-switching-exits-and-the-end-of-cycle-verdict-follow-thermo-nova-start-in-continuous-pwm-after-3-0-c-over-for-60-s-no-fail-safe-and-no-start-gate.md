---
id: "ADR-305"
title: "SAT mode switching, exits and the end-of-cycle verdict follow thermo-nova: start in continuous, PWM after 3.0 C over for 60 s, no fail-safe and no start gate"
status: "Proposed"
date: "2026-11-28"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "sat"
  - "pwm"
  - "control-mode"
  - "thermo-nova"
aliases:
  - "start in continuous"
  - "no flame-out fail-safe"
  - "no start gate"
  - "plain thermo-nova switching"
components:
  - "SAT control mode switching"
symbols:
  - "satCycleCheckOvershootSwitch"
  - "satCycleCheckPwmExits"
  - "satCycleApplyEndOfCycle"
  - "satPwmFirstStart"
  - "satStartGateActive"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-305 SAT mode switching, exits and the end-of-cycle verdict follow thermo-nova: start in continuous, PWM after 3.0 C over for 60 s, no fail-safe and no start gate

## Status

Proposed, 2026-11-28. Guideline-level (ADR-080) with a test gate: `tests/test_sat_overshoot_sustain.py` and `tests/test_sat_pwm_first_start.py` (see Verification). On acceptance it supersedes ADR-252 and ADR-296 and partly supersedes ADR-249, ADR-254, ADR-236, ADR-297 and ADR-251; the frontmatter `supersedes` field and the README lines of those ADRs are set then, not while Proposed.

## Status History

```yaml
status_history:
  - date: 2026-11-28
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: "Initial proposal. Decision maker: Robert van den Breemen (entries 518 and 519; entry 518 relayed by the coordinator, not seen by otgw-architect). Sergeant D answered A and No rules to keep (entries 517, 520). Robert accepts."
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) runs the boiler in one of two control modes. In **continuous** the boiler modulates on a setpoint. In **PWM** (pulse-width modulation) SAT switches the burner on and off in timed phases with the modulation held low. Terms used below: CH (central heating, the CH-enable bit SAT sends), API (the REST and MQTT interfaces), DM (Discord direct message), EN and NL (the English and Dutch manuals).

Over the past weeks the firmware gained rules on top of thermo-nova, the pinned SAT reference (`custom_components/sat` at 7beaaf5, ADR-196):

- every heating start enters PWM first (entries 385, 387; ADR-252);
- a 60 s start gate silences the PWM exits;
- a flame-out fail-safe switches back to PWM at 3.0 C over the request for 30 s (entries 288, 289, 359, 417, 429);
- one PWM entry per flame (entry 429, `_cycle_burnLeftPwm`);
- a 180 s flame gate on the underheat exit (entry 456, ADR-297);
- a switch margin of 2.0 C as a setting (`satpwmswitchmargin`, entry 407, ADR-254);
- an end-of-cycle verdict margin of 2.0 C (entries 15, 37).

Field runs show the extra rules make SAT switch more, not less. In the gruijter run (30.8 h), saturation and the fail-safe alternated 17 times. The coordinator's reading is that the loop starts in the start-in-PWM rule: a cold flow gives duty 1.00, then saturation after 300 s, continuous, an overshoot, and the fail-safe (`_comms/plan_sat_mode_switching_simplify_for_robert_2026-11-28.md` section 4; a reading, not yet measured).

Robert ordered option A of that plan on 2026-11-28: SAT switches as thermo-nova does, without the extra rules (entry 518, recorded by otgw-docs; source "coordinator chat 2026-11-28 08:27", no DM id). Sergeant D answered "My answer is A" (entry 517, #dev-sat-mqtt 1576136688189841540, 07:24:29 UTC, reply to 1576134798974390283; recorded by otgw-docs). Robert then moved the cleanup (plan step 3d) into the same beta: "Can you do the cleanup step 3d right away with the other work. I want a cleanup in one go" (entry 519, #dev-sat-mqtt 1576138809484116152). Asked which rules he wants to keep, Sergeant D answered "No rules to keep." and "also open the climate.py. Holds most of the informations" (entry 520, 1576138978237882527 and 1576139118776295435). Entries 517 to 520 were read on OTGW-sat-decisions origin/main 49287b0 in this session.

**What thermo-nova does.** All of the following was opened at 7beaaf5 in this session (HC = `heating_control.py`).

- **Each tick with heating demand runs, in this order** (HC:202-228):
  1. A request at or above the maximum setpoint disables PWM (HC:203-204).
  2. The overshoot enable (HC:206).
  3. `pwm.update()` (HC:211).
  4. The runtime exits (HC:217).
- **Without heating demand** PWM is disabled and the guards reset (HC:229-234). Every heating start therefore begins with PWM off, that is in continuous.
- **Enable** (HC:262-305): flow >= requested + `OVERSHOOT_MARGIN_CELSIUS` (3.0, `cycles/const.py:10`), held for `OVERSHOOT_SUSTAIN_SECONDS` (60, `const.py:21`). Any failing tick clears the timer. It does not enable:
  - when PWM is already on;
  - during hot water or the 300 s after it (`const.py:19`);
  - without heating demand, or with a request at or below `COLD_SETPOINT` (22, `const.py:34`);
  - with an unknown flow.
- **Runtime exits** (HC:307-378). They run only with PWM enabled, in the PWM ON phase, with heating demand, and outside hot water and its 300 s:
  - underheat: flow <= requested + `UNDERSHOOT_MARGIN_CELSIUS` (-3.0, `cycles/const.py:11`) for `UNDERHEAT_SUSTAIN_SECONDS` (180, `const.py:22`), HC:331-356;
  - saturation: off time 0 for `SATURATION_SUSTAIN_SECONDS` (300, `const.py:23`), HC:358-378.
- **End-of-cycle verdict** (`pwm.py:219-227`, subscribed at HC:167). A continuous cycle classified OVERSHOOT enables PWM. A PWM cycle classified UNDERHEAT disables it. The classifier uses p90 >= 3.0 (`cycles/classifier.py:74`) and p50 <= -3.0 (`:75`).
- **Restore after a restart.** A saved PWM-enabled flag is restored (`pwm.py:111-115`, HC:181-184). A heater that is off at the first tick clears it again (HC:230).
- **`climate.py`** (Sergeant D asked for it, entry 520):
  - The loop calls `update()` on every run. It skips only without an outside temperature or with HVAC off (`climate.py:544-545`). There is no hot-water branch, so the PWM clock keeps running during a draw. Only the enable and exit guards pause for hot water (HC:268-274, `:313-315`).
  - A restart restores the saved PWM flag at once (`climate.py:169-176`, `pwm.py:111-115`).
  - A change of HVAC mode or target resets only the PIDs, never PWM (`climate.py:461-464`, `:583`, `:660`).
  - There is no mode command: the presets are temperature presets (`climate.py:604-640`, as read by otgw-poc-dev; not opened by me).
- **Heater off** (`heating_control.py:64-72`, `:229-238`). No HEAT mode, closed valves or a request at or below 22 C disable PWM on that tick and send setpoint 10.0 with CH off. There is no 180 s delay.
- **What thermo-nova does not have:**
  - a start in PWM or a start gate;
  - a fail-safe or a per-flame lock;
  - a flame gate on the underheat exit;
  - a minimum dwell or hysteresis beyond the 3.0 C margins and the 60/180/300 s holds.

## Decision

SAT decides between continuous and PWM exactly as thermo-nova 7beaaf5 does. The firmware's extra mode rules are removed.

1. **Start.** Every heating start runs in continuous. PWM comes only through rule 2 or the end-of-cycle verdict.
2. **Continuous to PWM.** Flow >= requested setpoint + 3.0 C, held for 60 s, with thermo-nova's gates (HC:262-305): PWM not yet on; outside hot water and the 300 s after it; heating demand, with the request above the cold setpoint; a known flow. The margin is a fixed 3.0. The setting `satpwmswitchmargin` no longer drives the switch.
3. **PWM to continuous.** These exits run only in the PWM ON phase, with heating demand, and outside hot water and its 300 s:
   - **underheat:** flow <= requested - 3.0 C for 180 s;
   - **saturation:** no off time for 300 s.

   Two more ways out:
   - **at the maximum:** a request at or above the maximum setpoint ends PWM on that tick (HC:203-204, entry 463), now without the 60 s start gate;
   - **heater off:** ends PWM as before (ADR-199).
4. **End-of-cycle verdict** (`pwm.py:219-227`). A continuous cycle classified OVERSHOOT enables PWM. A PWM cycle classified UNDERHEAT disables it. The classifier's overshoot margin for this verdict is 3.0 (`cycles/classifier.py:74`), not 2.0.
5. **Removed:**
   - the start in PWM (`satPwmFirstStart()`, `SATcontrol.ino:4400`);
   - the start gate (`satStartGateActive()`, `SATcontrol.ino:980`, `SAT_START_GATE_MS` `:973`);
   - the flame-out fail-safe (`SATcycles.ino:41-42` and its branch in `satCycleCheckOvershootSwitch()`, `:1408`);
   - the one-PWM-entry-per-flame lock (`_cycle_burnLeftPwm`, `SATcycles.ino:189`, set at `:1511` and `:1528`);
   - the 180 s flame gate on the underheat exit (`SAT_UNDERHEAT_FLAME_MIN_MS`, `SATcycles.ino:51`).
6. **Kept as they are:**
   - The restore of a saved PWM flag after a restart, which thermo-nova also does (entry 121; `pwm.py:111-115`).
   - The tolerance band (ADR-250, ADR-253).
   - The start-up setpoint sequence of entries 344, 347 and 348: return + 18 while the flame is off, a 20 s hold, then flow - 1.0. That is setpoint logic, not the choice of mode, and this ADR does not decide it.

7. **Cleanup in the same beta** (plan step 3d, entry 519: "I want a cleanup in one go"; Sergeant D, entry 520: "No rules to keep."):
   - **No kept PWM ON time.** Every PWM exit resets the PWM state, as `pwm.disable()` calls `reset()` (`pwm.py:87-100`, `:106-109`). The freeze and resume of ADR-296 go. The 180 s minimum flame-on hold inside an ON phase stays, timed from the flame-on that ON phase sees (`pwm.py:200-202`).
   - **No hot-water pause of the PWM clock.** The control loop runs during a draw. `climate.py:544-545` skips it only without an outside temperature or with HVAC off, and `climate.py` has no hot-water reference. `pwm.update()` has no hot-water branch in its ON/OFF timing (`pwm.py:154-214`). The only hot-water check is in the low-duty case (`pwm.py:248`). The enable and exit guards keep their own hot-water pause and the 300 s after it (HC:268-274, `:313-315`).
   - **The cold cutoff acts at once.** Without heating demand, PWM is disabled on that tick (HC:229-234). There is no 180 s wait before a cold cutoff ends an ON phase.
   - **The `control_mode` command** (auto, continuous, PWM) is ignored in this beta and writes a log line. This keeps Home Assistant automations from breaking. It is removed in a later release (entry 519, interpretation (a)). The OFF state (SAT off, safety trip, lost OpenTherm link) stays.
   - **The MsgID 3 veto code** goes. ADR-251 already makes it allow everything, so behaviour does not change. The MsgID 3 reply stays recorded and shown.

**Kept despite the cleanup** (not extra mode rules; entry 520 is read as not covering them, interpretation ours):

- The tolerance band's 180 s minimum burn (entries 275 and 376, ADR-250). The cold cutoff and the band share `satPwmColdCutWaitsForMinBurn()` today. The implementer keeps the band's use when the cold cutoff stops using it.
- The hourly start limit, MM (TASK-3143), the heating-failure window and the self-tune.

## Decision Contract

### Must

- Start every heating start in continuous mode.
- Switch to PWM only in one of two ways:
  - flow >= requested + 3.0 C, held 60 s, with thermo-nova's gates;
  - a continuous cycle with an OVERSHOOT verdict at a margin of 3.0 C.
- Leave PWM only in one of these ways:
  - underheat: flow <= requested - 3.0 C for 180 s, in the PWM ON phase;
  - saturation: 300 s without off time;
  - a request at or above the maximum setpoint;
  - heater off;
  - a PWM cycle with an UNDERHEAT verdict.
- Allow a PWM re-entry inside the same flame when rule 2 holds again.
- Count the underheat 180 s from the first qualifying tick in the PWM ON phase, with no wait for the flame's age.
- Reset the PWM ON time on every PWM exit.
- Keep the PWM ON/OFF clock running during a hot-water draw.
- End PWM on the tick the cold cutoff (no heating demand) comes.
- Answer a `control_mode` write with a log line and no mode change.

### Must Not

- Enter PWM at a heating start, at boot or when SAT is switched on. The only exception is the restore of a saved PWM flag.
- Keep any of these: a start gate, a flame-out fail-safe, a per-flame PWM lock, or a flame-age gate on the underheat exit.
- Use the setting `satpwmswitchmargin`, or a classifier margin of 2.0, for a mode decision.
- Add a minimum dwell or hysteresis that thermo-nova does not have. If one is wanted, plan option C is a new decision for Robert.

### Exceptions

- The 60 s boot silence of entry 388 still comes first: no mode logic runs before it ends.
- Removing the REST, MQTT and settings field `satpwmswitchmargin` (plan step 3c) is an API change. It may first ship as an ignored write; that choice is the implementer's, stated in the task.

### Verification

- **Host test `tests/test_sat_overshoot_sustain.py`**, written by otgw-poc-dev. It compiles the real `satCycleCheckOvershootSwitch()` and `satCycleCheckPwmExits()`. Every case is red on origin/dev before the change and green after:

  | Case | Expected |
  |---|---|
  | Flow 3.0 C over for 30 s | no switch (the fail-safe is gone) |
  | Flow 2.9 C over, held | never switches |
  | Flow 3.0 C over for 60 s | switches (today it already switches at 2.0 C) |
  | Flow 3.0 C under, from ignition | exit 180 s after the first qualifying tick (today about 360 s, because of the flame gate) |
  | Re-entry in the same flame after an exit | allowed |
  | At the maximum setpoint | ends PWM at once, with no 60 s start gate |

- **Host test `tests/test_sat_pwm_first_start.py`**, inverted. An OFF to ON heating start stays in continuous for every requested control mode. Red on origin/dev.
- **End-of-cycle verdict**, `tests/test_sat_cycle_classifier.py`. A continuous cycle with p90 = 2.5 C does not enable PWM; one with 3.0 C does.
- **Replay before the code** (plan phase 1). `_replay_tools` on the gruijter run (30.8 h) and the indigo run (25 h): mode switches counted for the current firmware and for option A.
- **Cleanup cases.** `tests/test_sat_pwm_on_freeze.py` becomes "an exit and a re-entry in one flame start a fresh ON time". `tests/test_sat_pwm_anchors.py` and `tests/test_sat_pwm_frozen_flow_dhw.py` check that a draw during an ON phase does not stop the ON clock. `tests/test_sat_cold_cut_min_burn.py` checks that a cold cutoff ends PWM at once, while the band still waits 180 s. Each is red on origin/dev.
- **Bench.** A SAT simulation shows a heating start in continuous, and PWM only after a held 3.0 C overshoot.

## Alternatives Considered

- **B: A plus a changed or removed saturation exit.** Rejected for now. PWM at duty 1.00 with the modulation held at 0 changes the heat output. It is also a departure from thermo-nova that is not yet measured.
- **C: A plus a minimum dwell of N minutes in a mode.** Not chosen now. It is the only rule that guarantees no fast flipping, but thermo-nova does not have it, and N must be chosen. The plan's advice: decide after the phase 1 replay.
- **Remove only the fail-safe.** Rejected. Rule 2 would then do the same work 30 s later, so the loop can stay. The start in PWM is where the loop is read to begin.
- **Keep the current rules.** Rejected by Robert's order. The field runs show the flipping that the rules were meant to stop.

## Consequences

**Positive:**

- Fewer rules: one entry rule, three runtime exits and one verdict, each with a thermo-nova citation. The firmware becomes easier to check against the pin (ADR-196).
- A heating start no longer begins at duty 1.00, so the loop "start in PWM, then saturation, then fail-safe" has no entry point. This is a reading; the replay measures it.
- The firmware inventory (`_comms/firmware_mode_switch_rules_2026-11-28.md` section 2) lists the code that becomes dead: the start, gate, fail-safe, lock and flame-gate symbols and their narration lines.

**Negative:**

- **Cold-start warm-up.** Entry 387 gave two reasons for the start in PWM:
  - (1) SAT continues in PWM after a restart. The restore still covers this.
  - (2) A short warm-up for the boiler components, which means less thermal stress. This reason is given up.

  Mitigation: the start-up setpoint sequence of entry 348 stays. The bench and the replay look at the first minutes of a cold start.
- **The underheat exit can fire early.** Without the 180 s flame gate it can come about 180 s after ignition, while the flow is still cold (the reason for entry 456). Mitigation: saturation and underheat both come from thermo-nova, and the replay counts these exits.
- **A target step can still push the duty to 1.00 and saturate.** Thermo-nova does that too (plan section 4). Mitigation: option C stays open.
- **API.** `satpwmswitchmargin` (REST, MQTT `sat/pwm_switch_margin`, settings) loses its effect or goes away. Home Assistant automations that set it see no effect.
- **Tests and docs.**
  - These tests need a rewrite: `test_sat_slave_lowload.py`, `test_sat_overshoot_sustain.py`, `test_sat_pwm_first_start.py`, `test_sat_pwm_on_freeze.py`, `test_sat_pwm_override.py`.
  - These docs need new text: MANUAL 5.6, the EN and NL manual chapter 5, `docs/sat/firmware-sat-spec.md`, `docs/sat/sat-comparison.md`, `docs/api/MQTT.md`, `docs/api/openapi.yaml`, and `settingsHelp.h` with its regenerated gz header.
- **Shared setting.** `settings.sat.fOvershootMargin` (2.0) also feeds the overshoot-seconds count at `SATcycles.ino:1287`. This ADR changes only the verdict margin (`:961`). The implementer keeps that other use as it is, or names the change in the task.
- **Timing.** This is a big change to the control loop close to the stable release. Plan section 8 puts steps 3a-3c in their own beta, after the replay.

## Open Questions

- [ ] Entry 518 quotes Robert's order as the coordinator relayed it (coordinator chat, no Discord link). Robert confirms the quote at acceptance (Evidence gate).
- [ ] Entries 158 and 211 (the 180 s hold after a draw): the implementer of step 3d states per entry what stays as thermo-nova (`pwm.py:144-146`, `:200-202`) and what goes, before the code lands.

## Superseded and overturned

**ADRs.** For each one I read the Decision and the Contract sections in this session.

| ADR | Effect | Part |
|---|---|---|
| ADR-252 | superseded in full | the start in PWM and the start gate; its restore clause is kept as entry 121 |
| ADR-249 | partly superseded | the switch margin as a setting, and the fail-safe it kept |
| ADR-254 | partly superseded | the 2.0 C / 60 s margin and its migration, the 3.0 C / 30 s fail-safe, the per-flame lock (entry 429) |
| ADR-236 | partly superseded | only the line "the start gate of ADR-252 stays"; its 3.0 C / 180 s rule and its gates stay |
| ADR-297 | partly superseded | points 1, 2 and 4 (the 180 s flame gate and the kept start gate); point 3 (at-max) stays, without the gate |
| ADR-296 | superseded in full | the kept PWM ON time, its freeze and resume, and the minimum burn across flips (step 3d, entry 519) |
| ADR-251 | partly superseded | its code-level Must (`satPwmPermitted()` returns true) goes with the veto code; the behaviour and the recorded MsgID 3 reply stay |
| ADR-250, ADR-253 | unchanged | the band keeps CH off, leaves the PWM state as it is and keeps its 180 s minimum burn (entries 275 for the band, 376) |

**Register entries.** "Body" means I read the entry's own text; "heading" means I read only its title.

| Entry | What goes | Checked by |
|---|---|---|
| 385, 387 | every start in PWM first; reason 2 of entry 387 | body |
| 288, 289, 359 | the flame-out fail-safe (3.5 C / 30 s) | body |
| 417 | the fail-safe part; the "heating allowed" gate of entry 410 stays | body |
| 429 | both parts: the fail-safe at 3.0 C and the one-PWM-entry-per-flame lock | body |
| 428 | the behaviour reading (b) that entry 429 settled; the counting rule for the hourly limit is not decided here | body |
| 407 | the 2.0 C margin; the 60 s stays | body |
| 22 | the 2.0 C margin of the switch; its "cold start in continuous" agrees with this ADR | body (first lines) |
| 15, 37 | the end-of-cycle verdict margin of 2.0; the port of `on_cycle_end` stays | body |
| 456 | the 180 s flame gate on the underheat exit | body |
| 463 | only the 60 s start gate before the at-max exit; the at-max exit itself stays | body |
| 448, 410 | not overturned: these are thermo-nova's own rules | body |
| 475, 476, 478, 479 | the kept PWM ON time and the minimum burn across flips (rule 6; 477 is the KD change and is not touched) | body (Settled line) |
| 95 (hot-water part), 103, 132 | the hot-water pause and resume of the PWM ON clock (rule 7) | body (Settled line) |
| 105, 106, 107, 234 | the hold of a PWM ON start while a hot-water flame burns, and its counting; thermo-nova has no such hold (`pwm.py:154-177`), so they go with rule 7 | body (Settled line); entry 105 itself names it a deviation from `pwm.py:160` |
| 158, 211 | the 180 s hold after a draw; thermo-nova times the hold from the flame-on its ON phase sees (`pwm.py:144-146`, `:200-202`). The implementer keeps what matches that and drops the draw-specific parts | body (Settled line) |
| 275 | for the cold cutoff only: the cutoff no longer waits 180 s (rule 8). For the band it stays (entry 376, ADR-250) | body (Settled line) |
| 31, 80, 260 | NOT overturned: the 180 s minimum flame-on in an ON phase (`pwm.py:200-202`) and "no PWM schedule while the cold cutoff holds" (HC:229-234) are thermo-nova's own | body (Settled line) |

## Related Decisions

- **ADR-196** (SAT ground truth): this ADR brings the firmware back to the pin.
- **Superseded in full or in part:** ADR-252, ADR-296, ADR-249, ADR-254, ADR-236, ADR-297, ADR-251 (see the table above).
- **Unchanged:** ADR-250, ADR-253, ADR-199 (see the table above).

## References

- **thermo-nova 7beaaf5** (https://github.com/Alexwijn/SAT/tree/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat), local copy `other-projects/SAT-releases-thermo-nova` (HEAD verified as 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb):
  - `heating_control.py:64-72`, `:167`, `:181-190`, `:197-240`, `:262-305`, `:307-378`;
  - `pwm.py:87-115`, `:144-146`, `:154-214`, `:219-227`, `:248`;
  - `climate.py:169-176`, `:461-464`, `:544-545`, `:583`, `:660`;
  - `cycles/const.py:10-11`, `cycles/classifier.py:74-75`;
  - `const.py:19-23`, `:34`.
- **Firmware at origin/dev 32761e62a:**
  - `SATcycles.ino:35`, `:41-43`, `:47`, `:51-52`, `:189`, `:961`, `:1037`, `:1287`, `:1408`, `:1486`, `:1511`, `:1528`;
  - `SATcontrol.ino:973`, `:980`, `:4400`, `:4419`;
  - `SATtypes.h:565-566`.
- **The plan and its research files**, in `OTGW-field-data/_comms/`: `plan_sat_mode_switching_simplify_for_robert_2026-11-28.md`, `thermonova_mode_switching_2026-11-28.md`, `firmware_mode_switch_rules_2026-11-28.md`, `pwm_switching_explained_2026-11-28.md`.
- **The register**, `sergeant-d-corrections.md`: OTGW-sat-decisions origin/main 49287b0, entries up to 520 (517 to 520 recorded by otgw-docs).
