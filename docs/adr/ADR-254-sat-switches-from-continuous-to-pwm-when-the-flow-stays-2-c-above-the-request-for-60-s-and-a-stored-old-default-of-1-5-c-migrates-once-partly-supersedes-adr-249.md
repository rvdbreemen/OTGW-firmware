---
id: "ADR-254"
title: "SAT switches from continuous to PWM when the flow stays 2 C above the request for 60 s, and a stored old default of 1.5 C migrates once (partly supersedes ADR-249)"
status: "Accepted"
date: "2026-11-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-249"
  - "ADR-251"
  - "ADR-252"
topics:
  - "sat"
  - "continuous to pwm switch"
  - "overshoot switch"
  - "pwm switch margin"
  - "settings migration"
aliases:
  - "entry 407"
  - "Q-407a"
  - "2 C for 60 s"
  - "satpwmswitchmargin"
  - "SATpwmmargin2"
components:
  - "SAT_OVERSHOOT_SUSTAIN_MS"
  - "SATpwmmargin2"
symbols:
  - "satCycleCheckOvershootSwitch"
  - "SAT_OVERSHOOT_SUSTAIN_MS"
  - "fPwmSwitchMargin"
  - "g_sawSatPwmMarginMarkerKey"
  - "kSatPwmMarginMarkerValue"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-254 SAT switches from continuous to PWM when the flow stays 2 C above the request for 60 s, and a stored old default of 1.5 C migrates once (partly supersedes ADR-249)

## Status

Accepted, 2026-11-25, by Robert van den Breemen (interactive in the coordinator session, relayed). Checked against the register before acceptance and aligned in place to Sergeant D's entries 410, 417 and 429 (fail-safe 3.0 C, the margin rule at most once per flame, heating allowed only); nothing new decided. Release 2.0.0 (entry 407, Q-407a).

## Status History

