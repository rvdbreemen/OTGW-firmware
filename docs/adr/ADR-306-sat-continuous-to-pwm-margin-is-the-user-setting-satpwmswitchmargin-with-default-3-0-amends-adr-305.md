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
    reason: "Initial proposal. Decision makers: Robert van den Breemen for the setting with default 3.0 (coordinator chat 2026-11-28 08:57, register entry 522; relayed, not seen by otgw-architect) and for one margin and one setting (DM 1576146949323751437, read back by otgw-architect); the coordinator for the SATpwmmargin3 migration (09:02, register entry 523, reversible; Robert is told)."
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
- **The end-of-cycle verdict uses the same setting** (decided by the coordinator, 09:02, register entry 523; confirmed in intent by Robert's DM 1576146949323751437). A continuous cycle whose tail p90 of flow minus control setpoint is at or above `satpwmswitchmargin` gets the OVERSHOOT verdict and enables PWM. This replaces ADR-305's "fixed 3.0, with `satovershootmargin` as the label only" for the verdict.
  - Reason: thermo-nova uses one constant, `OVERSHOOT_MARGIN_CELSIUS` (`cycles/const.py:10`), both for the runtime enable (`heating_control.py:288`) and for the verdict (`cycles/classifier.py:74`). One margin with one meaning keeps the two paths from disagreeing.
- **One margin, one setting** (Robert, DM 1576146949323751437, 08:05:16 UTC, after the ADR-306 question; register entry 525, OTGW-sat-decisions main 6ce7d6a): "Klinkt niet goed. Mijn bedoeling is 1 overshoot switch to PMW margins and setting." In English: "That does not sound right. My intent is one overshoot switch-to-PWM margin and setting." The coordinator's reading was sent back to him for confirmation (DM 1576147063882776688):
  - one overshoot margin, the setting `satpwmswitchmargin` (default 3.0);
  - it serves the continuous-to-PWM switch, the end-of-cycle verdict and the classification shown to the user;
  - the separate setting `satovershootmargin` goes;
  - the underheat exit stays a fixed rule.
  - This ADR follows that reading. Acceptance waits for his confirmation.
- **`satovershootmargin` goes.** The classification label (`SATcycles.ino:961`) and the overshoot-seconds count (`SATcycles.ino:1287`) read `satpwmswitchmargin` instead. The key `SATovershootmargin` (`settingStuff.ino:385`) is no longer written, and an old key in `settings.ini` is ignored on load. Its REST, MQTT and web UI fields go. With one margin, the label and the verdict can no longer disagree.
- **Removed, no alias** (decided by the coordinator, 09:40). A write to `overshoot_margin` does not set `satpwmswitchmargin`. Reasons:
  - An alias would be a second way in to the same margin, which is what Robert objected to.
  - Few automations can depend on it. The MQTT command `overshoot_margin` has been on the dev line since 2026-04-05 (commit 4d482ab53, `git log -S`), but the 2.x line has been public only through betas since 2026-11-01, run by a small group of testers. The coordinator gave the reason as "from the recent 2.0.0 betas (TASK-1485 era)". The git history shows the command is older than that; the small number of 2.x users is what carries the argument.
  - If Robert asks for an alias, this ADR changes.
- **Clarification of ADR-305 Decision 7 and its restore Must** (reported by the coordinator at 10:10 from the test author's run, not seen by otgw-architect).
  - What ADR-305 says: after a restart, "IDLE with the continuous setpoint and the entry-348 sequence" until the first update with the heater on.
  - What the code does: the restored flag puts SAT in PWM mode with the plain request until the first tick (CS=39.9 at 60000 ms, MM=0). On that first tick, a heater that is off ends PWM and clears the flag.
  - That matches thermo-nova on the wire. With PWM enabled and the status IDLE, thermo-nova is in CONTINUOUS control mode (HC:147-152). Its continuous setpoint is the request while the boiler temperature is unknown or at or below the request (HC:458-474). Its modulation state is OFF because PWM is enabled (HC:139-140), so MM is 0 (HC:517-518).
  - So the boiler sees the plain request with MM 0 in both, and the entry-348 start-up sequence does not run in that window.
  - ADR-305's wording is corrected here. Its Must reads: until the first update with the heater on, SAT sends the plain request with MM 0, and a heater that is off at that tick clears the flag.
  - This brings back the boot case that ADR-252 recorded: the boiler can light on the plain request without the flame-off hold. That is thermo-nova's behaviour. It falls under the cold-start risk that Robert accepted for ADR-305 (DM 1576143352036982909).
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

- Read the continuous-to-PWM margin, the end-of-cycle verdict margin, the classification label margin and the overshoot-seconds margin from `settings.sat.fPwmSwitchMargin`, with default 3.0.
- Stop writing `SATovershootmargin`, and ignore an old key on load.
- Keep the key `SATpwmswitchmargin`, its range 0.5-5.0, the REST field and the MQTT topic.
- Migrate once, under the marker `SATpwmmargin3`: a stored 1.5 or 2.0 becomes 3.0, and every other value stays.

### Must Not

- Use the setting for the underheat exit or the saturation exit.
- Keep a second overshoot margin setting.
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
- **Keep `satovershootmargin` as a label-only setting** (the first draft of this ADR). Rejected by Robert, DM 1576146949323751437: one margin and one setting.
- **Keep default 2.0.** Rejected: Robert set the default at 3.0.

## Consequences

**Positive:**

- A user whose boiler flips too often or too rarely can tune the margin without new firmware.
- No API (application programming interface) break: Home Assistant automations that set `pwm_switch_margin` keep working.

**Negative:**

- **The migration overwrites a deliberate 2.0.** A user who chose 2.0 on purpose sees it become 3.0 once. The firmware cannot tell that 2.0 apart from the old default. Mitigation: the beta notes say so, and the user can set 2.0 again; the marker stops a second migration.
- **`satovershootmargin` is removed from the API** (the MQTT settings command `overshoot_margin`, `MQTTstuff.ino:735`; the REST setting `overshoot_margin`, `restAPI.ino:1660-1661`, and its field `settings.sat.overshoot_margin`, `:2314`; the web UI field). A Home Assistant automation that writes it no longer has an effect. A user who had tuned it to change the classification now tunes `satpwmswitchmargin`, which also moves the switch.
- **The classification moves from 2.0 to 3.0 at the defaults.** Fewer cycles show OVERSHOOT than before.
- The docs and the help text keep the setting but must show the new default (`settingsHelp.h:138`, `docs/api/MQTT.md:376`, `docs/api/openapi.yaml`, MANUAL 5.6). These docs rows in `_comms/adr305_docs_lines_to_change_2026-11-28.md` change from "remove" to "default 3.0".

## Open Questions

- [ ] Robert confirms the coordinator's reading of DM 1576146949323751437 (DM 1576147063882776688): one margin and one setting, `satovershootmargin` goes, the underheat exit stays fixed, the one-time migration stays unless he says no.

## Related Decisions

- **ADR-305**: amended in its Decision 2 margin clause and its Decision 4 verdict margin. Everything else in it stands.
- **ADR-254**: already replaced in part by ADR-305. Its one-time 1.5 to 2.0 migration (`SATpwmmargin2`) is followed by the `SATpwmmargin3` step.

## References

- Robert van den Breemen, coordinator chat 2026-11-28 08:57 (relayed by the OTGW Coordinator at 08:58; no Discord id); register entry 522 (OTGW-sat-decisions main 5ec9755).
- The coordinator's decisions on the verdict margin and the migration, cross-session message of 09:02.
- Robert's DM 1576146949323751437 and the coordinator's read-back DM 1576147063882776688 (both read back by otgw-architect).
- thermo-nova 7beaaf5 (https://github.com/Alexwijn/SAT/tree/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat): `cycles/const.py:10-11`, `heating_control.py:288-297`, `cycles/classifier.py:74`.
- Firmware at origin/dev e0b06a180: `SATtypes.h:565-566`, `settingStuff.ino:386-387`, `:735-738`, `:1242`; `SATcycles.ino:961`, `:1287`; `settingStuff.ino:385`, `:1241`; `MQTTstuff.ino:735`; `restAPI.ino:1660-1661`, `:2314`.
