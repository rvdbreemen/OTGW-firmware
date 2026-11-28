---
id: "ADR-306"
title: "SAT continuous-to-PWM margin is the user setting satpwmswitchmargin with default 3.0 (amends ADR-305)"
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
  - "settings"
aliases:
  - "satpwmswitchmargin default 3.0"
  - "overshoot margin setting"
components:
  - "SAT control mode switching"
symbols:
  - "fPwmSwitchMargin"
  - "satCycleCheckOvershootSwitch"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-306 SAT continuous-to-PWM margin is the user setting satpwmswitchmargin with default 3.0 (amends ADR-305)

## Status

Proposed, 2026-11-28.

Guideline-level (ADR-080) with a test gate: `tests/test_sat_overshoot_sustain.py` (see Verification). It amends two clauses of ADR-305: the margin of Decision 2 and the verdict margin of Decision 4. ADR-305 stays in force for everything else.

## Status History

```yaml
status_history:
  - date: 2026-11-28
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: "Initial proposal. Decision makers: Robert van den Breemen for the setting with default 3.0 (coordinator chat 2026-11-28 08:57, register entry 522; relayed, not seen by otgw-architect); the coordinator for the verdict using the same setting and the SATpwmmargin3 migration (09:02, reversible; Robert is told)."
    changed_via: adr-kit
```

## Context

ADR-305 (Accepted 2026-11-28) makes SAT (Smart Autotune Thermostat) switch between continuous and PWM (pulse-width modulation) as thermo-nova does. Its Decision 2 sets the continuous-to-PWM margin to a fixed 3.0 C (thermo-nova `OVERSHOOT_MARGIN_CELSIUS`, `cycles/const.py:10`, opened at 7beaaf5 in this session), and removes the user setting `satpwmswitchmargin`.

Robert then wrote (register entry 522, recorded by otgw-docs, main 5ec9755), in the coordinator chat at 2026-11-28 08:57 (relayed by the coordinator; there is no Discord id): "De fixed overshoot marge mag je omzetten naar een variabele die via de setting ingesteld mag worden. De default is 3.0." In English: the fixed overshoot margin may become a variable that the setting sets; the default is 3.0.

Firmware today (origin/dev e0b06a180, opened in this session):
- The setting is `settings.sat.fPwmSwitchMargin`, default 2.0 (`SATtypes.h:566`).
- `settingStuff.ino:386` writes it on every save, so every unit has the key in `settings.ini`.
- `settingStuff.ino:1242` clamps it to 0.5-5.0 on write.
- `settingStuff.ino:735-738` holds a one-time migration: a stored 1.5 (the old default) becomes 2.0 when the marker `SATpwmmargin2` is missing.

## Decision

The margin of the continuous-to-PWM rule of ADR-305 is the user setting `satpwmswitchmargin`, with a default of 3.0. The rule switches when flow >= requested setpoint + `satpwmswitchmargin` holds for 60 s, with thermo-nova's gates (ADR-305 Decision 2).

- The setting stays, with its key, its REST (Representational State Transfer) field `pwm_switch_margin`, its MQTT (Message Queuing Telemetry Transport) topic `sat/pwm_switch_margin`, its web UI field and its range 0.5-5.0. Only its default changes, from 2.0 to 3.0.
- ADR-305's text "the setting `satpwmswitchmargin` is removed, and an old key in `settings.ini` is ignored on load" no longer holds. The Must Not "Use the setting `satpwmswitchmargin` ... for a mode decision" no longer holds for this rule.
- **The end-of-cycle verdict uses the same setting** (decided by the coordinator, 09:02; register entry 523). A continuous cycle whose tail p90 of flow minus control setpoint is at or above `satpwmswitchmargin` gets the OVERSHOOT verdict and enables PWM. This replaces ADR-305's "fixed 3.0, with `satovershootmargin` as the label only" for the verdict.
  - Reason: thermo-nova uses one constant, `OVERSHOOT_MARGIN_CELSIUS` (`cycles/const.py:10`), both for the runtime enable (`heating_control.py:288`) and for the verdict (`cycles/classifier.py:74`). One margin with one meaning keeps the two paths from disagreeing.
- **`satovershootmargin` does not change** (key, default 2.0, range). It still labels the classification shown to the user (`SATcycles.ino:961`) and feeds the overshoot-seconds count (`SATcycles.ino:1287`). It decides no mode. With both settings at their defaults (2.0 and 3.0), a cycle with a p90 between 2.0 and 3.0 shows the label OVERSHOOT while the verdict leaves the mode as it is. The verdict decides the mode.
- **Register.** Entry 523 (OTGW-sat-decisions main ec2c041) records the verdict and migration decisions of the coordinator. It supersedes the part of entry 522 that reads "verdict + underheat stay 3.0" for the verdict. Entry 522 stands for the setting with default 3.0, and for the underheat exit.
- **The underheat exit stays fixed:** requested - 3.0 C for 180 s (ADR-305 Decision 3, `cycles/const.py:11`). The 60 s hold and the 300 s saturation exit also stay as ADR-305 has them.
- **One-time migration to 3.0** (decided by the coordinator, 09:02; reversible; Robert is told; register entry 523).
  - A new marker `SATpwmmargin3` is added.
  - If the marker is missing at load, a stored `SATpwmswitchmargin` of 1.5 or 2.0 (both old defaults) becomes 3.0 once.
  - Any other stored value is a user choice and stays.
  - A missing key, a new install or a reset gives 3.0.
  - The existing `SATpwmmargin2` step (1.5 to 2.0, `settingStuff.ino:735-738`) is folded into this step or runs before it, so a stored 1.5 also ends at 3.0.
  - Reason: the firmware writes the key on every save (`settingStuff.ino:386`), so without a migration almost every upgraded unit would keep 2.0 and not switch as thermo-nova does.

