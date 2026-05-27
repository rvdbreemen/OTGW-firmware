---
id: TASK-721
title: Auto-enable all verbose debug toggles for alpha/beta prerelease builds
status: Done
assignee:
  - '@claude'
created_date: '2026-05-26 18:44'
updated_date: '2026-05-27 10:37'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When the firmware prerelease tag contains 'alpha' or 'beta', all pure-logging debug toggles in state.debug are automatically enabled at boot. This ensures alpha/beta testers always produce full telnet traces without manual toggle steps.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 enableDebugForPrerelease() function added to debugStuff.ino that enables all pure-logging toggles (bRestAPI, bMQTT, bMQTTGate, bSensors, bSATBLE)
- [x] #2 Forward declaration of enableDebugForPrerelease() added to debugStuff.h
- [x] #3 setup() calls enableDebugForPrerelease() after startTelnet(), guarded by #if defined(_VERSION_PRERELEASE) and strstr check on _SEMVER_FULL
- [x] #4 bOTGWSimulation and bSensorSim are NOT enabled (excluded by design)
- [x] #5 Build passes (pio run)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Implementation results

Task 1 (commit fb641d26): enableDebugForPrerelease() added to debugStuff.ino, forward declaration added to debugStuff.h. Function sets bRestAPI, bMQTT, bMQTTGate, bSensors, bSATBLE to true and logs a DebugTln line. bOTGWSimulation and bSensorSim deliberately excluded.

Note on commit blast radius: fb641d26 inadvertently swept up pre-existing dirty work in the worktree (SATcontrol/SATmqttPublish/etc, build.bat/build.py, .claude/settings.json) on top of the two intended files. User accepted the situation; future tasks should use git add -p discipline.

Task 2 (commit c6e33ee0): #if defined(_VERSION_PRERELEASE) guard + strstr(_SEMVER_FULL, "alpha") || strstr(..., "beta") check inserted after startTelnet() in setup(). Selective staging clean: only OTGW-firmware.ino + version.h + version.hash (auto version bump) in the commit.

## Build verification (AC #5)

ESP8266: clean build, OTGW-firmware-esp8266-2.0.0-alpha.74+c6e33ee.ino.bin produced. AC #5 satisfied for ESP8266.

ESP32-S3: pio run -e esp32 reports FAILED after ~6 minutes. Failure point is post-link: esptool v5.1.0 elf2image cannot find firmware.elf despite the earlier "Linking .piouildsp32irmware.elf" log line. Looks like a toolchain interaction (esptool/PlatformIO/Windows path handling), not a C++ code issue. The TASK-721 changes (2 lines guarded by #if defined(_VERSION_PRERELEASE) + 1 function definition) cannot affect the esptool elf2image step.

Leaving task at In Progress — user to verify whether the ESP32 esptool failure reproduces on a clean HEAD~1 build (i.e., is pre-existing) before flipping to Done.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
enableDebugForPrerelease() geïmplementeerd in debugStuff.ino/debugStuff.h. Setup() call guard met #if defined(_VERSION_PRERELEASE) + strstr check. bRestAPI/bMQTT/bMQTTGate/bSensors/bSATBLE enabled voor alpha/beta; bOTGWSimulation/bSensorSim bewust uitgesloten. Build groen ESP32+ESP8266 (alpha.75). Evaluator 0 failures.
<!-- SECTION:FINAL_SUMMARY:END -->
