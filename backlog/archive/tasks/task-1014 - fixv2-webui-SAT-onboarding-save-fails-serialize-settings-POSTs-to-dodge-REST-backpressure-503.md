---
id: TASK-1014
title: >-
  fix(v2-webui): SAT onboarding save fails - serialize settings POSTs to dodge
  REST backpressure 503
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 21:04'
updated_date: '2026-07-06 04:52'
labels: []
dependencies: []
ordinal: 225000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT onboarding wizard's final Enable step showed 'Could not save the settings, SAT was not enabled'. commit() fired 6-7 settings POSTs concurrently via Promise.all; the REST backpressure gate (restAPI.ino:2455, TASK-884) caps concurrent /api requests at 4 (2 under <24KB maxblock, 1 under <16KB), so the excess got a cheap 503, Promise.all rejected, and the commit failed - sometimes after satenabled was the dropped write, leaving a partial save. Fix: land settings one at a time via a sequential promise chain. Same pattern applied to the MQTT onboarding commit, testConn, and the weather lat/lon detect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT onboarding commit lands settings sequentially (no Promise.all over POSTs)
- [x] #2 Never more than one settings POST in flight at once
- [x] #3 MQTT onboarding commit, testConn, and weather-detect also serialized
- [x] #4 Prerelease bumped and LittleFS filesystem rebuilt
- [x] #5 On-device: complete SAT onboarding, verify no 503 and SAT enabled
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Committed cce5c743 (bump alpha.327->alpha.328 + fix, 53 files). Serialized all 11 burst sites: v2.js x5, index.js x2, sat.js x3, index.html CSS links. evaluate.py --quick: 0 fail. Promise.all-over-fetch grep: clean.

BLOCKED on build + on-device verify: this machine has only Python 3.14.2 and no ~/.platformio toolchain. build.py pip-installs pio under 3.14, which cannot run the espressif32 platform (needs 3.10-3.13), and masks the failure with exit 0 (the known build.py-lies trap). .pio/build/esp32/littlefs.bin is stale (prior session, 11:59). AC4 (fs rebuild) and AC5 (on-device) cannot be verified until a Python 3.10-3.13 + toolchain is available. NOT pushed to origin/dev (push policy requires a verified build).

Build now GREEN via build.bat (exit 0): fresh firmware.bin + littlefs.bin (23:45, alpha.328+cce5c74) + merged.bin + merged-full.bin. Required fixing the build toolchain itself (committed 9afb7428): build.bat enforces Python 3.10-3.13 (forces portable-3.12.10 bootstrap on a 3.14-only host); build.py runs 'python -m platformio' (embed python has no pio.exe); build.py honours PLATFORMIO_CORE_DIR (core relocated to D:\DevData\platformio). Pushed cce5c743 + 9afb7428 to origin/dev. AC5 (on-device: flash + walk SAT onboarding, confirm no 503 + SAT enabled) remains -- hardware/field verify.

On-device verification done via a new CDP-driven functional test (scripts/verify_sat_onboarding.py), not manual clicking - drove the actual wizard DOM (button.click() on real data-sa elements, exactly the code path a user's click fires) on the live bench device (192.168.88.64, esp32-classic, alpha.330/commit 190dbbdfc, which already includes the cce5c743 serialization fix). Flow: nav to SAT page -> toggle #satEnable (satOnbNeeded()==true on a fresh device intercepts this and opens the wizard, per v2.js:3221-3227) -> Welcome->Get started->Radiators->Continue->Auto-detect->Continue->Automatic->Continue->(sources health loads)->Enable SAT. Result: wizard reached the 'done' screen, all 7 commit() settings POSTs returned 200 (0 503s), and a post-check GET /api/v2/settings confirmed satenabled=true and sat_onboarded=true. 7 screenshots captured as evidence (welcome, each wizard step, final done screen) and sent to the maintainer. Found and fixed two script bugs along the way: (1) the wizard does NOT auto-show on page load for a never-onboarded device - that's the 'migrate' path gated on satenabled already being true; a fresh device needs the #satEnable toggle click, which is the 'enable' path (v2.js:3226); (2) a selector-quoting bug in the click helper (naive single-quote wrapping broke selectors containing their own quotes like [data-sa='sys-radiators'] - fixed with JSON.stringify encoding).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC#5 (final remaining AC) confirmed on-device: the SAT onboarding wizard, driven exactly as a real user would via its actual DOM buttons on the live bench device, completes with zero 503s and both satenabled/sat_onboarded correctly set to true. All 5 ACs now pass. New reusable tool: scripts/verify_sat_onboarding.py (CDP-driven, screenshots each step) for future regression checks of this flow.
<!-- SECTION:FINAL_SUMMARY:END -->
