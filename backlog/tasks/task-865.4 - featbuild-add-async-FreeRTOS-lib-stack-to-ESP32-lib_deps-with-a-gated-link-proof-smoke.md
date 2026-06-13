---
id: TASK-865.4
title: >-
  feat(build): add async/FreeRTOS lib stack to ESP32 lib_deps with a gated
  link-proof smoke
status: Done
assignee:
  - '@claude'
created_date: '2026-06-13 05:42'
updated_date: '2026-06-13 11:23'
labels:
  - async-esp32s3
dependencies: []
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 foundation; SOLE owner of the async lib_deps)
Add the async stack to `[env:esp32].lib_deps` (esp32-classic/combo inherit via extends): ESP32Async/AsyncTCP (pin FIRST, lock the maintained fork), ESP32Async/ESPAsyncWebServer (incl. AsyncWebSocket), espMqttClient (bertmelis, plain non-async variant). seq7/9/10/11 are pure consumers and MUST NOT re-declare these.

## What
- platformio.ini:165-174 ([env:esp32] lib_deps, next to EthernetESP32/NimBLE) add the three with ADR-123 comments. Resolve exact version pins via `pio pkg search` + cross-check other-projects/EMS-ESP32-dev/platformio.ini (the blueprint); record pins in notes.
- gated link-proof smoke in `src/libraries/Platform/src/platform_esp32.h:19-29` (allowlisted include aggregator): reference one symbol per lib (e.g. `(void)&Sym`) so -flto does not dead-strip; NOT a bare include. Keep strippable to limit flash.
Independence: ESP32-gated, builds clean while esp8266 may still be in tree (could parallelize seq1-3).

