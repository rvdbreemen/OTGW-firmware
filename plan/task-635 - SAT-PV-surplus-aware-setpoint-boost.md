-----

id: TASK-635
title: ‘SAT: PV-surplus aware setpoint boost (opportunistic heat absorption)’
status: To Do
assignee: []
created_date: ‘2026-05-20 16:45’
updated_date: ‘2026-05-20 17:35’
labels:

- sat
- feature
- mqtt
- rest
- webui
- energy
- milestone-v2.0.0
  dependencies: []
  priority: medium

-----

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->

SAT already accepts external indoor and outdoor temperature via MQTT/REST (`satHandleExternalTemp`, `satHandleExternalOutdoor`). With those inputs, a user can build PV-surplus aware behavior entirely in Home Assistant by feeding a fake “warmer outdoor” value, but that is a hack: it pollutes the outdoor sensor history, it breaks heating-curve recommendation statistics, and it does not survive HA reboots.

This task adds PV-surplus as a first-class SAT input: a new optional surplus power input (W) that, when above a threshold and confirmed for a hold-time, opportunistically boosts the SAT target temperature by a configurable delta. This stores excess PV energy in the home as thermal mass (radiator water and structure), giving the user energy savings without compromising the heating-curve model or the safety system.

Scope is intentionally minimal for v2.0.0: a target-temperature boost driven by an external surplus signal, with safety constraints (max boost, max indoor temp ceiling, hysteresis on threshold). No native PV measurement, no inverter integration. The feature complements rather than replaces HA-side automations, and degrades gracefully when no surplus input arrives (stale-input expiry, same pattern as `bExternalTempValid`).

Rationale: the feature is laaghangend fruit because the integration point (target temp boost) already exists in code (`settings.sat.fTargetTemp` is reassigned by the preset/window logic at runtime), and the input pattern is identical to the existing external temp handlers. It differentiates the firmware against Laxilef and ESPHome (neither has this), and aligns with the prosumer/energy-transition user profile.

<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria

<!-- AC:BEGIN -->

- [ ] #1 New settings in `SATSection` struct: `bPvBoostEnabled` (bool, default false), `iPvBoostThresholdW` (uint16, default 1500, range 100-10000), `iPvBoostHoldS` (uint16, default 120, range 30-600), `fPvBoostDeltaC` (float, default 1.5, range 0.5-5.0), `fPvBoostMaxIndoorC` (float, default 23.0, range 18.0-28.0)
- [ ] #2 New runtime state in `SATState`: `fExternalPvSurplusW` (float), `bExternalPvSurplusValid` (bool), `iExternalPvSurplusLastMs` (uint32), `bPvBoostActive` (bool), `iPvBoostStartedMs` (uint32), `fPvBoostAppliedC` (float)
- [ ] #3 New external input handler `satHandlePvSurplus(const char* value)` in `SATcontrol.ino`, following the exact pattern of `satHandleExternalTemp`: numeric validation, range check (0 to 50000 W), timestamp update, debug log
- [ ] #4 Stale-input expiry: PV surplus value invalidates after `settings.sat.iSensorMaxAgeS` seconds (reuse existing setting, do not add a new one). Boost deactivates immediately on invalidation
- [ ] #5 Boost activation logic in the SAT control loop: surplus > threshold for ≥ hold-time AND current indoor temp < boost-max-indoor → activate boost, set `fPvBoostAppliedC = fPvBoostDeltaC`. Surplus < threshold (with 20% hysteresis, i.e. < 0.8 * threshold) OR indoor temp ≥ boost-max-indoor → deactivate boost immediately
- [ ] #6 Effective target temp = `settings.sat.fTargetTemp + state.sat.fPvBoostAppliedC` (when active). Implementation must NOT mutate `settings.sat.fTargetTemp`; the boost is applied at the point where target is read by the heating curve and PID (around line 820 / 3104 / 3644 in SATcontrol.ino). Confirm presets, window detection, and activity preset still operate on the unboosted target
- [ ] #7 Boost is bounded: never apply boost when window detection has cleared the target, never apply when SAT safety is tripped, never apply when boiler is in DHW priority
- [ ] #8 MQTT subscribe: `set/<nodeId>/sat/pv_surplus_w` (numeric W), `set/<nodeId>/sat/pv_boost_enabled` (0/1)
- [ ] #9 MQTT publish in `satPublishMQTT()`: `sat/pv_surplus_w`, `sat/pv_surplus_valid` (0/1), `sat/pv_boost_active` (0/1), `sat/pv_boost_applied_c`, plus all five new settings with the `sat/pv_boost_*` prefix
- [ ] #10 REST API: new settings visible in `GET /api/v2/settings` (keys `satpvboostenabled`, `satpvboostthresholdw`, `satpvboostholds`, `satpvboostdeltac`, `satpvboostmaxindoorc`). New endpoint `POST /api/v2/sat/pvsurplus` with body `{"value": <W>}`, mirroring `POST /api/v2/sat/externaltemp`
- [ ] #11 REST API: `GET /api/v2/sat/status` JSON includes `pv_surplus_w`, `pv_surplus_valid`, `pv_boost_active`, `pv_boost_applied_c`
- [ ] #12 HA auto-discovery: PV surplus W as `sensor` (unit W, device class power), pv_boost_active as `binary_sensor`, pv_boost_applied_c as `sensor` (unit °C). Boost enable as `switch`, the four numeric settings as `number` entities with min/max from the setting ranges
- [ ] #13 OpenAPI 3.0 spec (`docs/api/openapi.yaml`) updated with the new POST endpoint and status fields. Coordinated with TASK-210 (SAT REST endpoints to openapi.yaml) if still open
- [ ] #14 WebUI: PV-boost section in SAT settings page (5 fields, with inline help: what the threshold means, what the hold-time prevents, why the indoor max matters as safety ceiling). Default state: collapsed/disabled until user enables
- [ ] #15 WebUI SAT dashboard: when boost is active, show a clear visual indicator (badge “PV boost: +1.5°C active”) next to the target temperature. When inactive but enabled and waiting, show “PV boost: armed (surplus 800W < 1500W threshold)”
- [ ] #16 Documentation: new section in `docs/features/smart-autotune-thermostat.md` explaining the feature, intended use case (radiator/UFH heat absorption from PV), limits (not a DHW boost — that needs separate logic), and a HA automation example that publishes `pv_surplus_w` from an existing PV sensor (e.g. solar inverter integration or P1 meter calculation)
- [ ] #17 NL manual chapter `docs/manuals/nl/h05-sat-thermostaat.md` and EN equivalent updated with the new feature
- [ ] #18 ADR: new ADR-104 (or next free number) “SAT PV-surplus opportunistic setpoint boost” documenting the design decision (additive boost, no mutation of settings.sat.fTargetTemp, hysteresis percentage rationale, why no DHW boost in this iteration, why reuse iSensorMaxAgeS for stale-input expiry)
- [ ] #19 Default behavior: with `bPvBoostEnabled = false` (the default), this feature is completely inert. No new MQTT publishes when disabled (only the enable flag is published). Existing users see no change after upgrade
- [ ] #20 Boost-active duration cap: maximum continuous boost duration is bounded. Add `iPvBoostMaxDurationMin` setting (uint16, default 240, range 30-1440). After cap, deactivate boost and refuse to reactivate for 30 minutes (cooldown). Prevents runaway boost on faulty surplus signal

