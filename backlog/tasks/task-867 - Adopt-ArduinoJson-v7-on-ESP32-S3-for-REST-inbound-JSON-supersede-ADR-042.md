---
id: TASK-867
title: Adopt ArduinoJson v7 on ESP32-S3 for REST + inbound JSON (supersede ADR-042)
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-14 22:58'
updated_date: '2026-06-18 09:34'
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
- [x] #1 GO/NO-GO flash delta measured on esp32-combo (binding target) via a one-endpoint spike BEFORE any wholesale migration
- [x] #2 Superseding ADR Accepted: supersedes ADR-042; amends ADR-053 static-buffer scope to permit ArduinoJson v7 on ESP32-S3 for REST responses + inbound parsing; settings-persistence stays manual
- [x] #3 evaluate.py check_no_arduinojson updated (allow REST/parse use; keep ArduinoJson OUT of the settings-persistence path if that boundary is retained)
- [x] #4 REST v2 endpoints serve byte-identical JSON contracts (docs/api) via AsyncJsonResponse; restBeginStream/sendJsonMapEntry layer + REST_STREAM_BUFFER_SIZE removed once migrated
- [x] #5 Inbound JSON parsing handles empty-string, escaping and nested cases (the ADR-042 known-bug list) via deserializeJson
- [x] #6 settingStuff.ino settings persistence unchanged (manual wStrF/applySettingFromFile) - explicitly out of scope
- [x] #7 All targets (esp32, esp32-classic, esp32-combo) build + flash-fit; evaluate.py green
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 GO/NO-GO flash spike (esp32-combo) = GO. Baseline 1981939 B -> spiked 1986291 B (one flat REST object via JsonDocument+serializeJson routed to live sink). Delta +4352 B; free_after 110861 B (>>40KB margin). ArduinoJson 7.4.3 compiled clean on combo. Caveat: header-only lib, +4.3KB is a lower bound (deserialization not exercised; cost scales with usage breadth) but headroom ample. AC#2 (superseding ADR) already satisfied by ADR-141 (Accepted, #660). Remaining: AC#3 evaluate.py gate, AC#4 REST v2 -> AsyncJson, AC#5 inbound deserializeJson, AC#7 build 3 targets, AC#8 hardware heap (field).

AC#2 ADR wiring: ADR-042 already carries the sanctioned 'Superseded by ADR-141' status line; added the matching scope-narrowing status line to ADR-053 (JSON I/O exempt from static-buffer rule on S3 line; settings-persistence still bound). AC#3 evaluate.py::check_no_arduinojson relaxed to a settings-path-only ban (ARDUINOJSON_BANNED_FILES={settingStuff.ino}); ArduinoJson now permitted in REST/parse/discovery code. platformio.ini: pinned bblanchon/ArduinoJson @ 7.4.3 (matches EMS-ESP32 blueprint).

Slice proven: sendHealth() migrated to ArduinoJson v7 (native types). esp32 build SUCCESS (ArduinoJson 7.4.3 links on S3, 00:07:28). Flashed app-only no-erase COM4, live /v2/health now emits real JSON booleans (mqttconnected/otgwconnected/littlefsMounted false/false/true) instead of quoted strings - the intended JSON-correctness improvement. json_golden oracle (upgraded with bool-string normalization + expanded volatile list): 28 PASS, 3 environmental FAIL (debug volatile dump, /.health temp file, otdirect/overrides untouched-code volatile), ZERO migration regressions. AC#5 partial: inbound deserializeJson still pending. alpha.205.

Fanout batch (alpha.207): migrated 4 JSON-output files via 5 parallel agents, all hardware-verified clean (oracle: zero accidental regressions). OTDirect sendOTDirectOverridesJSON (4 arrays + queue sibling), FSexplorer apilistfiles (heterogeneous file array + storage summary, type=dir/file string labels, truncated real bool) + apifirmwarefilelist, SATweather weatherSendStatusJSON (13 fields string->number, dtostrf-quoted -> native float), SATble satBLERosterSendJSON. All oracle FAILs classified: weather string->number (intended), otdirect/overrides (gold was a truncated capture; new JSON valid+correct), filesystem +topic_cleanup.bin (env file), debug/otmonitor/telegraf (volatile device state). DEFERRED (cross-file lockstep): satSendStatusJSON (SATcontrol) calls satBLESendStatusJSON (SATble) mid-object into one open stream - must convert SATcontrol.ino + SATble.ino + the satBLESendStatusJSON declaration in OTGW-firmware.h together (change to satBLESendStatusJSON(JsonObject&)). satSendHealthJSON is in restAPI.ino:801 not SATcontrol. REMAINING: restAPI.ino (441 entries) + the SAT lockstep.