## Decision Contract

### Must

- Read the continuous-to-PWM margin and the end-of-cycle verdict margin from `settings.sat.fPwmSwitchMargin`, with default 3.0.
- Keep the key `SATpwmswitchmargin`, its range 0.5-5.0, the REST field and the MQTT topic.
- Migrate once, under the marker `SATpwmmargin3`: a stored 1.5 or 2.0 becomes 3.0, and every other value stays.

### Must Not

- Use the setting for the underheat exit or the saturation exit.
- Let `satovershootmargin` decide a mode.
- Rewrite a stored value other than 1.5 or 2.0, or run the migration a second time.

### Exceptions

- None.

### Verification

- **`tests/test_sat_overshoot_sustain.py`**, on the real `satCycleCheckOvershootSwitch()`. The red cases run on the code before this ADR (default 2.0):
  - at the default, a flow 2.5 C over the request, held 120 s, does not switch (red: it switches today);
  - at the default, a flow 3.0 C over the request switches at 60 s;
  - with the setting at 2.0, a flow 2.5 C over the request switches at 60 s (control: green on both).
- **Verdict.** `tests/test_sat_cycle_classifier.py`, with the setting at 3.0: a continuous cycle with p90 = 2.5 does not enable PWM; one with p90 = 3.0 does. With the setting at 2.5, the 2.5 cycle enables PWM.
- **Migration.** `tests/test_sat_pwm_margin_migration.py`. Each case is red on the code before this ADR:
  - no `SATpwmmargin3` marker, stored 2.0: becomes 3.0;
  - no marker, stored 1.5: becomes 3.0;
  - no marker, stored 2.4: stays 2.4;
  - marker present, stored 2.0: stays 2.0 (a user choice made after the migration);
  - a fresh `settings.ini` gives 3.0.

## Alternatives Considered

- **Fixed 3.0 (ADR-305 as written).** Rejected by Robert's message: he wants the margin settable.
- **No migration (stored 2.0 stays).** Rejected by the coordinator. Almost every upgraded unit would keep 2.0, because every save writes the key.
- **Verdict fixed at 3.0, separate from the setting (ADR-305 as written).** Rejected by the coordinator. The runtime rule and the verdict would then use different margins, while thermo-nova uses one constant for both.
- **Keep default 2.0.** Rejected: Robert set the default at 3.0.

## Consequences

**Positive:**

- A user whose boiler flips too often or too rarely can tune the margin without new firmware.
- No API (application programming interface) break: Home Assistant automations that set `pwm_switch_margin` keep working.

**Negative:**

- **The migration overwrites a deliberate 2.0.** A user who chose 2.0 on purpose sees it become 3.0 once. The firmware cannot tell that 2.0 apart from the old default. Mitigation: the beta notes say so, and the user can set 2.0 again; the marker stops a second migration.
- **The label and the verdict can disagree** at the default settings (label at 2.0, verdict at 3.0). The web UI may show OVERSHOOT for a cycle that does not switch to PWM.
- The docs and the help text keep the setting but must show the new default (`settingsHelp.h:138`, `docs/api/MQTT.md:376`, `docs/api/openapi.yaml`, MANUAL 5.6). These docs rows in `_comms/adr305_docs_lines_to_change_2026-11-28.md` change from "remove" to "default 3.0".

## Open Questions

- None.

## Related Decisions

- **ADR-305**: amended in its Decision 2 margin clause and its Decision 4 verdict margin. Everything else in it stands.
- **ADR-254**: already replaced in part by ADR-305. Its one-time 1.5 to 2.0 migration (`SATpwmmargin2`) is followed by the `SATpwmmargin3` step.

## References

- Robert van den Breemen, coordinator chat 2026-11-28 08:57 (relayed by the OTGW Coordinator at 08:58; no Discord id); register entry 522 (OTGW-sat-decisions main 5ec9755).
- The coordinator's decisions on the verdict margin and the migration, cross-session message of 09:02.
- thermo-nova 7beaaf5 (https://github.com/Alexwijn/SAT/tree/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat): `cycles/const.py:10-11`, `heating_control.py:288-297`, `cycles/classifier.py:74`.
- Firmware at origin/dev e0b06a180: `SATtypes.h:565-566`, `settingStuff.ino:386-387`, `:735-738`, `:1242`; `SATcycles.ino:961`, `:1287`.