<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->

1. Add six new settings to `SATSection` struct in `OTGW-firmware.h` (`bPvBoostEnabled`, `iPvBoostThresholdW`, `iPvBoostHoldS`, `fPvBoostDeltaC`, `fPvBoostMaxIndoorC`, `iPvBoostMaxDurationMin`)
1. Add six new runtime state fields to `SATState` struct in `OTGW-firmware.h` (`fExternalPvSurplusW`, `bExternalPvSurplusValid`, `iExternalPvSurplusLastMs`, `bPvBoostActive`, `iPvBoostStartedMs`, `fPvBoostAppliedC`, plus `iPvBoostCooldownUntilMs` for the post-cap cooldown)
1. Add settings persistence in `settingStuff.ino` (write/read/update with constrain), following the existing pattern for SAT settings
1. Implement `satHandlePvSurplus()` in `SATcontrol.ino` adjacent to the existing external handlers (around line 1080), with identical validation pattern
1. Implement `satEvaluatePvBoost()` function called from the SAT control loop (next to `satApplyPresets()` or `satCheckWindow()`). This function: checks enable flag; checks input validity (stale expiry); evaluates threshold + hold + indoor max + duration cap + cooldown; updates `bPvBoostActive` and `fPvBoostAppliedC`
1. Introduce a `satGetEffectiveTarget()` helper that returns `settings.sat.fTargetTemp + (state.sat.bPvBoostActive ? state.sat.fPvBoostAppliedC : 0.0f)`. Replace direct reads of `settings.sat.fTargetTemp` at the three identified locations (lines ~820, ~3104, ~3644) with this helper. Audit all other reads of `fTargetTemp` to confirm preset/window logic is untouched (those should NOT use the effective getter)
1. Verify safety interactions: if `state.sat.bSafetyTripped` is true, force `bPvBoostActive = false`. If window is open (target overridden), boost is suspended (do not apply on top of the window-reduced target — the user wants the room to cool down). If boiler is in DHW priority, boost is suspended
1. Add MQTT subscribe handlers in `MQTTstuff.ino` for `pv_surplus_w` and `pv_boost_enabled` (and the four numeric setting topics following the existing pattern for SAT settings)
1. Extend `satPublishMQTT()` with the new publishes. Gate publishes when feature is disabled per AC #19
1. Add REST endpoint `POST /api/v2/sat/pvsurplus` in `restAPI.ino` mirroring the existing `POST /api/v2/sat/externaltemp`
1. Extend the SAT status JSON output (around line 1658) with the four new fields
1. Add HA auto-discovery entries to `mqttha.cfg` (or wherever discovery is configured): one sensor, one binary_sensor, two sensors (applied_c), one switch, five numbers
1. WebUI: extend `sat.js` to render boost status and applied delta in the dashboard. Extend `index.js`/`index.html` with the new settings rows. Keep the section visually grouped under “PV Surplus Boost” with clear labels
1. Update OpenAPI spec, feature documentation (EN + NL manuals), and write ADR-104
1. Manual testing: enable feature; publish synthetic surplus values via MQTT; confirm boost activates after hold-time, deactivates on threshold breach with hysteresis, deactivates on indoor ceiling breach, deactivates on safety trip, respects duration cap and cooldown, and survives a SAT disable/enable cycle (`fPvBoostAppliedC` must reset to 0)

