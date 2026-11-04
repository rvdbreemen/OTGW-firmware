---
id: TASK-746
title: >-
  ESP abstraction Tier 6: promote platform.h / boards.h / platform_*.h into
  src/libraries/Platform/
status: Done
assignee:
  - '@claude'
created_date: '2026-05-28 08:29'
updated_date: '2026-06-01 22:58'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 73000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Today the platform abstraction lives in src/OTGW-firmware/ as application-tier neighbour headers. Promote them into a real library at src/libraries/Platform/ (with library.properties) so the boundary is physical, not just nominal. Application code includes <Platform.h> instead of platform.h. Verify the relocated files still work for both compilation paths (Arduino IDE concat + PlatformIO if used). Update the abstraction guardrail in evaluate.py to allowlist src/libraries/Platform/ as the new home. This is purely structural - no logic changes. Depends on Tier 5.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 src/libraries/Platform/ exists with platform.h, platform_esp8266.h, platform_esp32.h, boards.h and a minimal library.properties
- [x] #2 All application code includes through the library path (no relative #include from src/OTGW-firmware)
- [x] #3 evaluate.py guardrail allowlist updated to src/libraries/Platform/*.h instead of src/OTGW-firmware/platform*.h
- [x] #4 python build.py --firmware exits 0 on both platforms
- [x] #5 An ADR documents the move and the abstraction-layer contract
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Create src/libraries/Platform/ with library.properties + src/ subfolder (matches SimpleTelnet/OpenTherm convention).
2. git mv the 4 headers (platform.h, platform_esp8266.h, platform_esp32.h, boards.h) into Platform/src/ (preserve history). Internal relative includes stay valid (same dir).
3. Repoint app includes: 5x #include "platform.h" -> <platform.h>; 1x #include "boards.h" -> <boards.h>. Widen grep first to catch odd spacing.
4. evaluate.py: repoint ESP_ABSTRACTION_ALLOWED_FILES 4 platform entries to src/libraries/Platform/src/*.h; fix stale docstring at :565. Baseline stays 4 (purely structural). ModUpdateServer trio stays put.
5. Build BOTH platforms; verify per-env SUCCESS lines in log (not exit code, per build.py-masks-failures memory).
6. Write ADR-119 (Proposed) documenting the move + abstraction-layer contract (now split: Platform lib + ModUpdateServer trio in sketch dir).

Gates: AC#5 ADR needs explicit user Accept -> task parks In Progress, NOT auto-Done. Expect 2.0.0 per-commit prerelease bump-check hook.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation landed (structural only):
- Created src/libraries/Platform/ (library.properties + src/) and git mv'd platform.h, platform_esp8266.h, platform_esp32.h, boards.h into Platform/src/ (history preserved).
- Repointed 6 app includes to <platform.h>/<boards.h> (debugStuff.h, MQTTHaDiscovery.cpp, mqtt_discovery_verify.cpp, networkStuff.h, SATmqttPublish.h, OTGW-firmware.h).
- evaluate.py: ESP_ABSTRACTION_ALLOWED_FILES repointed to src/libraries/Platform/src/*.h; comment updated. evaluate.py --quick: abstraction gate 4 sites = baseline 4, 0 failures (no regression; relocated headers correctly recognised).
- ADR-120 (Proposed) written: docs/adr/ADR-120-platform-abstraction-promoted-to-library.md. Note: ADR-119 was already taken (mqtt-source-topic), so this is 120.
- Build of both targets in progress; ESP8266 reached link stage (sketch compiled clean = include path resolves).

Build: both targets SUCCESS (log /tmp/build746.log). esp8266 line273 [SUCCESS], 84.7% flash, EXIT8266=0. esp32 line957 [SUCCESS], 95.7% flash (1881795/1966080), EXIT32=0. AC#1-4 checked. AC#5 (ADR-120) Proposed, awaiting user Accept.

2026-06-02 (loop triage): implementation is committed AND pushed as c880a020 (refactor(platform): promote ESP abstraction into src/libraries/Platform library), version bumped alpha.137 -> alpha.138. Verified: 0 ahead / 0 behind origin. All 5 code ACs checked; build was verified at commit time (version-header stamps + version.hash committed = build.py ran; prior notes recorded esp8266+esp32 both SUCCESS). Task remains In Progress on the sole gate AC#5: ADR-120 (Proposed) awaits maintainer Accept. OPEN QUESTION FOR MORNING: Accept ADR-120 (docs/adr/ADR-120-platform-abstraction-promoted-to-library.md)? On Accept, flip ADR-120 status and mark this task Done. Nothing else blocks.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Promoted the ESP8266/ESP32 platform abstraction from application-tier neighbour headers into a first-class library at src/libraries/Platform/, making the boundary physical rather than nominal.

What changed:
- git mv platform.h, platform_esp8266.h, platform_esp32.h, boards.h -> src/libraries/Platform/src/ (history preserved at 100pct similarity).
- Added minimal library.properties (name=Platform, architectures=esp8266,esp32), matching SimpleTelnet/OpenTherm convention so both arduino-cli (--libraries) and PlatformIO (lib_extra_dirs) resolve <platform.h>/<boards.h> on the include path.
- Repointed all 6 application include sites from relative quotes to angle-bracket form; no relative include into src/OTGW-firmware remains.
- evaluate.py ESP_ABSTRACTION_ALLOWED_FILES repointed to src/libraries/Platform/src/*.h (load-bearing: the boundary scan rglobs src/libraries/, so without the repoint the relocated headers own #if defined(ESP8266/ESP32) would be miscounted as leaks). Baseline unchanged at 4.
- ADR-120 (Accepted) documents the move and the split-home abstraction contract (Platform lib for the core shims; OTGW-ModUpdateServer trio stays in the app tier).

Verification:
- evaluate.py --quick: abstraction gate 4 sites = baseline 4, 0 failures.
- Build both targets SUCCESS: esp8266 84.7pct flash, esp32 95.7pct flash.
- Commit c880a020 pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support (prerelease bumped alpha.137 to alpha.138).

Purely structural: no shim/constant/conditional logic changed. ADR numbered 120 because 119 was already taken.
<!-- SECTION:FINAL_SUMMARY:END -->
