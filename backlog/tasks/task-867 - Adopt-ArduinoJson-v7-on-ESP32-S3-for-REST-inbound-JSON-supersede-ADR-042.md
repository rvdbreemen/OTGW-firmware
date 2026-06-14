---
id: TASK-867
title: Adopt ArduinoJson v7 on ESP32-S3 for REST + inbound JSON (supersede ADR-042)
status: To Do
assignee: []
created_date: '2026-06-14 22:58'
updated_date: '2026-06-14 23:00'
labels: []
dependencies: []
ordinal: 83000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PLAN ONLY - do not execute yet (maintainer directive 2026-06-15). Revisit the ADR-042 no-ArduinoJson ban now the 2.0.0 line is ESP32-S3-only (ADR-128). ADR-042's rationale was 100% ESP8266-driven: DynamicJsonDocument heap-frag on ~40KB RAM (ADR-004, now superseded by ADR-053 static-buffer), and a v6 fixed-capacity 1536B overflow that silently wiped settings. ESP32-S3 has 327KB RAM (build shows 35.5% used); ArduinoJson v7 is elastic (no fixed capacity => the settings-wipe bug class is structurally gone). Both ESP32 OpenTherm references use it: EMS-ESP32-dev = bblanchon/ArduinoJson@7.4.3 + AsyncJsonResponse; OT-Thing-OTGW32 = ArduinoJson. NEW hard constraint ADR-042 never faced: flash budget - esp32-combo is at 94.6% (1984135/2097152, ~113KB free). Enforced ban lives in evaluate.py check_no_arduinojson (line 2248).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GO/NO-GO flash delta measured on esp32-combo (binding target) via a one-endpoint spike BEFORE any wholesale migration
- [ ] #2 Superseding ADR Accepted: supersedes ADR-042; amends ADR-053 static-buffer scope to permit ArduinoJson v7 on ESP32-S3 for REST responses + inbound parsing; settings-persistence stays manual
- [ ] #3 evaluate.py check_no_arduinojson updated (allow REST/parse use; keep ArduinoJson OUT of the settings-persistence path if that boundary is retained)
- [ ] #4 REST v2 endpoints serve byte-identical JSON contracts (docs/api) via AsyncJsonResponse; restBeginStream/sendJsonMapEntry layer + REST_STREAM_BUFFER_SIZE removed once migrated
- [ ] #5 Inbound JSON parsing handles empty-string, escaping and nested cases (the ADR-042 known-bug list) via deserializeJson
- [ ] #6 settingStuff.ino settings persistence unchanged (manual wStrF/applySettingFromFile) - explicitly out of scope
- [ ] #7 All targets (esp32, esp32-classic, esp32-combo) build + flash-fit; evaluate.py green
- [ ] #8 Heap validated under load on ESP32-S3 hardware (ADR-089 tiers, no fragmentation regression)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN ONLY — not for execution yet. Phased so each phase is independently shippable and the risky parts stay last.

## Phase 0 — Decision gate (no production code)
- Spike: add bblanchon/ArduinoJson @ ^7.x to a throwaway branch, convert ONE endpoint (e.g. /api/v2/device/info) to AsyncJsonResponse, build all three targets. Record the Flash% delta per target. esp32-combo (94.6%, ~113KB free) is the binding GO/NO-GO. If combo overflows, options: slim elsewhere first, or scope ArduinoJson to esp32/esp32-classic only (combo stays manual) — decide here.
- Pin version: match EMS (7.4.3) or latest 7.x. Add -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 if String interop is needed.

## Phase 1 — Superseding ADR (Accept before any merge)
- New ADR supersedes ADR-042, amends ADR-053 (static-buffer; ADR-004 already superseded by it). Decision: ArduinoJson v7 permitted on ESP32-S3 for (a) REST responses and (b) inbound JSON parsing; settings-persistence stays on the manual path. Record the flash measurement from Phase 0 as evidence. Note ADR-088 (MQTT burst) and ADR-089 (heap tiers) interactions.

## Phase 2 — Relax the CI gate
- evaluate.py check_no_arduinojson (line 2248): change from hard ban to a scoped rule — allow ArduinoJson in restAPI/jsonStuff; keep it banned in settingStuff.ino (settings persistence) and the MQTT discovery hot path if those boundaries are retained. Keep a test proving the boundary holds.

## Phase 3 — REST responses -> AsyncJsonResponse (incremental, endpoint by endpoint)
- Replace the restBeginStream/sendJsonMapEntry imperative-push layer with AsyncJsonResponse (EMS pattern). Migrate one v2 resource at a time (device, health, sat, otgw, otdirect, network, discovery, flash, firmware, filesystem), verifying each response is byte-identical to the documented contract (docs/api). 
- When the last endpoint is migrated: delete restBeginStream/restFinalize, the sendJsonMap*/sendNestedJson* helpers, and REST_STREAM_BUFFER_SIZE (this whole layer becomes dead). This is what makes the provisional 4096 chunk buffer moot.

## Phase 4 — Inbound parsing -> deserializeJson
- Replace extractJsonField / parseJsonKVLine / readJsonStringPair (HTTP bodies + support files) with deserializeJson. Fixes the ADR-042 known bugs (empty-string rejection, manual-escape gaps, flat-only). Keep bounded input size guards (max body len) to cap allocation.

## Phase 5 — Verify + field-validate
- Build all three targets, flash-fit, evaluate.py green. Soak REST under parallel load on ESP32-S3 hardware; watch ADR-089 heap tiers + maxFreeBlock for any fragmentation regression vs the manual baseline.

## Explicitly OUT of scope (this migration)
- settings-persistence (settingStuff.ino wStrF/applySettingFromFile) — low value, high risk (the path that broke under v6). Keep manual. A separate later decision if ever.
- MQTT HA discovery (MQTTstuff snprintf_P streaming) — works, ADR-088 windowing; keep for now.

## Andere zaken (related, documented here so they are not lost)
- The just-added REST_STREAM_BUFFER_SIZE = 4096 (webServerCompat.h) is PROVISIONAL: it optimizes the manual AsyncResponseStream path, which Phase 3 removes. If this migration proceeds, that constant is deleted in Phase 3; if it does NOT proceed, 4096 stays as the standing optimization. Either way it is committed now as part of the ETag/ADR-139 work and is not blocked by this task.
- ADR-139 (ETag serving, AsyncTCP core-pin) is independent and already implemented; not blocked by and does not block this migration.
- Reference precedent for the serving model: EMS-ESP32 ESP32React.cpp (AsyncJsonResponse + setLength) and OT-Thing-OTGW32 portal.cpp (beginResponseStream). We cannot copy EMS verbatim (ArduinoJson types throughout) but the AsyncJsonResponse serving shape is the target.
<!-- SECTION:PLAN:END -->
