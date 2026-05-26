---
id: TASK-721
title: Auto-enable all verbose debug toggles for alpha/beta prerelease builds
status: To Do
assignee: []
created_date: '2026-05-26 18:44'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When the firmware prerelease tag contains 'alpha' or 'beta', all pure-logging debug toggles in state.debug are automatically enabled at boot. This ensures alpha/beta testers always produce full telnet traces without manual toggle steps.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 enableDebugForPrerelease() function added to debugStuff.ino that enables all pure-logging toggles (bRestAPI, bMQTT, bMQTTGate, bSensors, bSATBLE)
- [ ] #2 Forward declaration of enableDebugForPrerelease() added to debugStuff.h
- [ ] #3 setup() calls enableDebugForPrerelease() after startTelnet(), guarded by #if defined(_VERSION_PRERELEASE) and strstr check on _SEMVER_FULL
- [ ] #4 bOTGWSimulation and bSensorSim are NOT enabled (excluded by design)
- [ ] #5 Build passes (pio run)
<!-- AC:END -->
