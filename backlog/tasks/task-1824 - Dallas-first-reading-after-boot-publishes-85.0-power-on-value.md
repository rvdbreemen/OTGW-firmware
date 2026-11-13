---
id: TASK-1824
title: 'Dallas: first reading after boot publishes 85.0 power-on value'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T21:39:00Z'
updated_date: '2026-11-13T21:05:00Z'
labels:
  - sensors
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 908000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while answering thaghostnl in #nederlandse-ondersteuning (msg https://discord.com/channels/812969634638725140/815561033036333076/1570190651684036729, 'telkens een brown-out 85 graden'; answer 1570191069813936168). Own code finding, a bug. initSensors() sets sensors.setWaitForConversion(false) (sensors_ext.ino:161). pollSensors() calls sensors.requestTemperatures() (sensors_ext.ino:233) and then sensors.getTempC() in the same tick (sensors_ext.ino:264), so every read returns the PREVIOUS conversion. On the first poll after boot no conversion has completed yet and the scratchpad still holds the DS18B20 power-on value 85.0; it is stored in DallasrealDevice[i].tempC and published to MQTT and the UI. Only DEVICE_DISCONNECTED_C (-127) is filtered (sensors_ext.ino:265-269). Parasite-mode behaviour (DallasTemperature.cpp:103-104, :470) stays open until thaghostnl answers about his wiring; out of scope here.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host test or bench capture shows 85.0 as the first value after boot on the old code
- [x] #2 The fix publishes no value before a completed conversion (for example skip the first read after requestTemperatures, or read in the next tick); the same test is green
- [x] #3 No valid measurement is lost after that: a real 85.0 after a valid previous value is still published, unless it jumps
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Read the scratchpad only after a conversion was started on an earlier poll (static dallasConversionRequested, set right after requestTemperatures(); the first poll after boot only starts one). Filter an exact 85.0 (the DS18B20 power-on scratchpad value 0x0550) when the sensor has no valid value yet or the step from its previous value is more than 10 °C (a brown-out reset); a real 85.0 reached in steps passes. -127 stays filtered. Everything else of pollSensors() unchanged. Host test compiles the real pollSensors() against a DS18B20 scratchpad model. Own decision, no maintainer question: the AC already asks for exactly this; the 10 °C jump is a choice (a poll is 1..3600 s, default 20 s; water or room temperature does not step 10 °C to exactly 85.0 in one poll), recorded here.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 21:42 UTC thaghostnl (msg 1570191896427499614): waterproof probe, cable about 1 m; he sees 85 in the stats and in MQTT; photo of the wiring to follow. Not yet clear whether it stays at 85 (wiring/parasite) or is only the first value (this bug).

2026-11-13 fix in lane/ba-1824 (uncommitted, bump waits for the build slot): sensors_ext.ino marker block 'dallas-read record' (dallasConversionRequested, DALLAS_POWER_ON_C 85.0, DALLAS_POWER_ON_JUMP_C 10.0, dallasReadingValid()), pollSensors() reads only when conversionDone and filters through dallasReadingValid(). New tests/test_dallas_first_reading.py compiles the real pollSensors() (+ the block) against a DS18B20 model (85.0 at power-on; a conversion completes before the next poll). origin/dev 3b1221c46: FAILED 2: boot publishes [85.0, 21.5, 21.5]; brown-out publishes [85.0, 21.5, 21.5, 85.0, 21.5]; controls (real 85 in steps published, -127 skipped) ok. Fix: OK 4/4. Also green: test_ino_prototype_hoisting, test_board_pin_reserved, test_fs_update_keep, test_setting_gpio_range, test_ha_discovery_json, test_sat_zone_aggregation, test_static_integrity; evaluate.py --quick 85 PASS 0 FAIL. Side effect, by design: every published value is one poll interval old, as before (the read still returns the previous conversion). Parasite power stays out of scope (task description). Open: build + bench by 09 (first value after boot not 85 on a unit with a DS18B20, if 09 has one).

2026-11-13 rebased onto 746ccd426, sharing one build with the TASK-1885 titles follow-up (2ec6d946d, beta.228). After the rebase: test_dallas_first_reading 4/4, test_ino_prototype_hoisting, test_board_pin_reserved, test_fs_update_keep, test_setting_gpio_range, test_static_integrity green; test/host 26/27 green; the one red, test/host/test_ot_support_migration.py (CASE M5/M6), is also red on clean origin/dev 746ccd426 (checked in a detached worktree), a known pre-existing red fixed by d3 in 9cdae6938 (test only, waiting in the push queue), not from this lane. evaluate.py --quick 85 PASS 0 FAIL.
<!-- SECTION:NOTES:END -->