<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->

**Why this design**

The boost is applied at the target-temperature read sites, not at the heating-curve output. This means the entire SAT control chain (heating curve, PID, cycle classifier, safety) operates on the boosted target consistently. Applying the boost at the heating-curve output would have caused PID to fight the curve, and the cycle classifier would have flagged the resulting overshoot as a problem.

`settings.sat.fTargetTemp` is deliberately NOT mutated. The preset/window logic already mutates it (with `fPreCustomTemp` / `fPreWindowTarget` rollback fields), so adding a third mutator would require triple bookkeeping. An additive boost via `satGetEffectiveTarget()` is simpler and safer.

Hysteresis is set at 20% of the threshold to prevent flapping when a PV inverter produces noisy values around the threshold (e.g. partly cloudy days). The hold-time prevents activation on transient peaks (e.g. cloud edge effect). Together they should give a few activate/deactivate cycles per day on a typical mixed-weather day, not dozens.

The indoor max ceiling (default 23.0°C) is a comfort safety, not a hardware safety. The existing SAT hard temperature ceiling (50°C UFH / 80°C radiator) still bounds the flow temperature. The indoor max prevents the boost from cooking the user.

The duration cap (default 4 hours) plus 30-minute cooldown prevents two failure modes: (1) a faulty surplus sensor that reports 5000W continuously, and (2) a thermal-mass-saturated home where additional heat input no longer helps. Both should produce visible behavior change rather than silent runaway.

**Why not native PV measurement**

OTGW32 has GPIO headroom in theory, but adding a current-clamp ADC reader, calibration logic, and a single-phase/three-phase model would more than double the scope of this task. Users with PV already have an inverter integration in HA or a P1-meter reader. Trusting that input is consistent with how SAT already trusts MQTT-supplied indoor/outdoor temps.

**Why not DHW boost**

DHW is controlled via `hotwater` MQTT commands (PIC PIC OT command `HW=`) and SAT’s role for DHW is minimal. A “boost DHW setpoint on PV surplus” feature would touch a different control path and warrant its own task. Keep this iteration focused on CH boost.

**HA automation example for the docs**

```yaml
automation:
  - alias: "Publish PV surplus to OTGW SAT"
    trigger:
      - platform: state
        entity_id: sensor.solar_net_power
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/sat/pv_surplus_w"
          payload: "{{ states('sensor.solar_net_power') | float }}"
```

Where `sensor.solar_net_power` is positive when exporting to grid (or, equivalently, when PV production exceeds household consumption).

**Test scenarios to cover**

1. Cold sunny day with mixed clouds: expect 3-5 boost cycles, no flapping
1. PV inverter offline mid-day: expect surplus invalidation after iSensorMaxAgeS, boost deactivates within one control interval
1. User toggles SAT off while boost is active: expect fPvBoostAppliedC to reset to 0
1. Window detection triggers while boost is active: expect boost to suspend, then resume when window closes (if surplus still present and hold-time re-elapses)
1. Boost reaches 4-hour duration cap: expect deactivation, 30-minute cooldown, no re-activation during cooldown even if surplus persists
1. Indoor temp drifts up to 23°C while boost is active (and surplus is still high): expect deactivation on indoor ceiling
1. PV sensor faulty, publishing 9999W constantly: expect duration cap to cut off the boost; user sees in dashboard

**Coordination notes**

- AC #18 ADR may need renumbering if PRs merge in parallel; check next free number at PR open time.
- AC #13 OpenAPI work overlaps with TASK-210 if that task is still in progress; coordinate to avoid merge conflicts.
- The new `iPvBoostMaxDurationMin` setting reuses the SAT settings persistence pattern; if a settings struct size limit becomes a concern, this is the first task to feel it (six new fields). Validate flash footprint on ESP8266 build (Arduino Core 2.7.4) before merging.

<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->

<!-- to be filled on completion -->

<!-- SECTION:FINAL_SUMMARY:END -->