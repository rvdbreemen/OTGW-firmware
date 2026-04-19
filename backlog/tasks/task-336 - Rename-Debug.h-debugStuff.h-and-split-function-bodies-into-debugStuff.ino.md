---
id: TASK-336
title: Rename Debug.h -> debugStuff.h and split function bodies into debugStuff.ino
status: To Do
assignee: []
created_date: '2026-04-19 18:35'
labels:
  - architecture
  - cleanup
  - review-2026-04-18-followup
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Debug.h holds 2 non-inline non-static function bodies (_debugPrintf_P, _debugBOL) in a header. Works today only because the file is included via a single TU chain (OTGW-firmware.h). Any future standalone .cpp that includes Debug.h transitively will hit multiple-definition linker errors. Project convention elsewhere (MQTTstuff, helperStuff, networkStuff, OTGW-Core) is .h for decls + .ino for bodies. Rename aligns Debug with that convention and removes the latent fragility.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 git mv Debug.h debugStuff.h (preserves blame)
- [ ] #2 Function bodies for _debugPrintf_P and _debugBOL moved to new debugStuff.ino
- [ ] #3 debugStuff.h contains only #include platform.h + macros + function decls (~40 lines)
- [ ] #4 Every #include Debug.h reference in the tree updated to debugStuff.h
- [ ] #5 Both ESP8266 and ESP32 build clean; telnet debug output verified on one boot
<!-- AC:END -->
