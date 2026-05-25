# ADR-110-sat-pv-surplus-setpoint-boost

## Status

Proposed, 2026-05-26

## Context

SAT already accepts external indoor and outdoor temperature via MQTT/REST
(`satHandleExternalTemp`, `satHandleExternalOutdoor`). Users with PV who want
the boiler to absorb excess solar power into the building's thermal mass have
two workarounds today:

1. Push a fake "warmer outdoor" value via the existing
   `set/<nodeId>/sat/outdoor_temp` topic from a Home Assistant automation that
   monitors PV surplus. This pollutes the outdoor sensor history, breaks the
   heating-curve recommendation statistics (`SATcontrol.ino` uses outdoor temp
   for curve auto-tuning), and gives no native firmware-side expiry of the
   override — when HA is restarted, the fake reading stays valid until the
   firmware's generic sensor-max-age timeout kicks in.
2. Push a higher `sat/target` directly. This conflicts with the preset system
   (preset switching writes `settings.sat.fTargetTemp` and would either erase
   the boost or be erased by it).

Both workarounds are user-visible hacks. Field reports on Discord
(#dev-sat-mqtt, 2026-Q2) ask for a first-class input: "I have surplus, please
store it as heat without me having to reverse-engineer your outdoor curve."

The integration point is small: the SAT control loop already has a single
`effectiveTarget` expression
(`effectiveTarget = targetTemp + state.sat.fComfortOffset`, `SATcontrol.ino`
~line 3780) where additive offsets are layered before the heating-curve
calculation. Adding a third additive term — `+ state.sat.fPvBoostAppliedC` —
is a 1-line change at the hot path; the rest of the work is settings,
plumbing (MQTT/REST/HA), and the activation/deactivation state machine.

References:

- `src/OTGW-firmware/SATcontrol.ino` — `satHandleExternalTemp` (handler
  template), `satGetRoomTemp` (ceiling check), control-loop entry around line
  3780 (`effectiveTarget` calculation).
- `src/OTGW-firmware/SATtypes.h` — `SATSection` / `SATRuntimeSection` (the
  two-struct settings/state model from ADR-051).
- `src/OTGW-firmware/MQTTstuff.ino` — `kSatMqttCmds[]` dispatch table from
  ADR-078, which the new input handler hooks into without bespoke routing.
- Backlog task TASK-640 — feature scope and AC list.

## Decision

Add PV-surplus as a first-class SAT input. Concretely:

1. **Additive, never mutates the target.** The boost is applied as
   `state.sat.fPvBoostAppliedC`, added to the `effectiveTarget` expression in
   the control loop. `settings.sat.fTargetTemp` is read-only from the boost
   path. Presets, comfort offset, window detection, and activity preset all
   continue to operate on the unboosted target and remain composable.

2. **Hold-time before activation, hysteresis on deactivation.** Activation
   requires `pv_surplus_w >= pv_boost_threshold_w` for `pv_boost_hold_s`
   seconds continuously (default 120 s). Deactivation triggers as soon as
   surplus drops below 80% of the threshold. The asymmetry is intentional:
   the hold prevents flapping when clouds wash over the panels, the
   hysteresis snaps off the boost quickly when the sun really does drop, so
   the home doesn't bank surplus that has stopped existing.

3. **Reuse `iSensorMaxAgeS` for stale-input expiry.** SAT already enforces a
   single max-age on external indoor/outdoor temps. Adding a second, separate
   PV-only max-age would be cognitive overhead with no clear benefit — if
   the user's automation goes silent, they want all stale external inputs
   gone, not just one of them. Default 6 hours is generous enough to survive
   HA upgrades but short enough that a permanently broken pipeline doesn't
   silently keep boosting.

4. **No DHW boost in this iteration.** DHW boost would need its own logic
   (write `HW=`, raise `sat/dhw_setpoint`, respect anti-Legionella, handle
   the storage-tank vs combi distinction from MsgID 3 HB3). That's a
   separate task; conflating it with the CH setpoint boost would make the
   feature scope creep and obscure the safety story.

5. **30-minute cooldown after max-duration cap.** The cap
   (`pv_boost_max_duration_min`, default 240 min) plus cooldown is a runaway
   guard. If a stuck PV signal claims "1500 W surplus forever", we
   eventually stop boosting and stay off long enough for the surplus reading
   to either come back honestly or stay broken. Fixed cooldown of 1800000 ms
   (30 min) — no setting, no UI surface, low cognitive cost.

6. **Inert when disabled.** `bPvBoostEnabled = false` is the default. With
   the flag off, the boost is never applied, runtime telemetry is not
   published over MQTT, and HA discovery still emits the switch + 5 number
   entities so the user can enable from the UI without flashing firmware.

7. **Safety guards override boost.** `bSafetyTripped`, `bWindowOpen`, and
   `bDhwActive` all unconditionally deactivate the boost. The reasoning:
   safety_tripped means SAT has lost trust in its inputs and is bailing out;
   window_open means the heat would be vented; dhw_active means the boiler
   has higher-priority work to do and we don't want to fight DHW for
   modulation budget.

## Alternatives Considered

### A. Mutate `settings.sat.fTargetTemp` directly when surplus is high

Trivially simple — set the field, no plumbing — but rejected because it
collides with the preset state machine. Presets write `fTargetTemp` on every
switch; the boost would either be erased on the next preset evaluation, or
the boost would erase the user's preset change. Composing additive
"effective target" terms is the established pattern (comfort offset already
does this); doing it the same way for PV boost keeps the mental model
consistent and the preset behavior unchanged. Persistence is also wrong: a
mutation persists to LittleFS, so a reboot mid-boost would keep the wrong
target.

### B. Per-HA-automation fake outdoor temp injection

The status quo workaround. Rejected because (1) it pollutes outdoor sensor
history (every dashboard graph shows the fake values), (2) the firmware's
heating-curve recommendation engine reads outdoor temp for tuning
suggestions — fake values produce nonsense recommendations, (3) no firmware-
side expiry that's PV-aware: if HA goes offline the boost persists until the
generic 6-hour sensor timeout, but the user has no way to distinguish "PV is
zero" from "PV pipeline broke." First-class input gives us a real
`bExternalPvSurplusValid` flag the diagnostic JSON can expose.

### C. Dedicated SAT preset "PV"

Add `SAT_PRESET_PV` to the enum, map it to a higher target. Rejected because
presets are mutually exclusive (`eActivePreset` is a single value, see
`SATtypes.h:49-53`); the user would have to give up their current preset
(Comfort, Eco, etc.) to take the boost. The whole point is that the boost
should layer on top of whatever the user has chosen for the time of day.
Also: presets are a coarse user-facing concept, not a system-controlled
state — using one for an automated boost confuses the model.

## Consequences

### Positive

- Users get a documented, named feature for an explicit use case (PV
  prosumers). Differentiates against Laxilef and ESPHome which don't have
  this.
- Heating-curve auto-tuning continues to see honest outdoor temps and keeps
  producing meaningful recommendations.
- Composability: the boost stacks with comfort offset and presets without
  interfering with either.
- Default-off means zero user-visible change after upgrade — no broker
  noise, no HA dashboard pollution. Existing users see exactly the same
  behavior they had before.
- The activation/deactivation/cooldown state machine is small enough
  (~80 lines) and isolated enough (one new function, one extra term in the
  effective-target expression) to be reasoned about and tested by reading.

### Negative

- Six new settings. The settings surface grows; users who don't care still
  see them in `sendDeviceSettings` JSON and in the WebUI settings page (we
  collapse the section to soften this, but it's still visible).
- The boost can cause short-term comfort overshoot — by design, the home
  warms above the user's preset target. The `pv_boost_max_indoor_c` ceiling
  caps this but doesn't eliminate it. Users who don't expect to see 23 °C
  in the living room may be surprised. The dashboard badge is the
  mitigation.
- HA-side automation is still required to compute "surplus" — the firmware
  doesn't know what export is. Misconfiguration (publishing total PV
  production instead of net surplus) will boost when it shouldn't and is
  hard to debug from the firmware side. The HA automation example in the
  feature doc reduces this risk but doesn't eliminate it.
- Stale-input expiry reuses `iSensorMaxAgeS` (default 6 h). If a user wants
  the temp inputs to expire after 1 h but the PV input to stay valid for
  6 h, they cannot — they share the timeout. We judged this acceptable for
  v2.0.0 and noted it explicitly in §3 above; a per-input expiry can be
  added later if a real user requests it.

## Related

- ADR-051 — Two-struct settings/state model (`settings.sat.*`,
  `state.sat.*`).
- ADR-078 — SAT MQTT command dispatch table (`kSatMqttCmds[]`), which the
  new `pv_surplus_w` / `pv_boost_enabled` commands hook into.
- ADR-085 — SAT integration overview.
- ADR-100 — JIT HA discovery; new entities slot into the existing
  `streamSatZoneDiscovery` drip path rather than allocating a new pseudo-ID.
- Backlog: TASK-640.
