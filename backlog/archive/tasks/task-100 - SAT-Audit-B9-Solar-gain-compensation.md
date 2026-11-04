---
id: TASK-100
title: 'SAT Audit B9: Solar gain compensation'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:51'
updated_date: '2026-04-09 05:21'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the solar gain compensation in Python SAT thermo-nova with C++ firmware. Verify solar irradiance detection, setpoint adjustment and time behavior of the compensation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Solar gain detection method compared (sensor or calculated)
- [x] #2 Setpoint adjustment formula verified
- [x] #3 Time behavior and hysteresis verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B9 audit: Solar gain compensation - GAPS FOUND

Python (solar_gain.py):
- SolarGainController.update() evaluates: sun_elevation gating (minimum_elevation check), rise_per_hour >= minimum_rise_per_hour, valves_open=True, boiler_output_is_low (flame=False OR relative_modulation <= SOLAR_GAIN_MAX_RELATIVE_MODULATION_PERCENT=20%)
- SOLAR_GAIN_HOLD_SECONDS=600: once triggered, stays active for 600s after last trigger
- Default min_elevation=12.0, min_rise_per_hour=0.6
- freeze_integral=True passed to PID when solar_gain is active
- No hysteresis-based sustain required to ACTIVATE (instant on one sample)

C++ (satUpdateSolarGain()):
- Rise rate: EMA of indoor temperature change per hour (alpha=0.3)
- Sun elevation gating: if bSunElevationValid and elevation < fSolarMinElevation, return early ✓
- Boiler low modulation: checks RelModLevel < 30.0f (vs Python's 20%)
- Activation: requires 10 minutes sustained (600000ms) before marking active
- Hold: no equivalent of Python's 600s hold-until timestamp; instead uses 10-min hysteresis to clear

GAPS:
1. Modulation threshold mismatch: Python uses SOLAR_GAIN_MAX_RELATIVE_MODULATION_PERCENT=20% as low-output threshold. C++ uses 30% (`lowModulation = (modulation < 30.0f)`). C++ is 50% more permissive.
2. Activation mechanism differs: Python is instant (one qualifying sample arms the hold_until timer). C++ requires 10-minute continuous sustain before activating. This is more conservative — may miss brief solar spikes.
3. Python: valves_open check is required for solar gain. C++ does not check bValvesOpen in satUpdateSolarGain(). When all valves are closed, solar gain should not apply.
4. Python hold: after the last qualifying sample, stays active for SOLAR_GAIN_HOLD_SECONDS=600s. C++ instead requires 10-min below threshold to clear (hysteresis). Different but comparable behavior.
5. Integral freeze: Python passes freeze_integral=True to PID. C++ checks bSolarGainActive inside _pidUpdateIntegral() — equivalent ✓.

Created audit-fix task for modulation threshold and valve check.
<!-- SECTION:FINAL_SUMMARY:END -->
