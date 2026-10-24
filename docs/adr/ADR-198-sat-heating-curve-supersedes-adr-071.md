---
id: "ADR-198"
title: "SAT heating curve (proposed successor to ADR-071)"
status: "Proposed"
date: "2026-10-24"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "sat"
  - "heating-curve"
  - "weather-compensation"
  - "thermo-nova"
aliases:
  - "heating curve"
  - "heating curve coefficient"
  - "curve coefficient"
  - "satcoefficient"
  - "heating curve slope"
  - "base offset"
components:
  - "SAT heating curve"
symbols:
  - "satCalcHeatingCurve"
  - "satGetBaseOffset"
  - "satGetEffectiveHeatingSystem"
  - "fHeatingCurveCoeff"
  - "fHeatingCurveValue"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-198 SAT heating curve (proposed successor to ADR-071)

## Status

Proposed, 2026-10-24.

This ADR is the proposed successor to ADR-071 (TASK-1355). ADR-071 stays Accepted and its file is untouched while this ADR is Proposed, so the `supersedes` list in the front matter is empty for now: adr-kit's supersede command writes both sides of a supersession at once (adr-kit 0.59.1 `adr_catalog.py:300`), and a link on this side only would contradict ADR-071's unchanged `superseded_by`. Whether `adr-lint --strict` rejects a one-sided link was not verified: `bin/adr-lint` contains no supersession check, and `bin/adr-lint --strict docs/adr` crashed on 2026-10-24 (`AttributeError` in `severity_of`). The supersession is recorded on both sides (`adr supersede ADR-071 --by ADR-198`) only after the maintainer's explicit OK. Guideline-level per ADR-080: no CI (continuous integration) gate checks this decision.

## Status History

