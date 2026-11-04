---
id: TASK-1336
title: >-
  Settings file stores every float with 2 decimals: weather lat/lon lose
  precision on each save
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T22:28:00Z'
updated_date: '2026-10-23T23:16:00Z'
labels:
  - settings
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 459000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-24 during TASK-1319/1334 bench OTA: lat/lon entered as 52.1355/6.1827 read back as 52.14/6.18 after a settings write. settingStuff.ino:224-227 writeJsonFloatKV() uses dtostrf(value, 1, 2, ...) for every float key, so each writeSettings() (any settings save, also the filesystem-OTA restore) rounds coordinates to ~1 km. REST GET shows the RAM value until the next save/reboot. Fix: per-key precision (coordinates 4-5 decimals) or a shortest-round-trip float format; check every float key read back (targettemp, coefficient, offsets) keeps its value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host or bench test: writeSettings + readSettings round-trip keeps lat/lon to at least 4 decimals (red on current code: 52.1355 -> 52.14)
- [x] #2 No other float setting changes value across a write/read round-trip
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: writeJsonFloatKV() takes a decimals parameter (default 2, every other key byte-identical); SATweatherlat/lon 5, SATthermalcoeff/SATautotunerate 4. tests/test_settings_float_roundtrip.py compiles the real writeJsonFloatKV/parseJsonKVLine/clampSettingFloat cut from settingStuff.ino: HEAD 2 FAIL (lat written 52.14, lon 6.18; thermalcoeff 0.0975 -> 0.10, autotunerate 0.0348 -> 0.03), fix OK (52.13550 / 6.18270); the range audit over every float key flags only those four on HEAD. Bench (OTGW32, working-tree build): POST lat 52.13554 / lon 6.18268, settings.ini holds 52.13554 / 6.18268 before and after a reboot (uptime 0). Review (opus): buffer 20 bytes fits all clamped values (checked with the ESP32 dtostrf), default argument compiles (combined build SUCCESS). Noted: REST GET shows lat/lon at 4 and autotunerate at 3 decimals (display only); the test stubs dtostrf with snprintf.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Float settings that need more than 2 decimals are no longer rounded on save: weather lat/lon keep 5 decimals, thermal coefficient and auto-tune rate 4; all other keys unchanged. Evidence: AC1 host round-trip red on HEAD (52.14) / green (52.13550) plus bench reboot keeping 52.13554/6.18268; AC2 range audit over every float key passes. Not changed: REST/MQTT display precision.
<!-- SECTION:FINAL_SUMMARY:END -->
