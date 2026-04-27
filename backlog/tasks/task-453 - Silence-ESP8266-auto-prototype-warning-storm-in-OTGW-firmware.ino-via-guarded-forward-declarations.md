---
id: TASK-453
title: >-
  Silence ESP8266 auto-prototype warning storm in OTGW-firmware.ino via guarded
  forward declarations
status: To Do
assignee: []
created_date: '2026-04-27 19:21'
labels:
  - build
  - warnings
  - esp32
  - esp8266
  - prototypes
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Context

On the 2.0.0 / OTGW32 / SAT branch, the ESP8266 build emits 30+ warnings of the shape:

```
OTGW-firmware.ino:NNN: warning: 'void <name>()' declared 'static' but never defined [-Wunused-function]
```

Reported on lines 68, 87-91, 193-249, 717. The line numbers point inside the body of `setup()` — they don't match the function names. This is the classic Arduino-IDE / PlatformIO auto-prototype trap: the build pre-pass synthesizes forward declarations at the top of the main `.ino` for every function it finds, regardless of whether the function body sits inside `#ifdef ARDUINO_ARCH_ESP32` (or similar) guards. On an ESP8266 build, the prototype gets emitted but the definition is gone → "declared static but never defined".

## Affected functions (grouped by feature)

- **Ethernet (W5500 / OTGW32)**: `_ensureLEDsInit`, `deriveEthMAC`, `probeW5500`, `startEthernet`, `switchToEthernet`, `switchToWiFi`
- **OLED rendering**: `probeOLED`, `drawHeader`, `drawPageDashboard`, `drawPageHC1`, `drawPageHC2`, `drawPageSystem`
- **OT command queue / override / synth**: `otCmdQueueEmpty`, `otCmdQueueFull`, `otCmdEnqueue`, `otCmdDequeue`, `setOverride`, `clearOverride`, `isUnknownId`, `getUnknownCount`, `incUnknownCount`, `clearUnknownCount`, `updateWriteCache`, `handleSlaveRequest`, `synthesizeResponse(char,char,const char*)`, `synthesizeResponse(const char*,const char*)`, `clearWriteOverride`, `resetTransientState`, `setOTDirectMode`, `checkThermostatTimeout`, `handleMasterModeSlaveFrame`, `setResponseModifier`, `clearResponseModifier`, `clearAllResponseModifiers`, `sendMasterRequestAsync`, `handleMasterResponse`, `scheduleMasterRequest`, `emitSummaryLine`, `enqueueWriteCommand`
- **Bridge / step-up / OT-bus**: `otDirectBridgeEvent`, `otDirectBridgeWriteLine`, `otDirectBridgeProcessStatus`, `bridgeFrameToParser`, `enableStepUp`, `probeOTBus`
- **Flame ratio**: `loopPiCtrl`, `initFlameRatio`, `flameRatioAccum`, `flameRatioSet`, `loopFlameRatio`
- **OTDirect handler**: `handleOTDirect`

## Approaches (KISS first)

1. **Manual guarded forward declarations** in a header (e.g. `platform_esp32.h` or a new `OTGW-firmware-prototypes.h`) with the same `#ifdef ARDUINO_ARCH_ESP32` guards as the definitions. Auto-generated prototype + manual prototype are identical, so no conflict; the unused-function warning disappears because GCC sees the matching declaration is also platform-conditional.
2. **Move definitions out of `OTGW-firmware.ino`** into a `.cpp` / per-platform `.ino` (e.g. `Ethernet.ino`, `OLED.ino`). The auto-prototype generator only inserts decls for the main `.ino`; satellite `.ino` files don't trigger the storm.
3. **Wrap call-sites and definitions together** under the same `#ifdef`, so on ESP8266 there is no reference and no orphan prototype. Often partly true already; auditing what slipped through.

Recommended starting point: option 2 for groups that already have a sibling file (Ethernet → `Ethernet.ino`, OLED → `OLED.ino`), option 1 for the OT-direct / bridge / flame-ratio cluster that lives in `OTGW-firmware.ino` proper.

## Acceptance Criteria
<!-- AC:BEGIN -->
See AC list. Must verify both ESP8266 and ESP32 builds remain clean (or improved) after the refactor.

## Risk

- Moving definitions can hit static linkage assumptions (file-static helpers turn into compile errors if siblings reference them). Mitigation: keep `static` only where definition + all call-sites are in the same translation unit; otherwise drop `static`.
- Auto-prototype generator is sensitive to whitespace/comments; verify by inspecting the preprocessed output if a fix doesn't take.
<!-- SECTION:DESCRIPTION:END -->

- [ ] #1 All ~40 OTGW-firmware.ino auto-prototype warnings on ESP8266 build resolved
- [ ] #2 ESP32 (OTGW32) build remains clean after refactor
- [ ] #3 No behavior change on either platform (no functions accidentally moved out of #ifdef scope)
- [ ] #4 Approach documented in task notes (per group: option 1 / 2 / 3 chosen, with rationale)
- [ ] #5 If new headers added, they follow ADR-079 / ADR-081 component-types convention
- [ ] #6 Smoke test: WiFi boot on ESP8266 still works; OLED + Ethernet boot on ESP32 still works
<!-- AC:END -->