```yaml
status_history:
  - date: 2026-11-17
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-17
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-249
    changed_via: adr-kit lifecycle
  - date: 2026-11-17
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-251
    changed_via: adr-kit lifecycle
  - date: 2026-11-17
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-252
    changed_via: adr-kit lifecycle
  - date: 2026-11-25
    status: Accepted
    changed_by: "Agent: Claude (otgw-architect)"
    reason: "Accepted by Robert van den Breemen, 2026-11-25 (interactive in the coordinator session, relayed): checked against the register; aligned in place to entries 410, 417 and 429 before acceptance (fail-safe 3.0 C, once per flame), no new decision"
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) leaves continuous control for PWM (pulse-width modulation of the burner) when the boiler
flow stays above the PID (proportional-integral-derivative) requested setpoint by a margin for some time. ADR-249 set
that rule to flow - requested >= 1.5 C (the setting `satpwmswitchmargin`, default 1.5, range 0.5-5.0) for 30 s, beside
a flame-out fail-safe of 3.5 C for 30 s (entry 359).

On a real Viessmann Vitodens (indigo's capture 2026-11-16 16:17-16:39, TASK-2005) SAT flipped between continuous and
PWM every 2.5 minutes: the request sat on the 1.5 C threshold and the 30 s rule fired again soon after each PWM to
continuous exit. Sergeant D decided on 2026-11-17 (entry 407, Discord message 1572117476525867152): "change the
continuous to PWM from boiler temperature -requested temperature >= 1.5 for 30 seconds that it is now to Boiler
temperature - requested temperature >= 2 for 60 seconds". Robert (maintainer) answered the follow-up Q-407a in session:
2.0 becomes the new default of the existing setting (A), the 3.5 C / 30 s fail-safe stays (C), release 2.0.0.

Three later entries change the surroundings of this rule (Sergeant D):

- **Entry 410 (Q-1887c, answer B, 2026-11-18)** and **entry 417 (Q-410a, answer A):** both continuous to PWM rules, the margin rule and the fail-safe, only run while heating is allowed, as thermo-nova (`heating_control.py:276-278`). A tolerance-band pause counts as heating not allowed.
- **Entry 429 (Q-2005b, answer B, 2026-11-19):** "My answer is B. But instead of flow above 3.5 change it to flow above 3." During one flame SAT enters PWM at most once: after a PWM to continuous exit the 2.0 C / 60 s rule does not take that flame back to PWM; only the fail-safe can, and its threshold goes from 3.5 C to 3.0 C (30 s hold unchanged).

Constraint: `writeSettings()` writes `SATpwmswitchmargin` on every save since TASK-1888
(`src/OTGW-firmware/settingStuff.ino:382`), so practically every unit that ran a beta since then has 1.5 stored. A new
struct default alone would only reach fresh installs.

Thermo-nova (pinned commit 7beaaf5) waits OVERSHOOT_SUSTAIN_SECONDS = 60.0 (`const.py:21`, used at
`heating_control.py:296`) at OVERSHOOT_MARGIN_CELSIUS +3.0 (`cycles/const.py:10-11`, used at `heating_control.py:288`).

## Decision

SAT switches from continuous to PWM when flow - requested setpoint >= `settings.sat.fPwmSwitchMargin` (default 2.0)
holds for 60 s, the fail-safe (3.0 C / 30 s since entry 429) keeps its own timer, and a settings file without the marker key
`SATpwmmargin2` that stores exactly 1.5 gets 2.0 once.

- `SAT_OVERSHOOT_SUSTAIN_MS = 60000UL` (`src/OTGW-firmware/SATcycles.ino:35`), read by `satCycleCheckOvershootSwitch()`
  (`SATcycles.ino:1408`, threshold at `:1433`). The fail-safe is `SAT_OVERSHOOT_FAILSAFE_DELTA_C = 3.0f` (`SATcycles.ino:41`, entry 429) with `SAT_OVERSHOOT_FAILSAFE_SUSTAIN_MS` 30000 (`:42`).
- `fPwmSwitchMargin = 2.0f` (`src/OTGW-firmware/SATtypes.h:551`).
- The migration (`settingStuff.ino:727-735`, block `[sat-pwmmargin-migrate]`) follows the TASK-1961 marker pattern: the
  parse records whether `SATpwmmargin2` was present; without it a stored 1.5 (within 0.01) becomes 2.0 and the deferred
  flush writes the marker, so the migration runs once. Any other stored value stays.
- With the defaults a flow at least 3.0 C over the request switches through the fail-safe after 30 s, 30 s before
  the switch rule would fire.
- **Once per flame (entry 429).** After an underheat or saturation exit with the flame burning, `_cycle_burnLeftPwm` is set (`SATcycles.ino:1511`, `:1528`) and the margin rule is held for the rest of that flame (`:1425`); only the fail-safe can switch back. The at-max exit does not set that latch, so after it the margin rule can still re-enter PWM in the same flame (see ADR-296, point 7).
- **Heating allowed only (entries 410, 417).** Both rules run only while the request is above the cold threshold and no tolerance-band pause is active (`SATcontrol.ino:8576-8577`); otherwise their starts are cleared.

## Decision Contract

### Must

- The continuous to PWM switch fires on the first control tick at least 60 s after the first qualifying tick
  (flow - requested >= `fPwmSwitchMargin`).
- The default of `satpwmswitchmargin` is 2.0; its range stays 0.5-5.0.
- A settings file without `SATpwmmargin2` and with `SATpwmswitchmargin` 1.5 boots with 2.0 and writes the marker.
- The fail-safe, 3.0 C (entry 429), keeps its own 30 s timer.
- After a PWM to continuous exit that sets `_cycle_burnLeftPwm`, the margin rule does not switch that flame back to PWM (entry 429).
- Both rules run only while heating is allowed (entries 410, 417).

### Must Not

- Move a stored margin other than 1.5, or move a 1.5 that is stored together with the marker.
- Share the sustain constant between the switch rule and the fail-safe.

### Exceptions

- A user who deliberately chose 1.5 before this release is moved to 2.0 once; the stored value cannot tell that choice
  apart from the old default. Setting 1.5 again afterwards stays.

### Verification

- `python tests/test_sat_overshoot_sustain.py`: 85 cases on the real `satCycleCheckOvershootSwitch()`; 31 fail on the
  code before this ADR (`OTGW_SATCYCLES_INO` / `OTGW_SATTYPES_H` pointing at origin/dev f4373938a), all pass after it.
- `python tests/test_sat_pwm_margin_migration.py`: the real setter, writer and migration lines of `settingStuff.ino`;
  cases a, b and d fail on the code before this ADR, all four pass after it.
- `python tests/test_sat_lowload_contract.py` (low-load (LL) cases LL-004, LL-020b) and `tests/test_sat_slave_lowload.py`,
  `tests/test_sat_pwm_override.py` pin the 60 s timing.

## Alternatives Considered

- **Keep 1.5 C / 30 s (entry 404, "keep as is").** Rejected: Sergeant D overruled his own entry 404 with entry 407
  after the Vitodens ping-pong.
- **A fixed 2.0 C without a setting (Q-407a option B).** Rejected by Robert: the setting already exists with a REST (Representational State Transfer) API,
  MQTT (Message Queuing Telemetry Transport) and web UI contract; moving its default keeps that contract.
- **Change only the struct default, no migration.** Rejected: every save since TASK-1888 stored 1.5, so the new rule
  would reach almost no existing unit, including the Vitodens that triggered the change.
- **Drop the 3.5 C / 30 s fail-safe (Q-407a option D).** Rejected by Robert (C): it keeps a fast exit for a strong
  overshoot that the 60 s wait would otherwise delay. Its threshold later became 3.0 C (entry 429).

## Consequences

**Positive:**

- A request that sits near the old 1.5 C threshold no longer flips the mode every few minutes; the switch needs a
  larger and longer overshoot.
- The 60 s wait matches thermo-nova (`const.py:21`).
- Existing units get the new rule on the first boot of this release, without user action.

**Negative:**

- A moderate overshoot (2.0 to 3.0 C) runs 30 s longer in continuous before PWM takes over. Mitigation: the fail-safe
  still switches at 3.0 C after 30 s.
- A deliberate user choice of 1.5 is overwritten once. Mitigation: it is visible in the settings page and can be set
  again; the marker keeps it from then on.
- `settings.ini` gains one key (`SATpwmmargin2`); a bench settings diff shows it with the 1.5 to 2.0 change.

## Open Questions

- [x] Sergeant D is told of Robert's Q-407a answer (by 92) and may object to the 2.0 default or the kept fail-safe. — **Answered 2026-11-25 by Agent: Claude (otgw-architect), from the register (OTGW-sat-decisions 7655cca):** the kept fail-safe has Sergeant D's own decision since entry 429 (2026-11-19), which keeps it and sets its threshold to 3.0 C, with the 2.0 C / 60 s rule of entry 407 in the same answer. No entry from 408 to 493 objects to the 2.0 default. Interpretation (ours): no objection is outstanding. Robert's go to accept after aligning this ADR to entries 410, 417 and 429 was relayed by the coordinator on 2026-11-25.

## Related Decisions

- **ADR-249**: partly superseded: the 1.5 C / 30 s threshold and time of its switch rule are replaced; its own margin
  setting, the equality rule and the gates stay.
- **ADR-251**: unchanged: MsgID 3 plays no part in the switch.
- **ADR-252**: unchanged: the PWM-first start and its 60 s gate on the PWM to continuous exit.
- **ADR-296**: one PWM ON clock per flame; a re-entry through this rule or the fail-safe resumes the frozen clock.

## References

- `docs/sat/decisions/sergeant-d-corrections.md` entry 407 (with the Q-407a answer), entries 327, 359, 360, 404; entries 410, 417 and 429 (OTGW-sat-decisions 7655cca).
- TASK-2005 (indigo's Vitodens capture 2026-11-16).
- `src/OTGW-firmware/SATcycles.ino:35`, `:41-42`, `:1408`, `:1425`, `:1433`, `:1511`, `:1528`; `src/OTGW-firmware/SATtypes.h:551`;
  `src/OTGW-firmware/settingStuff.ino:284`, `:382-383`, `:727-735`; `src/OTGW-firmware/SATcontrol.ino:8576-8577` (origin/dev a60386c99).
- `other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:21`, `cycles/const.py:10-11`,
  `heating_control.py:288`, `:296` (commit 7beaaf5).
