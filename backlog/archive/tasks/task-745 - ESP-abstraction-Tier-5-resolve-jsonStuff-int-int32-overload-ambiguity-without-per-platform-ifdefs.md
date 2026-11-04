---
id: TASK-745
title: >-
  ESP abstraction Tier 5: resolve jsonStuff int/int32 overload ambiguity without
  per-platform ifdefs
status: Done
assignee:
  - '@claude'
created_date: '2026-05-28 08:29'
updated_date: '2026-06-01 19:17'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 72000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
jsonStuff.ino:389, 487, 660 define extra sendJsonMapEntry(int) / (unsigned int) overloads only on ESP32 because xtensa-esp32 distinguishes int (32-bit) from int32_t (also 32-bit but defined as long). Three resolution paths: (1) Cast all caller sites to int32_t/uint32_t explicitly so the platform-specific overloads disappear (preferred but ~50 callsite touches). (2) Wrap the overloads in a JSON_INT_OVERLOADS_NEEDED macro set per-platform inside platform_*.h - moves the conditional into the abstraction tier. (3) Refactor sendJsonMapEntry into a template (most invasive). Pick the cleanest option after a quick survey of callsite count. Depends on Tier 3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 jsonStuff.ino has no #if defined(ESP32) guard around the ambiguity-overload definitions
- [ ] #2 Resolution approach is documented in an ADR if the strategy isn't obvious from the diff
- [ ] #3 python build.py --firmware exits 0 on both platforms; no warnings about ambiguous overload resolution
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T21:17:53+02:00: DONE — 3b... overload sites gated on PLATFORM_INT_DISTINCT_FROM_INT32 (platform.h). Option 2 chosen (macro, not ~594-callsite cast). Baseline 7->4. Build both SUCCESS (ESP8266 no dup-def confirms it), eval 0-fail.
<!-- SECTION:NOTES:END -->
