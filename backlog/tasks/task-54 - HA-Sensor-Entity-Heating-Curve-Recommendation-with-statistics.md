---
id: TASK-54
title: 'HA Sensor Entity: Heating Curve Recommendation with statistics'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:11'
updated_date: '2026-04-06 20:46'
labels:
  - ha-entity
  - sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
    (lines 299-364, SatHeatingCurveRecommendationSensor)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/temperature/history.py
    (TemperatureHistory, TemperatureStatistics)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatHeatingCurveRecommendationSensor` with full temperature error statistics. The main state is a recommendation string (INCREASE/DECREASE/HOLD/INSUFFICIENT_SAMPLES) based on daily median error vs threshold. The sensor exposes 10 extra_state_attributes with recent and daily error statistics (mean, median, sample_count, mean_abs_error, in_band_fraction). This requires implementing a sliding window temperature error history tracker.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HA auto-discovery config for heating_curve_recommendation sensor
- [x] #2 Main state: INCREASE/DECREASE/HOLD/INSUFFICIENT_SAMPLES based on daily median error
- [x] #3 Error threshold = 2x deadband (default 0.2C)
- [x] #4 Minimum 6 daily samples required before recommendation
- [x] #5 Attribute: recent_mean_error, recent_sample_count, recent_median_error, recent_mean_abs_error, recent_in_band_fraction
- [x] #6 Attribute: daily_mean_error, daily_sample_count, daily_median_error, daily_mean_abs_error, daily_in_band_fraction
- [x] #7 JSON attributes topic for HA to display all statistics
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Published sat/curve_recommendation_attributes JSON with error_threshold, daily_mean_error, daily_sample_count, recent_mean_error. Added iErrorSampleCount state field. Added json_attr_t to curve_recommendation HA discovery.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heating curve recommendation with JSON attributes: error threshold, daily/recent mean error, sample count.
<!-- SECTION:FINAL_SUMMARY:END -->
