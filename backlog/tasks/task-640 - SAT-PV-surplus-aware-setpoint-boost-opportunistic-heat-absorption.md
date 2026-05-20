---
id: TASK-640
title: 'SAT: PV-surplus aware setpoint boost (opportunistic heat absorption)'
status: To Do
assignee: []
created_date: '2026-05-20 18:26'
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
ordinal: 43000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT already accepts external indoor and outdoor temperature via MQTT/REST (satHandleExternalTemp, satHandleExternalOutdoor). With those inputs, a user can build PV-surplus aware behavior entirely in Home Assistant by feeding a fake "warmer outdoor" value, but that is a hack: it pollutes the outdoor sensor history, it breaks heating-curve recommendation statistics, and it does not survive HA reboots.

This task adds PV-surplus as a first-class SAT input: a new optional surplus power input (W) that, when above a threshold and confirmed for a hold-time, opportunistically boosts the SAT target temperature by a configurable delta. This stores excess PV energy in the home as thermal mass (radiator water and structure), giving the user energy savings without compromising the heating-curve model or the safety system.

Scope is intentionally minimal for v2.0.0: a target-temperature boost driven by an external surplus signal, with safety constraints (max boost, max indoor temp ceiling, hysteresis on threshold). No native PV measurement, no inverter integration. The feature complements rather than replaces HA-side automations, and degrades gracefully when no surplus input arrives (stale-input expiry, same pattern as bExternalTempValid).

Rationale: the feature is low-hanging fruit because the integration point (target temp boost) already exists in code (settings.sat.fTargetTemp is reassigned by the preset/window logic at runtime), and the input pattern is identical to the existing external temp handlers. It differentiates the firmware against Laxilef and ESPHome (neither has this), and aligns with the prosumer/energy-transition user profile.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New settings in SATSection struct: bPvBoostEnabled (bool, default false), iPvBoostThresholdW (uint16, default 1500, range 100-10000), iPvBoostHoldS (uint16, default 120, range 30-600), fPvBoostDeltaC (float, default 1.5, range 0.5-5.0), fPvBoostMaxIndoorC (float, default 23.0, range 18.0-28.0)
- [ ] #2 New runtime state in SATState: fExternalPvSurplusW (float), bExternalPvSurplusValid (bool), iExternalPvSurplusLastMs (uint32), bPvBoostActive (bool), iPvBoostStartedMs (uint32), fPvBoostAppliedC (float)
- [ ] #3 New external input handler satHandlePvSurplus(const char* value) in SATcontrol.ino, following the exact pattern of satHandleExternalTemp: numeric validation, range check (0 to 50000 W), timestamp update, debug log
- [ ] #4 Stale-input expiry: PV surplus value invalidates after settings.sat.iSensorMaxAgeS seconds (reuse existing setting, do not add a new one). Boost deactivates immediately on invalidation
- [ ] #5 Boost activation logic in the SAT control loop: surplus > threshold for >= hold-time AND current indoor temp < boost-max-indoor -> activate boost, set fPvBoostAppliedC = fPvBoostDeltaC. Surplus < threshold (with 20% hysteresis, i.e. < 0.8 * threshold) OR indoor temp >= boost-max-indoor -> deactivate boost immediately
- [ ] #6 Effective target temp = settings.sat.fTargetTemp + state.sat.fPvBoostAppliedC (when active). Implementation must NOT mutate settings.sat.fTargetTemp; the boost is applied at the point where target is read by the heating curve and PID (around lines 820, 3104, 3644 in SATcontrol.ino as of 2026-05-20 — verify line numbers at implementation time). Confirm presets, window detection, and activity preset still operate on the unboosted target
- [ ] #7 Boost is bounded: never apply boost when window detection has cleared the target, never apply when SAT safety is tripped, never apply when boiler is in DHW priority
- [ ] #8 MQTT subscribe: set/<nodeId>/sat/pv_surplus_w (numeric W), set/<nodeId>/sat/pv_boost_enabled (0/1)
- [ ] #9 MQTT publish in satPublishMQTT(): sat/pv_surplus_w, sat/pv_surplus_valid (0/1), sat/pv_boost_active (0/1), sat/pv_boost_applied_c, plus all five new settings with the sat/pv_boost_* prefix
- [ ] #10 REST API: new settings visible in GET /api/v2/settings (keys satpvboostenabled, satpvboostthresholdw, satpvboostholds, satpvboostdeltac, satpvboostmaxindoorc). New endpoint POST /api/v2/sat/pvsurplus with body {"value": <W>}, mirroring POST /api/v2/sat/externaltemp
- [ ] #11 REST API: GET /api/v2/sat/status JSON includes pv_surplus_w, pv_surplus_valid, pv_boost_active, pv_boost_applied_c
- [ ] #12 HA auto-discovery: PV surplus W as sensor (unit W, device class power), pv_boost_active as binary_sensor, pv_boost_applied_c as sensor (unit °C). Boost enable as switch, the four numeric settings as number entities with min/max from the setting ranges
- [ ] #13 OpenAPI 3.0 spec (docs/api/openapi.yaml) updated with the new POST endpoint and status fields. TASK-210 (SAT REST endpoints to openapi.yaml) is Done — no coordination required
- [ ] #14 WebUI: PV-boost section in SAT settings page (5 fields, with inline help: what the threshold means, what the hold-time prevents, why the indoor max matters as safety ceiling). Default state: collapsed/disabled until user enables
- [ ] #15 WebUI SAT dashboard: when boost is active, show a clear visual indicator (badge 'PV boost: +1.5°C active') next to the target temperature. When inactive but enabled and waiting, show 'PV boost: armed (surplus 800W < 1500W threshold)'
- [ ] #16 Documentation: new section in docs/features/smart-autotune-thermostat.md explaining the feature, intended use case (radiator/UFH heat absorption from PV), limits (not a DHW boost — that needs separate logic), and a HA automation example that publishes pv_surplus_w from an existing PV sensor (e.g. solar inverter integration or P1 meter calculation)
- [ ] #17 NL manual chapter docs/manuals/nl/h05-sat-thermostaat.md and EN equivalent updated with the new feature
- [ ] #18 ADR: new ADR-104 (or next free number at implementation time) 'SAT PV-surplus opportunistic setpoint boost' documenting the design decision (additive boost, no mutation of settings.sat.fTargetTemp, hysteresis percentage rationale, why no DHW boost in this iteration, why reuse iSensorMaxAgeS for stale-input expiry)
- [ ] #19 Default behavior: with bPvBoostEnabled = false (the default), this feature is completely inert. No new MQTT publishes when disabled (only the enable flag is published). Existing users see no change after upgrade
- [ ] #20 Boost-active duration cap: maximum continuous boost duration is bounded. Add iPvBoostMaxDurationMin setting (uint16, default 240, range 30-1440). After cap, deactivate boost and refuse to reactivate for 30 minutes (cooldown). Prevents runaway boost on faulty surplus signal
<!-- AC:END -->
