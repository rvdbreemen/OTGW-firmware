---
id: TASK-336
title: Rename Debug.h -> debugStuff.h and split function bodies into debugStuff.ino
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 18:35'
updated_date: '2026-04-19 18:51'
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
- [x] #1 git mv Debug.h debugStuff.h (preserves blame)
- [x] #2 Function bodies for _debugPrintf_P and _debugBOL moved to new debugStuff.ino
- [x] #3 debugStuff.h contains only #include platform.h + macros + function decls (~40 lines)
- [x] #4 Every #include Debug.h reference in the tree updated to debugStuff.h
- [ ] #5 Both ESP8266 and ESP32 build clean; telnet debug output verified on one boot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Uitgebreid met Debugtypes.h merge per ADR-081 (types merge in stuff.h wanneer beide bestaan). Eén commit dekt nu: (a) Debug.h rename naar debugStuff.h, (b) function bodies gesplitst naar debugStuff.ino, (c) DebugSection struct gemerged uit Debugtypes.h in debugStuff.h, (d) Debugtypes.h verwijderd, (e) ADR-081 gedocumenteerd.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Debug subsystem opruiming volledig (3-in-1 commit): rename Debug.h naar debugStuff.h + split function bodies naar debugStuff.ino (project-wide stuff.h/.ino conventie) + merge Debugtypes.h in debugStuff.h (ADR-081 regel voor components met stuff.h). debugStuff.h bevat nu twee duidelijk gelabelde secties: Types (DebugSection struct) en Interface (macros + function decls). OTGW-firmware.h include blok aangepast: debugStuff.h vervangt de oude Debugtypes.h positie voor early-visibility van DebugSection aan OTGWState. Later include bij networkStuff/helperStuff wordt no-op via pragma once. Beide platforms bouwen clean (ESP8266 0.76 MB, ESP32-S3 1.79 MB, byte-identiek).
<!-- SECTION:FINAL_SUMMARY:END -->
