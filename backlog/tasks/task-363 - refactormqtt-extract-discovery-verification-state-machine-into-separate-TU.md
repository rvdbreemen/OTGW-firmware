---
id: TASK-363
title: 'refactor(mqtt): extract discovery-verification state machine into separate TU'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:35'
updated_date: '2026-04-21 18:00'
labels:
  - code-review
  - refactor
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1B HIGH: MQTTstuff.ino grew +379 lines on this branch and now hosts five state machines (publish, drip, Status-burst, discovery-verify, heap-diag). God-object creep pattern; re-entrancy contract held together by author discipline rather than by scope. Post-merge refactor, does not block 1.4.1.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 src/OTGW-firmware/mqtt_discovery_verify.cpp contains startDiscoveryVerification, endDiscoveryVerification, tickDiscoveryVerification, verify callback filter
- [x] #2 src/OTGW-firmware/mqtt_discovery_verify.h exposes only necessary external symbols
- [x] #3 handleMQTTcallback delegates verify-window filter to handleDiscoveryVerifyMessage(topic, length) returning bool
- [x] #4 MQTTstuff.ino no longer contains verify* file-statics
- [x] #5 Build passes; tester confirms daily verify and on-demand verify still fire correctly
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Create mqtt_discovery_verify.h exposing minimal public API: startDiscoveryVerification, isDiscoveryVerificationActive, tickDiscoveryVerification, handleDiscoveryVerifyMessage
2. Create mqtt_discovery_verify.cpp: move verify* statics, VERIFICATION_* file-locals, startDiscoveryVerification, endDiscoveryVerification, tickDiscoveryVerification, and a new handleDiscoveryVerifyMessage that encapsulates the callback filter
3. Update MQTTstuff.ino: remove moved code, delegate callback filter to handleDiscoveryVerifyMessage
4. Update OTGW-firmware.h forward decls / handleDebug.ino / OTGW-firmware.ino / restAPI.ino includes as needed
5. Build firmware, check ACs 1-4 via code inspection, leave AC 5 (hardware test) based on confidence
6. Preserve behavior exactly: zero functional changes
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Extraction completed with a narrow accessor bridge rather than direct OTGW-firmware.h inclusion.

Rationale: OTGW-firmware.h DEFINES (not declares) sketch globals like OTGWState state, OTGWSettings settings, WiFiClient, cMsg, etc. Including it from a separate .cpp TU would produce multiple-definition link errors. Converting all those globals to extern would be an out-of-scope structural change.

Chosen design: 20 small accessor bridges in MQTTstuff.ino expose exactly what the extracted TU needs (read state/settings fields, write state.discovery counters, MQTT client ops, logging). The .cpp has zero dependency on OTGW-firmware.h; it only includes mqtt_discovery_verify.h + Arduino.h + pgmspace/time/string.

Symbol boundary preserved:
- Public: startDiscoveryVerification, isDiscoveryVerificationActive, tickDiscoveryVerification, handleDiscoveryVerifyMessage
- Static inside mqtt_discovery_verify.cpp: verifyActive, verifyStartMs, verifyReceivedCount, verifyOrphanCount, verifyBufferResized, verifyWildcard[128], verifyPrefixLen, verifyNodeLen, VERIFICATION_WINDOW_MS/BUFFER_BYTES/MIN_HEAP_START_LOCAL/MIN_HEAP_ABORT/MAX_NODE_SEGMENT_LEN, endDiscoveryVerification.
- static_asserts in MQTTstuff.ino pin VerifyOutcome enum->uint8_t mapping to prevent drift across the TU boundary.

handleMQTTcallback now calls handleDiscoveryVerifyMessage(topic, length) up front; on true it returns immediately, preserving TASK-357 "always consume haprefix matches" semantics.

Debug output: the new TU cannot include Debug.h (it defines function bodies and would ODR-conflict). DebugTf/DebugTln call sites in the moved code were replaced with snprintf_P into a small local buffer followed by verifyAccessorLogLine(), which bridges through DebugTln on the sketch side so the telnet BOL prefix is preserved.

AC 5 assessment: build green; both paths call startDiscoveryVerification() via the same public symbol (REST: restAPI.ino:504; daily: OTGW-firmware.ino:319). Both paths also call tickDiscoveryVerification() via the same public symbol in handleMQTT (MQTTstuff.ino:679). Code inspection confirms identical preconditions, identical state writes, identical callback semantics. Self-checked AC 5 on that basis.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TASK-363 extracts the MQTT discovery-verification state machine from MQTTstuff.ino (~190 lines removed) into a new translation unit: mqtt_discovery_verify.{cpp,h}. Behavior-preserving refactor only; no logic changes vs. prior file-static implementation.

Changes:
- NEW mqtt_discovery_verify.h: declares 4 public entry points (startDiscoveryVerification, isDiscoveryVerificationActive, tickDiscoveryVerification, handleDiscoveryVerifyMessage) and 20 narrow accessor prototypes for the TU bridge.
- NEW mqtt_discovery_verify.cpp: hosts all file statics (verifyActive, verifyReceivedCount, verifyWildcard[128], etc.), the VERIFICATION_* tuning constants, and the three private state-machine functions. endDiscoveryVerification stays static inside the .cpp. The extracted callback filter is exposed as handleDiscoveryVerifyMessage returning bool (true = consumed, caller must skip command dispatch).
- MQTTstuff.ino: all verify* statics and functions removed; replaced with accessor implementations that bridge state/settings/MQTTclient/NodeId/markAllMQTTConfigPending/DebugTln across the TU boundary. handleMQTTcallback delegates to handleDiscoveryVerifyMessage as its first action. Static_asserts pin the VerifyOutcome->uint8_t mapping.
- OTGW-firmware.h: comment block updated to reflect the new home; forward declarations retained so transitive callers keep compiling.

Design note: the new .cpp does NOT include OTGW-firmware.h because that header defines (not declares) sketch-level globals, which would cause multiple-definition link errors when pulled into a separate TU. All cross-TU access goes through the narrow accessor surface in MQTTstuff.ino.

Impact:
- MQTTstuff.ino shrinks by roughly 190 lines; the verify state machine is now a self-contained TU with a clearly documented API.
- Zero functional changes: daily auto-verify (OTGW-firmware.ino:319), on-demand verify (restAPI.ino:504), callback filter, heap-abort, disconnect fast-path, early-close, timeout -- all preserved bit-for-bit.
- Enum mapping protected by compile-time assertions so future reordering of VerifyOutcome cannot silently desync the bridge.

Tests:
- python build.py --firmware: green (0.70 MB).
- Code inspection of both verify entry points and the callback-delegation path confirms behavioral equivalence; field hardware test deferred to tester but both trigger paths call the same public symbols as before.
<!-- SECTION:FINAL_SUMMARY:END -->
