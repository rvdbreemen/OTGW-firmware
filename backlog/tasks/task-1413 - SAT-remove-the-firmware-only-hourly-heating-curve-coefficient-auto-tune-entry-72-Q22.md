---
id: TASK-1413
title: >-
  SAT: remove the firmware-only hourly heating-curve coefficient auto-tune
  (entry 72, Q22)
status: Done
assignee: []
created_date: '2026-10-28T06:44:00Z'
updated_date: '2026-10-31T05:10:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 577000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 72, Q22: Remove). Remove the hourly coefficient auto-tune (SATcontrol.ino ~4798-4934, call at ~5252 inside if(roomValid)), its setting/state fields, REST/MQTT/HA discovery entities (retire the retained config, update docs/api/MQTT.md) and v2.js entries. Keep satautogains and the curve recommendation. Q21 (entry 72): 5.0 upper bound stays (ADR-198).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: HEAD with SATautotune on rewrites fHeatingCurveCoeff after 1 h and 6 mostly-overshoot cycles; the fix never writes it (red then green)
- [x] #2 A stored coefficient is unchanged across the upgrade
- [x] #3 Retained HA discovery config for the removed entity is cleared; docs/api/MQTT.md updated
- [x] #4 ADR-198 open question on auto-tune closed with entry 72
- [x] #5 build.bat esp32-combo SUCCESS and evaluate.py --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed: own red/green AC#1, decision entry 72 (ADR-198 still Proposed, waits on Robert).

Review (opus, contract lens): PASS. All remaining SAT switch ids/topics byte-identical (tuple diff base vs fix: only -sat-auto-tune-enable removed); dhw_enable gate index 10 unchanged; retired rows publish empty retained to the exact old config topic and the retained state topics; removed block is entry 72's hourly coefficient auto-tune (writes only fHeatingCurveCoeff), auto gains and curve recommendation intact; old settings.ini keys load cleanly. Stale docs fixed (api/README switch count 12, openapi wording). Test gap: ha-discovery golden skips without dumps; ids verified by tuple comparison.

Integrated on alpha.561 (31118db50) as alpha.562: full suite 72/72 rc=0, evaluate EXIT 0. AC1 test_sat_autotune_removed: base FAILED (coefficient 2.0 -> 1.96 after 1 h + 6 cycles), fix OK (structural; behavioural part skips: function gone). AC2 static (old keys fall through updateSetting; review read the loader). AC3 retire rows verified by review; broker check pending (bench broker offline). AC4 ADR-198 open question: pending (ADR-198 still Proposed per notes). AC5 build after the soak.

2026-10-31 close-out: Shipped in 2.0.0-alpha.564+d6dd798 (pushed origin/dev 8744d985a..d6dd798f1, zip Discord msg 1565955628164186185, report msg 1565955677522894960). Build esp32-combo SUCCESS; evaluate --quick on d6dd798f1 Failed 0, 100%; all 73 tests/test_*.py files OK. Bench OTGW32 .61, OTA app + filesystem, 173 settings keys kept. AC#1 fresh: tests/test_sat_autotune_removed.py with OTGW_SATCONTROL_INO = SATcontrol.ino of 4fadbb38c (pre-fix): FAILED (1): coefficient after 1 h + 6 mostly-overshoot cycles 1.9600 (settings writes: 1); on d6dd798f1 OK (real-function case skipped: satAutoTuneUpdate no longer exists). AC#2 bench: satcoefficient 0.6 on alpha.535 and 0.6 after the OTA to alpha.564; satautotune/satautotunerate gone. AC#3 bench: retained homeassistant/switch/otgw-1020BA21B4F8/sat_auto_tune_enable/config plus OTGW/value/.../sat/auto_tune, auto_tune_rate (and deadband) seeded on the device's broker, reboot: all four received an empty retained publish and none remained retained; docs/api/MQTT.md updated in 02029bf75. AC#4: ADR-198 open question closed with entry 72 (line 184). AC#5: build + evaluate above.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the firmware-only hourly auto-tune of the heating-curve coefficient with its settings, REST fields, MQTT topics and HA switch (Sergeant D entry 72). Evidence: AC#1 old code rewrites 2.0 -> 1.96, fix has no writer; AC#2 bench coefficient 0.6 kept across the upgrade; AC#3 bench retained discovery and state topics cleared after reboot; AC#4 ADR-198 entry 72; AC#5 alpha.564 build + evaluate green. BREAKING for automations on sat/auto_tune. alpha.562, published in alpha.564+d6dd798.
<!-- SECTION:FINAL_SUMMARY:END -->
