---
id: TASK-886
title: >-
  fix(web): full ADR-141 revert — remove ArduinoJson, all-streaming JsonEmit,
  hand-rolled inbound parse
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-18 20:02'
updated_date: '2026-06-18 22:20'
labels: []
dependencies: []
ordinal: 102000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer decision 2026-06-18 (TASK-885 A/B): full ADR-141 revert. Streaming JsonEmit wins the heap axis decisively (maxblock ~31KB vs ArduinoJson <12KB cratered; 0 ADR-089 tiers; ~8KB smaller binary; settings byte-identical). Remove ArduinoJson entirely: convert all ~40 restSendJson(JsonDocument) output sites to restBeginStream+JsonEmit+restFinalize, rewrite the ONE inbound site (extractJsonField) as a bounded flat top-level scanner, delete the ADR-145 chunked plumbing, drop the lib dep. Builds on sub-branch feature-2.0.0-esp32s3-async-jsonemit (7372a1e5 already has JsonEmit + 6 heavy handlers). HONESTY: JsonEmit single-pass buffers the whole response in the AsyncResponseStream cbuf (no doc pool) — NOT the zero-whole-buffer property the deleted ADR-145 chunked path had; the heap win is killing doc-pool fragmentation. Bounds-check OK: largest response settings 7.3KB, gate=4 -> 29KB peak << 90KB floor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 extractJsonField rewritten as a bounded flat top-level scanner that skips values wholesale (no key-in-value false match); host-compiled C test passes for synthetic cases (key-in-value, escapes, bool/number/null, missing, whitespace) AND real captured bodies (webhook/simulate/settings POST); all ~18 callers confirmed flat-scalar; lenient-parse acceptable given downstream atof/atoi/range validation
- [ ] #2 All remaining restSendJson(JsonDocument) output sites converted to restBeginStream+JsonEmit+restFinalize across restAPI/FSexplorer/OTDirect/SATble/SATcontrol/SATweather; tiny error-object responses routed via sendApiError not JsonEmit boilerplate
- [ ] #3 ArduinoJson fully removed: restSendJson(JsonDocument&)+RestJsonStream+JsonChunkWindow+measureJson deleted from webServerCompat.h; #include <ArduinoJson.h> removed (OTGW-firmware.h, webServerCompat.h); lib dep removed from platformio.ini; grep confirms zero ArduinoJson symbols outside src/libraries
- [ ] #4 Superseding ADR (Proposed) reverts ADR-141 and retires ADR-145; states the honest cbuf-vs-doc-pool tradeoff (whole-response cbuf, no doc pool; NOT zero-buffer)
- [ ] #5 build esp32 green + evaluate.py --quick green; json_golden semantic-equal (settings byte-identical) + explicit bool/float byte-checks pass
- [ ] #6 [field] 8-16w load test GATE-ON (cap 4): no OOM-class reboot, heap pristine (maxblock floor, 0 ADR-089 tiers); any reboot decoded via _serialcap+addr2line and attributed — TASK-879/watchdog/core-1 is approach-independent, NOT a revert regression
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Progress 2026-06-19: (1) Inbound parser DONE: extractJsonField rewritten in jsonStuff.ino as a bounded non-throwing flat top-level scanner (xjfReadString/xjfSkipValue/extractJsonFieldImpl); skips values wholesale -> no key-in-value false match; \uXXXX->UTF-8; strlcpy-truncation (behaviour-preserving). Host-validated: scripts/tests/test_extract_json_field.py 32/32 pass (no host C compiler available -> Python 1:1 mirror of the C control flow). (2) ADR-146 DRAFTED (Proposed): docs/adr/ADR-146-revert-adr141-streaming-jsonemit-rest-esp32s3.md supersedes ADR-141 + retires ADR-145; honest cbuf-not-zero-buffer + F1/F5-not-resurrected + reboot-axis caveats. (3) evaluate.py::check_no_arduinojson FLIPPED from ADR-141 settings-path-only to the FULL ADR-146 ban (any ArduinoJson symbol outside src/libraries). Per maintainer standing directive 2026-06-19: never use ArduinoJson. (4) OpenAPI compliance validator built: scripts/tests/openapi_compliance.py (jsonschema 2020-12 vs docs/api/openapi.yaml) per maintainer request. (5) Outbound conversion IN PROGRESS via 5-agent fanout+adversarial-review workflow (restAPI/FSexplorer/OTDirect/SATble/SATweather). NOTE: field device 192.168.88.39 went offline mid-session (100% ping loss); golden+compliance run post-flash (flash is over COM4 USB, WiFi not required). Streaming base parked on sub-branch feature-2.0.0-esp32s3-async-jsonemit (commit 7372a1e5).
<!-- SECTION:NOTES:END -->