## Acceptance Criteria
- build: `build.py --target esp32` exit 0, log shows AsyncTCP+ESPAsyncWebServer+espMqttClient resolved/linked; esp32-classic+esp32-combo also exit 0 (inherited deps link on all three).
- build: per-target flash usage captured; esp32-combo stays <100% of the 1.875MB app slot (ADR-127 ~98.4%, ~30KB headroom); if overflow, scope the combo smoke strippable or build link-proof only on esp32.
- build: smoke REFERENCES a symbol per lib (not dead-stripped under -flto; confirm in link map).
- evaluator: `evaluate.py --quick` no new failures; check_esp_abstraction_boundary green (includes in platform_esp32.h, no raw #ifdef ESP32 in .ino).
- chosen version pins recorded in task notes with source (pio pkg search + EMS-ESP32 platformio.ini).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-865.4 implemented (build/evaluator ACs only; NOT committed/bumped — Land phase owns that).

CHOSEN PINS (added ONCE to [env:esp32].lib_deps; esp32-classic/esp32-combo inherit via extends, never re-declared):
  ESP32Async/AsyncTCP @ 3.4.10        -- registry latest == EMS-ESP32 blueprint pin. Listed FIRST so ESPAsyncWebServer's dep resolution locks the maintained ESP32Async fork (not me-no-dev/AsyncTCP).
  ESP32Async/ESPAsyncWebServer @ 3.11.0  -- registry latest is 3.11.1 (a 5-day-old patch); pinned 3.11.0 to match the blueprint-validated version and minimize variables. Provides AsyncWebSocket.
  bertmelis/espMqttClient @ 1.7.2     -- registry latest. Plain non-async variant (instantiated class is espMqttClient, the sync ClientSync transport; NOT espMqttClientAsync). Blueprint vendors a patched 1.7.0 fork; we take upstream registry release.
PIN SOURCES: 'python -m platformio pkg show' (registry) + other-projects/EMS-ESP32-dev/platformio.ini (blueprint, lines 105-108 AsyncTCP 3.4.10 / ESPAsyncWebServer 3.11.0; espMqttClient vendored locally there at v1.7.0).

LINK-PROOF SMOKE: src/libraries/Platform/src/platform_esp32.h. Ungated #include <AsyncTCP.h>/<ESPAsyncWebServer.h>/<espMqttClient.h> in the include aggregator (compile-proof + LDF resolution on all three envs). Forced reference = inline __attribute__((used)) platformAsyncLinkProof() that instantiates AsyncClient + AsyncWebSocket('/_linkproof') + espMqttClient, gated on -DASYNC_LINK_PROOF (set ONLY in [env:esp32].build_flags). esp32-classic/esp32-combo rebuild build_flags from ${env.build_flags} (global [env]), so they do NOT inherit the flag and the smoke strips there — keeping the flash-tight combo under budget. ASYNC_LINK_PROOF is a feature flag (not a platform/board macro), so check_esp_abstraction_boundary stays green.

BUILD EVIDENCE (build.py masks per-env failures — grepped per-env SUCCESS line for each):
  esp32:         SUCCESS; Flash 97.8% (1923647/1966080). Smoke RETAINED + confirmed in .map: platformAsyncLinkProof() @ 0x4202bdb4; AsyncClient::AsyncClient, AsyncWebSocket (_ZN14AsyncWebSocket), espMqttClient::espMqttClient ctors present; lib17a/AsyncTCP.cpp.o + libaa3/ESPAsyncWebServer/*.o + libc0c/espMqttClient/*.o all linked. -> 'symbol per lib not dead-stripped under -flto' AC met.
  esp32-classic: SUCCESS; Flash 93.4% (1836423/1966080). Smoke correctly STRIPPED (platformAsyncLinkProof count=0 in map). All 3 libs resolved + .cpp objects compiled.
  esp32-combo:   SUCCESS; Flash 98.5% (1935727/1966080) < 100% — ~30KB headroom, within ADR-127 1.875MB app slot. Smoke STRIPPED (count=0). All 3 libs resolved + .cpp compiled.
  (All three: AsyncTCP.cpp.o, ESPAsyncWebServer/AsyncWebSocket.cpp.o, espMqttClient/espMqttClient.cpp.o compiled — 'inherited deps link on all three' AC met. Symbol-level link proof is esp32-only by design / AC-blessed fallback.)

EVALUATOR: python evaluate.py --quick -> Passed 61, Warnings 1, Failed 0. The 1 warning ('STATUS_BURST_COOLDOWN_MS bound: boards.h not found') is a pre-existing --quick path quirk, unrelated to this change. check_esp_abstraction_boundary clean (no violations printed). No NEW failures.

FILES CHANGED: platformio.ini (lib_deps + ASYNC_LINK_PROOF flag), src/libraries/Platform/src/platform_esp32.h (includes + smoke). version.h/version.hash on disk are alpha.178 from seq2's prior landed bump + build pre-script, NOT a bump introduced here. Field-flash ACs out of scope (no hardware).

PRERELEASE BUMP FIX (prior-review blocker resolved): The earlier pass left version.h at alpha.178 (consumed by TASK-865.2 commit 6bca21ad) while staging the firmware edit to src/libraries/Platform/src/platform_esp32.h. The .githooks/pre-commit bump-check (lines 24-29) would have FAILed: platform_esp32.h matches the src/libraries/ TRIGGER, but the staged version.h had no +/- _VERSION_PRERELEASE pair -> exit 1. Ran bin/bump-prerelease.sh: alpha.178 -> alpha.179, which staged version.h + data/version.hash + 42 source-file version banners (44 files total). Verified the exact hook predicate now passes: 'git diff --cached -- src/OTGW-firmware/version.h | grep _VERSION_PRERELEASE' shows both '-#define _VERSION_PRERELEASE alpha.178' and '+#define _VERSION_PRERELEASE alpha.179'. Re-ran evaluate.py --quick post-bump: 61 passed / 1 pre-existing warning / 0 failed (unchanged). Did NOT rebuild: the bump touches only version-string literals (comment banners + quoted _SEMVER_*/_VERSION strings), which is build-neutral; the prior pass's fresh-artifact build-green on all three envs still holds. NOT committed (Land phase owns commit/status). Lander: stage platformio.ini + platform_esp32.h explicitly, keep the bump's staged banner set, exclude the sibling task-865.2 .md notes edit.
<!-- SECTION:NOTES:END -->