```yaml
status_history:
  - date: 2026-10-24
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal, supersedes ADR-071 (SAT port audit finding 69, TASK-1355)
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) computes a boiler flow temperature from the room target and the outdoor temperature (weather compensation), and the PID (proportional-integral-derivative) controller adds its correction on top. Ground truth for SAT behaviour is, in order (ADR-196): Sergeant D's recorded decisions in `docs/sat/sergeant-d-corrections.md`, then thermo-nova at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. thermo-nova paths below are relative to `other-projects/SAT-releases-thermo-nova/custom_components/sat`; that local copy was verified identical to the pinned commit on 2026-10-22 (CLAUDE.md, "SAT ground truth") and was not re-diffed for this ADR. Firmware paths are relative to `src/OTGW-firmware` at HEAD `2d01d3297`.

ADR-071 (Accepted 2026-04-09) recorded the curve. The SAT port audit of 2026-10-24 (`docs/sat/sat-port-audit-2026-10-24.md`, finding 69) found three wrong statements in it. The maintainer directive of 2026-10-24 is that an ADR with wrong values is superseded, not edited.

1. **Default coefficient.** ADR-071 says 1.0. The firmware default is 2.0 (`SATtypes.h:465`) since TASK-1347 (commit `8bfea9d7d`, 2026-10-24), as thermo-nova `const.py:182` and Sergeant D entry 24.
2. **Range.** ADR-071 says "approximately 0.4 - 2.0" and "typically 0.6 - 1.4". The firmware has four different ranges (see Decision point 4), none of them 0.4-2.0, and thermo-nova accepts 0.1-12 (`config_flow.py:616-618`).
3. **Origin.** ADR-071 says the formula was "derived from the OT-Thing reference implementation" and links OT-Thing. The formula is thermo-nova `heating_curve.py:28-39`. Sergeant D says he arrived at this convex shape himself, after many conversations with users (entry 24, #dev-sat-mqtt 2026-05-08 05:42 UTC). No OT-Thing source for it was found.

ADR-071 also claimed an "exact port". Two differences make that untrue (Decision points 2 and 5), and its ESP8266 soft-float trade-off no longer applies: the `dev` line is ESP32-S3 only.

Sergeant D statements this ADR relies on:

- **Entry 3** (2026-10-22): "The base temperature is everywhere in the code because it is the basis value of SAT." The base offset is a design constant, not a tunable. This ADR agrees.
- **Entry 24** (2026-10-20, answer b and follow-up; 2026-05-08 on the convex curve): "Curve Coefficient 2.0", "Leave existing settings alone", "Real case scenarios shown that convex curves are performing better even on mild and extreme conditions." This ADR agrees. The entry quotes the old firmware value (1.5 at `SATtypes.h:442`), and TASK-1347 has replaced it since.
- **Entry 28** (PID): the integral is clamped to plus or minus the heating curve value (thermo-nova `pid.py:191-192`), so the curve value also bounds the PID. This ADR does not change the PID, and ADR-069 stays the PID record.
- **Entry 30** (2026-06-20): "even the base offset should be only heating system related ... The heating system ( radiators or underfloor) should only affect the base offset." Device type (gas boiler, heat pump) and heating system are separate settings. This ADR agrees.

## Decision

SAT keeps thermo-nova's convex heating curve with one user coefficient, default 2.0, and a base offset chosen by the heating system alone. The firmware's known differences from thermo-nova are recorded here as differences, not as an exact port.

**1. Formula.** `satCalcHeatingCurve()` (`SATcontrol.ino:591-606`) computes

```text
value    = 4 * (target - 20) + 0.03 * (outside - 20)^2 - 0.4 * (outside - 20)
setpoint = base_offset + (coefficient / 4) * value
```

as thermo-nova `HeatingCurve.calculate()` (`heating_curve.py:28-30`) and `update()` (`heating_curve.py:36-39`). The reference temperature 20 is `SAT_HC_REF_TEMP` (`SATcontrol.ino:126`). The constants 4, 0.03 and 0.4 are fixed and not user settings, as in thermo-nova. The convex shape is deliberate (entry 24). The firmware stores the result in `state.sat.fHeatingCurveValue` (`SATcontrol.ino:604`).

**2. Base offset.** 27.2 °C for radiators and 20.0 °C for underfloor (`SAT_HC_BASE_OFFSET_RAD` and `SAT_HC_BASE_OFFSET_FLOOR`, `SATcontrol.ino:124-125`), chosen in `satGetBaseOffset()` (`SATcontrol.ino:330-335`) from the heating system only. AUTO resolves to radiators (`satGetEffectiveHeatingSystem()`, `SATcontrol.ino:269-276`). The values match thermo-nova `types.py:116-118`.

This is a deliberate difference from thermo-nova. thermo-nova has one axis, where `HEAT_PUMP` is a heating system and gets 27.2 (`types.py:110-118`). The firmware has a separate device type (`iHeatingSource`, `SATtypes.h:462`). A heat pump with underfloor therefore gets 20.0 in the firmware, where thermo-nova would give 27.2. That follows entry 30. ADR-071's table row "Heat pump: 27.2" no longer describes the firmware.

**3. Coefficient default.** `fHeatingCurveCoeff = 2.0f` (`SATtypes.h:465`), as thermo-nova `const.py:182` (entry 24, TASK-1347). A unit that already stores a coefficient keeps it, and there is no migration (entry 24). The setup wizard writes the coefficient only in manual tuning and keeps the stored value in automatic tuning (`data/v2.js:5400`, `:5447`).

**4. Coefficient ranges: recorded as they are, not reconciled.** The firmware has four ranges:

| Where | Range | Source |
|---|---|---|
| Setting write and settings load (`updateSetting`, also called on load at `settingStuff.ino:556`; the MQTT (Message Queuing Telemetry Transport) `heating_curve` command maps to the same `SATcoefficient` setting, `MQTTstuff.ino:710`) | 0.1 to 5.0, clamped with `constrain()`; a non-finite input keeps the old value (`clampSettingFloat`, `settingStuff.ino:708-716`) | `settingStuff.ino:1001` |
| REST settings metadata | 0 to 5 (integer min/max, so 0.1 cannot be expressed; value shown with 1 decimal) | `restAPI.ino:3802`, `:3887-3888` |
| Setup wizard slider | 0.2 to 2.4, step 0.1 | `data/v2.js:5547` |
| Firmware auto-tune clamp | 0.3 to 5.0 | `SATcontrol.ino:4695-4696` |
| thermo-nova config flow | 0.1 to 12, step 0.1 | `config_flow.py:616-618` |

This ADR records that the persisted setting is clamped to 0.1-5.0, as the code does today (`settingStuff.ino:1001`). It does not decide that range: no Sergeant D entry or maintainer decision sets it. Whether the upper bound follows thermo-nova's 12, and whether the REST metadata and the wizard slider match the setting clamp, is an open question (see Open Questions). ADR-071's "0.4 - 2.0" is withdrawn.

**5. Differences from thermo-nova, kept for now and recorded:**

- **No rounding.** thermo-nova rounds the scaled curve to 0.1 °C (`round(..., 1)`, `heating_curve.py:39`). The firmware does not (`SATcontrol.ino:602-605`), and neither does the web UI copy of the formula (`data/v2.js:4818-4822`). The difference is at most 0.05 °C on the curve value, and it reaches the PID gains and the integral clamp through that value.
- **Target input.** thermo-nova feeds `self.target_temperature` (`climate.py:688-697`). The firmware feeds `targetTemp + state.sat.fComfortOffset`, plus the PV (photovoltaic) boost when active (`SATcontrol.ino:5062-5067`). ADR-110 records the PV boost. Whether thermo-nova's `target_temperature` already includes an equivalent of the comfort offset was not checked for this ADR.
- **Auto-tune of the coefficient.** A firmware-only option (`bAutoTune`, default off, `SATtypes.h:533`) scales the coefficient by its rate, clamps it to 0.3-5.0 and persists it with `writeSettings()` (`SATcontrol.ino:4664-4705`). thermo-nova reads the coefficient only from the config entry (`entry_data.py:213`, `heating_curve.py:25`) and never changes it. Sergeant D has not been asked about this option.

**6. Coefficient in the PID gains.** With automatic gains, the PID derives Kp from the coefficient and the curve value: `kp = coeff * curveValue / divisor` (`SATpid.ino:130-137`; zones `SATcontrol.ino:2031-2033`), as thermo-nova `pid.py:71`, except that thermo-nova rounds Kp to 6 decimals (`round(..., 6)`) and the firmware does not (`SATpid.ino:135`). That rounding is a PID difference, recorded as an open question in ADR-197, and is not part of the curve differences in point 5. ADR-071 got this part right. ADR-069 stays the record for the PID itself.

**7. Output clamp.** The curve plus the PID correction, after the multi-zone block and the solar offset, is clamped to [`SAT_MIN_SETPOINT` 10, `satEffectiveMaxSetpoint()`] (`SATcontrol.ino:5145-5152`). The ceiling is the lowest of:

- the boiler's MaxTSet, or 75 when MaxTSet is below 30 (`SATcontrol.ino:317-318`, `:132`);
- the system cap: 62 for radiators, 45 for underfloor, 40 for a heat pump source (`satGetMaxSetpoint()`, `SATcontrol.ino:303-310`);
- the hard ceiling: 80 for radiators, 50 for underfloor (`SATcontrol.ino:137-138`);
- the global cap `settings.sat.fMaxSetpoint`, which falls back to 65 when its value is out of range (`SATcontrol.ino:139`, `:323-324`).

ADR-071 left out the global cap and MaxTSet. The code comment at `SATcontrol.ino:293-302` records why the radiator cap of 62 differs from thermo-nova's default of 55. This ADR does not decide the ceilings and only records them. Changing them would need its own decision.

## Decision Contract

### Must

- Compute the curve with thermo-nova's formula and its fixed constants 4, 0.03, 0.4 and reference 20 (`heating_curve.py:28-39`).
- Take the base offset from the heating system alone: 27.2 radiators (and AUTO), 20.0 underfloor (entries 3 and 30).
- Default the coefficient to 2.0 and keep a stored coefficient across upgrades (entry 24).
- Cite thermo-nova `heating_curve.py`, not OT-Thing, as the origin of the formula.

### Must Not

- Turn the polynomial constants or the base offsets into user settings (entry 3).
- Let the device type (heat pump) change the base offset (entry 30).
- Migrate a stored coefficient to the new default (entry 24).
- Describe the port as exact while the differences in point 5 remain.

### Verification

- `python tests/test_sat_wizard_coefficient.py` checks the 2.0 default in `SATtypes.h` and that the wizard keeps a stored coefficient. It is a source check and does not run the wizard. Run on 2026-10-24: 4 tests, OK.
- No test calls `satCalcHeatingCurve()`. No file under `tests/` contains `satCalcHeatingCurve`. A case-sensitive grep of `tests/` for `satCalcHeatingCurve` or `HeatingCurve` finds only `tests/test_sat_wizard_coefficient.py`; a case-insensitive grep also finds two web UI tests (`tests/webui/sat-page-order.test.mjs`, `tests/webui/sat-status-pill.test.mjs`), which use `heating_curve` only as a fixture value and do not compute the curve (2026-10-24). The formula equivalence in point 1 was checked by reading both sources, not by running them.
- No CI gate. This ADR claims none.

## Alternatives Considered

- **Amend ADR-071 in place.** Rejected: ADR-071 is Accepted and immutable, and the maintainer directive of 2026-10-24 is to supersede an ADR with wrong values.
- **Retire ADR-071 without a successor.** Rejected: the curve is a durable decision that the PID, the PWM duty formula (base offset) and the UI all depend on. Without a record, the next maintainer has only the code and an audit finding.
- **Declare the firmware an exact port and fix the differences in the same change.** Rejected for this ADR: rounding, the target input and auto-tune each need a decision of their own, and the auto-tune option needs Sergeant D's view. This ADR records the current state and lists the decisions still to be taken. Code changes are out of scope for TASK-1355.
- **Adopt thermo-nova's 0.1-12 range now.** Not chosen: no installation or statement shows a need for a coefficient above 5, and the four firmware ranges should be reconciled in one step. Left as an open question.
- **Keep thermo-nova's single axis (heat pump base offset 27.2).** Rejected on entry 30: the heating system alone sets the base offset.

## Consequences

**Positive:**

- Every value in the record matches the code at HEAD `2d01d3297` and has a citation (firmware `file:line`, entry number, thermo-nova `file:line`).
- The real origin of the formula is recorded with Sergeant D's own statement.
- The departures from thermo-nova are visible, so a later "exact port" claim can be checked against this list.

**Negative:**

- **Four inconsistent coefficient ranges stay in the code.** A user can set 2.6 through the REST (Representational State Transfer) API or the MQTT `heating_curve` command, but the wizard slider cannot show it (max 2.4). The REST metadata advertises a minimum of 0, which the setting clamp turns into 0.1. Mitigation: open question below, and a backlog task to follow.
- **Rounding difference.** The curve value differs from thermo-nova by up to 0.05 °C. Mitigation: open question below.
- **Auto-tune can move the coefficient away from Sergeant D's default without the user's choice.** It is off by default (`SATtypes.h:533`). Mitigation: open question below.
- **No executable test of the formula.** Mitigation: a host test that compiles the real `satCalcHeatingCurve()` and compares it with thermo-nova values would close this gap.

## Open Questions

- [x] Coefficient range: keep 0.1-5.0, or follow thermo-nova's 0.1-12 (`config_flow.py:616-618`)? And should the REST metadata (`restAPI.ino:3888`, 0-5 as integers) and the wizard slider (`data/v2.js:5547`, 0.2-2.4) follow the setting clamp (`settingStuff.ino:1001`)? Needs a maintainer decision. Ask Sergeant D only for the upper bound, through `docs/sat/open-questions.md`. — **Answered 2026-10-24 by Robert van den Breemen:** Robert 2026-10-24: keep 0.1-5.0 and make the REST metadata and the wizard slider follow it (TASK-1359). Whether 5 is a high enough upper bound is asked of Sergeant D (docs/sat/open-questions.md Q21); a different answer from him leads to a follow-up ADR.
- [x] Round the curve value to 0.1 °C as thermo-nova `heating_curve.py:39`? Firmware `SATcontrol.ino:602-605` does not round. — **Answered 2026-10-24 by Robert van den Breemen:** Robert 2026-10-24: yes, round to 0.1 C as thermo-nova heating_curve.py:39 (TASK-1360; plan shown to Sergeant D first because it touches control).
- [ ] Firmware auto-tune of the coefficient (`SATcontrol.ino:4664-4705`) is not in thermo-nova. Keep it, or remove it? This one is for Sergeant D (ADR-196: firmware additions need his view), asked one question per message through `docs/sat/open-questions.md`.
- [x] Target input: does thermo-nova's `target_temperature` (`climate.py:690`) include anything equivalent to the firmware's comfort offset (`SATcontrol.ino:5062`)? Not checked for this ADR. — **Answered 2026-10-24 by Agent: Claude:** Checked 2026-10-24: no. thermo-nova's target_temperature is the plain target (climate.py:266-268) and the heating curve uses it unchanged (climate.py:690, :697); thermal comfort adjusts the measured room temperature instead (climate.py:276-277, SummerSimmer.index). The firmware adds a comfort offset and the PV boost to the target before the curve (SATcontrol.ino:5063-5065): a firmware-only difference, already audit findings #33 and #48, to be put to Sergeant D through docs/sat/open-questions.md. The curve formula itself is unaffected.
- [x] With more than one zone (`settings.sat.iZoneCount > 1`), `satZonePidStep()` (`SATcontrol.ino:2003`) calls `satCalcHeatingCurve()` again for each zone that passes its validity checks (`:2025`, called from `:5085`) after the primary call (`:5067`). Each call overwrites `state.sat.fHeatingCurveValue` (`:604`), so the published `heating_curve` is the curve of the last zone that reached that call, not the primary's. This was found by reading the code and not reproduced. Is that intended? — **Answered 2026-10-24 by Agent: Claude:** Not intended; fixed by TASK-1353 (alpha.498, 2026-10-24): satCalcHeatingCurve() is now pure and only the primary call site stores state.sat.fHeatingCurveValue, so the published curve is the primary curve with zones on (tests/test_sat_published_curve.py, red on the old code).

## Related Decisions

- **ADR-071 (SAT heating curve algorithm)**: superseded by this ADR on acceptance (`adr supersede ADR-071 --by ADR-198`). Its formula, its fixed constants, the per-system base offsets for radiators and underfloor, the coefficient's role in the PID gains, and the rejected alternatives (fixed setpoint, linear slope, lookup table, building model) carry over. Its default 1.0, its 0.4-2.0 and 0.6-1.4 ranges, its OT-Thing origin, its "exact port" claim, its heat-pump base-offset row and its ESP8266 float note are replaced.
- **ADR-069 (SAT PID v3) and its proposed successor ADR-197**: the PID adds its correction to the curve value and derives its gains from the coefficient. ADR-069 stays Accepted until the maintainer accepts ADR-197 (TASK-1355). This ADR decides nothing about the PID, and its gain formula and integral clamp match ADR-197's.
- **ADR-196 (SAT behaviour ground truth)**: the source order this ADR follows.
- **ADR-195 (SAT cold state and CH-enable rule) and its proposed successor ADR-199**: the cold cutoff acts on the requested setpoint that starts from this curve. Neither is changed by this ADR.
- **ADR-110 (SAT PV surplus setpoint boost)**: adds the boost to the target before the curve. It is unchanged.
- **ADR-085 (SAT integration)**: the umbrella decision. It is unchanged.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level and claims no gate.

## References

- `docs/sat/sat-port-audit-2026-10-24.md`, finding 69 (ADR drift).
- `docs/sat/sergeant-d-corrections.md` entries 3, 24, 28 and 30, with the Discord links recorded there. Entry 24 links: https://discord.com/channels/812969634638725140/1514720723980259460/1562170196897038447, https://discord.com/channels/812969634638725140/1514720723980259460/1562177212155826199, https://discord.com/channels/812969634638725140/1105556725714649128/1502183820454006835.
- thermo-nova at `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, `custom_components/sat`: `heating_curve.py:13-39`; `const.py:97`, `:182`; `config_flow.py:616-618`; `types.py:110-118`; `entry_data.py:50`, `:213`; `pid.py:71`, `:191-192`; `climate.py:688-697`.
- Firmware at HEAD `2d01d3297`, `src/OTGW-firmware`: `SATcontrol.ino:124-126`, `:129`, `:132`, `:137-139`, `:269-276`, `:293-310`, `:315-327`, `:330-335`, `:591-606`, `:2025-2034`, `:4664-4705`, `:5062-5067`, `:5085`, `:5145-5152`; `SATpid.ino:120-142`; `SATtypes.h:462`, `:465`, `:533`; `settingStuff.ino:556`, `:708-716`, `:1001`; `MQTTstuff.ino:710`; `SATcontrol.ino:2003`; `restAPI.ino:3802`, `:3887-3888`; `data/v2.js:4818-4822`, `:5400`, `:5447`, `:5547`.
- Tasks: TASK-1355 (this supersession), TASK-1347 (coefficient default 2.0, commit `8bfea9d7d`), TASK-1273.
- Tests: `tests/test_sat_wizard_coefficient.py`.

This decision has no declarative code rule that a regex could check, and no host test of the formula exists yet. It therefore has no Enforcement block and is guideline-level per ADR-080.