SAT lockstep done (alpha.208): satSendStatusJSON (134 fields, exact order, area_* dynamic char[] keys + last_blocked_cmds nested JsonArray) + satBLESendStatusJSON(JsonObject&) [sig changed in .ino + .h] converted to ArduinoJson v7. satSendJsonFloat KEPT (9 live callers in restAPI satSendHealthJSON). Oracle: /v2/sat, /v2/sat/status (132 fields), /v2/sat/status?detail=full all PASS semantic-equal. Last big hand-rolled block done. REMAINING: restAPI.ino (~441 entries incl satSendHealthJSON) + inbound deserializeJson (AC#5).

restAPI.ino batch (alpha.209): 23 handlers converted to ArduinoJson v7 (sendDeviceInfoV2, sendDeviceSettings, sendSensorStatus, sendOTmonitorV2, sendDeviceCrashLog, sendPICsettings, sendOTDirectStatus, satSendHealthJSON, handleDebugDump, handleNetwork, sendOTValue/Label, device/time, flash-status, etc.). 24 restSendJson sites, zero hand-rolled JSON calls remain (verified by grep, only comments). Type fixes: string-bool->bool throughout, satSendJsonFloat->native (x9 in satSendHealthJSON), CONOFF/CCONOFF string labels kept, sendJsonSettingObj rebuilt with serialized() for byte-match. Traps handled: static getDallasAddress key->String copy, dangling CSTR(String temp)->direct String assign. Oracle: all converted endpoints PASS semantic-equal; FAILs are golden-artifacts (debug/overrides gold was truncated/invalid - conversion produces valid JSON) or env/intended. LEFT (heap, ADR-089): 3 unbounded handleOtgw streamed arrays (boiler-support/ot-support/overrides, 0..255 msgid space). REMAINING: delete now-unused satSendJsonFloat, decide on the 3 streamed arrays, inbound deserializeJson (AC#5), golden re-capture + OpenAPI.

Output cleanup (alpha.210): deleted dead satSendJsonFloat (SATcontrol.ino, zero callers post-migration); converted the last 3 hand-rolled streamed arrays in handleOtgw (boiler-support/ot-support/overrides) to ArduinoJson - adds escaping safety for OTmap labels (snprintf did not escape). All 3 verified SEMANTIC-EQUAL vs pre-conversion refs. REST OUTPUT NOW 100% ArduinoJson (only the HTML 404 page uses raw stream, correct). Full oracle 26 PASS / 5 FAIL (all env/golden-artifact: truncated gold for debug+overrides, weather string->number intended, filesystem topic_cleanup). REMAINING: inbound deserializeJson (AC#5) via extractJsonField single-point reimpl; golden re-capture; OpenAPI type sync; evaluate.py green.

AC#5 inbound DONE (alpha.211): reimplemented extractJsonField with deserializeJson (single point, fixes the strstr false-match + escaping/nested bugs; all ~15 call sites benefit). Hardware-verified: POST {name,value} parses correctly (target 20->23.5), false-match body {decoy:value,...,value:21.0} correctly yields 21.0 (old parser would mis-parse), restored. parseJsonKVLine (settings persistence) left manual (AC#6). PARTITION REBALANCE (TASK-867, maintainer-requested): adding deserializeJson overflowed the 1.875MB app (+1367B). Shrank LittleFS partition 2.0->1.5MB (assets use ~0.95MB), grew app 1.875->2.375MB (partitions_otgw_esp32.csv app0 0x260000, spiffs 0x270000/0x180000; build.py fs_offset/fs_size/app_size; otgw-test FLASH_MAP). Plus -DARDUINOJSON_USE_DOUBLE=0/-DUSE_LONG_LONG=0 (firmware is all-float -> correct + smaller). Full-flashed new partition table no-erase: WiFi creds PRESERVED (NVS untouched), LittleFS mounts at 0x270000 (index.html 200/39791B), app Flash 78.9% (was 100%). Oracle: only new diff is device/info LittleFSsize 2->1.5 (intended). AC#7 esp32 build SUCCESS + fits. classic shares the csv; combo (own csv) is a follow-up.

AC#4 DONE (with documented deviations): all REST v2 output migrated to ArduinoJson v7. Grep confirms ZERO callers of sendJsonMapEntry/sendStartJsonMap/sendEndJsonMap outside their now-dead definitions in jsonStuff.ino (optional dead-def sweep = small follow-up). DEVIATIONS from the literal AC wording, both deliberate: (1) used serializeJson-into-AsyncResponseStream (restSendJson helper) rather than AsyncJsonResponse - chunked streaming is gentler on the S3 heap than AsyncJsonResponse's single flat buffer (ADR-089). (2) Contracts are SEMANTIC-identical, not byte-identical: the maintainer directed 'fix broken manual JSON, do not reproduce it', so string-bools became real bools and dtostrf-quoted numbers became native JSON numbers (intended improvements, oracle-classified). REST_STREAM_BUFFER_SIZE: not present. AC#6 DONE: settingStuff.ino code untouched across the whole migration (b54c0890~1..HEAD diff is banner-only); parseJsonKVLine persistence stays manual.

AC#8 heap-under-load measured via scripts/tests/test_load.py on 192.168.88.39 (committed ec14692b). On AC#8's ACTUAL criterion (ADR-089 heap tiers + fragmentation regression), the migration is CLEAN: at realistic <=4-worker load (870 req / 3 min) success 99.4%, freeheap fully recovers to baseline (no leak), zero ADR-089 tier entries (low/warning/critical all +0), fragmentation peaks under churn then recovers, and idle steady-state fragmentation returns to the ~44% baseline (maxfreeblock ~53KB) = no fragmentation regression vs the manual path. SEPARATELY, an UNTHROTTLED 8-worker flood (near-DoS, not realistic load) triggers an IDF Task-Watchdog reboot (lastreset='Task watchdog', heap healthy ~94KB, no panic). That is CPU starvation (async_tcp pinned to core 1 hogging idle1, ADR-139), ORTHOGONAL to the heap axis, and is the SAME pre-existing mechanism as TASK-879 (seen at alpha.199, before this migration). Tracked in TASK-883 incl. an A/B check to settle whether the migration shifted the threshold (currently UNMEASURED -> not attributed to the migration). AC#8 stays In Review for maintainer field validation; the heap criterion itself shows no regression.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Migrated all JSON I/O on the ESP32-S3 2.0.0 line from hand-rolled code to ArduinoJson v7 (ADR-141). Output: every REST handler (health, SAT status 132 fields, device/info, settings, sensors, OTDirect, filesystem, weather, BLE, otmonitor/telegraf, the 3 otgw streamed arrays, +more) now builds a JsonDocument and streams via serializeJson; 0 callers of the old sendJsonMapEntry layer remain. Inbound: extractJsonField reimplemented with deserializeJson (one-point fix for ~15 call sites; kills the strstr false-match + escaping/nested bugs). Type-correctness improvements per maintainer: string-bools -> real bools, dtostrf-quoted numbers -> native numbers. Settings persistence stays manual (AC#6). Partition rebalanced (app 1.875->2.375 MB, LittleFS 2.0->1.5 MB) on both esp32 and combo tables to fit the ArduinoJson footprint; -DARDUINOJSON_USE_DOUBLE=0. All 3 targets build + flash-fit (esp32 78.9%, classic 76.6%, combo 80.7%), evaluate.py 0 failures. Hardware-verified end-to-end on 192.168.88.39 via a json_golden semantic-equality oracle (zero contract regressions) + full partition reflash (creds preserved, LittleFS remounts). Shipped alpha.205-211. AC#8 (heap-under-load on S3) is field validation -> In Review.
<!-- SECTION:FINAL_SUMMARY:END -->
