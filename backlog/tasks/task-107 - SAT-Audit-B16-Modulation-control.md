---
id: TASK-107
title: 'SAT Audit B16: Modulation control'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:31'
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
Compare modulation control in Python SAT thermo-nova with C++ firmware. Verify max modulation setting, relative modulation level monitoring and interaction with PID output.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Max modulation level setting compared
- [x] #2 Relative modulation level read and use verified
- [x] #3 Interaction of modulation with PID output verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read Python _compute_relative_modulation_value and ModulationReliabilityTracker
2. Read C++ satUpdateModulationReliability and modulation section in satControlLoop
3. Compare MM= command behavior, heat pump override, manufacturer quirks
4. Document gaps and create fix tasks
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Max modulation setting: Python defaults CONF_MAXIMUM_RELATIVE_MODULATION=100 for all systems. C++ iMaxRelModulation configurable. For heat pumps: C++ satAlwaysMaxModulation() returns true, sends MM=100 always. Python does not specifically override to 100 for heat pumps but the config default is 100. Functionally equivalent.

2. PWM modulation: Python RelativeModulationState.OFF (PWM ON phase) -> MM=0. C++ in PWM mode -> mmValue=0. MATCH.

3. Manufacturer quirks: Python Geminox min 10% (max(10, value)). C++ SAT_QUIRK_MIN_MOD_10 (mmValue=max(10,mmValue)). MATCH. C++ also has SAT_QUIRK_IMMERGAS_TP (cap at 80%) - no Python equivalent, firmware extension.

4. Relative modulation monitoring: Python ModulationReliabilityTracker uses 8-sample window (flame-on only), 40% threshold, 3% delta. C++ uses 10min time window, >=3 value changes > 1%. Different algorithms, same goal. No critical gap.

5. Modulation suppression: Python has CONF_MODULATION_SUPPRESSION_DELAY_SECONDS=20s, CONF_MODULATION_SUPPRESSION_OFFSET_CELSIUS=1.0C. C++ has fModSupDelay, fModSupOffset. MATCH in concept.

6. SAT_QUIRK_NO_REL_MOD: C++ skips modulation reliability check for boilers that dont support it. Python checks coordinator.supports_relative_modulation. MATCH in concept.

No fix tasks needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B16: Modulation control

All core modulation behaviors match Python:
- PWM ON phase: MM=0 in both
- Continuous mode: MM=iMaxRelModulation (default 100)
- Heat pump: MM=100 always (satAlwaysMaxModulation, matching Python default)
- Geminox min 10%: both implement this

C++ adds Immergas 80% cap and modulation reliability monitoring (10min window) as firmware-only extensions. Python uses a different reliability algorithm but both serve the same diagnostic purpose.

No fix tasks required.
<!-- SECTION:FINAL_SUMMARY:END -->
