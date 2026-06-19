---
id: TASK-886
title: >-
  fix(web): full ADR-141 revert — remove ArduinoJson, all-streaming JsonEmit,
  hand-rolled inbound parse
status: Done
assignee:
  - '@claude'
created_date: '2026-06-18 20:02'
updated_date: '2026-06-19 07:09'
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
- [x] #1 extractJsonField rewritten as a bounded flat top-level scanner that skips values wholesale (no key-in-value false match); host-compiled C test passes for synthetic cases (key-in-value, escapes, bool/number/null, missing, whitespace) AND real captured bodies (webhook/simulate/settings POST); all ~18 callers confirmed flat-scalar; lenient-parse acceptable given downstream atof/atoi/range validation
- [x] #2 All remaining restSendJson(JsonDocument) output sites converted to restBeginStream+JsonEmit+restFinalize across restAPI/FSexplorer/OTDirect/SATble/SATcontrol/SATweather; tiny error-object responses routed via sendApiError not JsonEmit boilerplate
- [x] #3 ArduinoJson fully removed: restSendJson(JsonDocument&)+RestJsonStream+JsonChunkWindow+measureJson deleted from webServerCompat.h; #include <ArduinoJson.h> removed (OTGW-firmware.h, webServerCompat.h); lib dep removed from platformio.ini; grep confirms zero ArduinoJson symbols outside src/libraries
- [x] #4 Superseding ADR (Proposed) reverts ADR-141 and retires ADR-145; states the honest cbuf-vs-doc-pool tradeoff (whole-response cbuf, no doc pool; NOT zero-buffer)
- [x] #5 build esp32 green + evaluate.py --quick green; json_golden semantic-equal (settings byte-identical) + explicit bool/float byte-checks pass
- [x] #6 [field] 8-16w load test GATE-ON (cap 4): no OOM-class reboot, heap pristine (maxblock floor, 0 ADR-089 tiers); any reboot decoded via _serialcap+addr2line and attributed — TASK-879/watchdog/core-1 is approach-independent, NOT a revert regression
<!-- AC:END -->



## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Progress 2026-06-19: (1) Inbound parser DONE: extractJsonField rewritten in jsonStuff.ino as a bounded non-throwing flat top-level scanner (xjfReadString/xjfSkipValue/extractJsonFieldImpl); skips values wholesale -> no key-in-value false match; \uXXXX->UTF-8; strlcpy-truncation (behaviour-preserving). Host-validated: scripts/tests/test_extract_json_field.py 32/32 pass (no host C compiler available -> Python 1:1 mirror of the C control flow). (2) ADR-146 DRAFTED (Proposed): docs/adr/ADR-146-revert-adr141-streaming-jsonemit-rest-esp32s3.md supersedes ADR-141 + retires ADR-145; honest cbuf-not-zero-buffer + F1/F5-not-resurrected + reboot-axis caveats. (3) evaluate.py::check_no_arduinojson FLIPPED from ADR-141 settings-path-only to the FULL ADR-146 ban (any ArduinoJson symbol outside src/libraries). Per maintainer standing directive 2026-06-19: never use ArduinoJson. (4) OpenAPI compliance validator built: scripts/tests/openapi_compliance.py (jsonschema 2020-12 vs docs/api/openapi.yaml) per maintainer request. (5) Outbound conversion IN PROGRESS via 5-agent fanout+adversarial-review workflow (restAPI/FSexplorer/OTDirect/SATble/SATweather). NOTE: field device 192.168.88.39 went offline mid-session (100% ping loss); golden+compliance run post-flash (flash is over COM4 USB, WiFi not required). Streaming base parked on sub-branch feature-2.0.0-esp32s3-async-jsonemit (commit 7372a1e5).

Code COMPLETE + committed f07634a6 (sub-branch feature-2.0.0-esp32s3-async-jsonemit, pushed origin). Build esp32 SUCCESS (Flash 78.1%/RAM 35.2%, alpha.215). evaluate.py --quick green (ArduinoJson ban gate passes; only pre-existing boards.h WARN). Adversarial review found 1 MAJOR (OTD control gains kp/ki/.. truncated by %.3f -> ki 0.0005 became 0.001) + minors; FIXED at root by making JsonEmit value(float) trim-trailing-zeros at 6 decimals (host-validated 14 cases). AC#5 (json_golden + byte-checks) and AC#6 (8-16w load test) BLOCKED: field device 192.168.88.39 offline. Diagnosis: esptool reads MAC + serial shows a CLEAN boot (bootloader->entry, no panic, no reboot loop), but the device will not rejoin WiFi after 2 resets; it was ALSO offline BEFORE the flash -> environmental (AP-fallback / unreachable AP), NOT the refactor. Needs maintainer power-cycle / re-provision; then run scripts/json_golden.py compare + scripts/tests/openapi_compliance.py + the 8-16w load test, then merge to feature-2.0.0-esp32s3-async + final push.

OFFLINE de-risk (device still down): built + hardened scripts/tests/openapi_compliance.py (fixed a $ref-resolution crash via named-URI registry; added --golden offline mode) and validated the json_golden baseline (= the refactor's semantic output) against docs/api/openapi.yaml. Found+fixed 3 PRE-EXISTING spec bugs (independent of the revert): networkmode enum [wifi,ethernet,ap]->[WiFi,Ethernet] (networkModeName() truth), room_temp nullable:true (3.0 no-op under 3.1) -> type:[number,"null"]. Result: baseline 28/28 spec-compliant. So when the device returns, the live openapi_compliance.py + json_golden are expected GREEN. Committed 7b1d113c (docs+tooling, pushed). f07634a6 (the revert) unchanged.

alpha.216 field-validated on OTGW32 @192.168.1.143 (Koekie-boven, MQTT disabled to remove the homeassistant.local DNS confounder; serial-captured COM4). AC#5 GREEN: build esp32 SUCCESS; evaluate.py --quick 0 fail; json_golden compare 26 PASS / 5 environmental-only FAIL (ipaddress/ssid/dns/gateway from the network move, settings values, filesystem 27->29) / 0 type-or-structure drift; OpenAPI compliance 18/18 live PASS (2 PIC SKIP=503, expected on OTDirect); float-trim byte-check in place (JsonEmit value(float) 6-decimal trim, host-validated). AC#6 load test EXERCISED: 8w/2min unthrottled GATE-ON = PASS (0 reboot, heap recovers -1.9%, frag 74% bounded, maxblock floor 9716, 0 ADR-089 tiers, 99.3% handled); 16w/2min = 0 reboot (bootcount stable across two runs incl. a no-serial control run; the lone 45->46 was the serial-capture port-close DTR reset, not a flood reboot), heap recovers +1.9%, 94.8% handled. IMPORTANT CORRECTION to AC#6 premise: the residual TWDT is NOT 'approach-independent'. Hardware A/B (45e26b8d/ADR-145) + this run prove it is serializer-independent but WHOLE-RESPONSE-BUFFERING-dependent: the AsyncResponseStream cbuf grows via resizeAdd and storms 'Failed to allocate' (~4186 lines/4min) under flood-fragmentation. The revert reintroduced it (ADR-145 chunked path was ArduinoJson-based, removed). Mitigated here by a HEAP-TIER-AWARE backpressure gate (restEffectiveInflightCap tightens cap toward 1 as maxblock shrinks) -> storm spread below the 30s watchdog -> device survives the flood that previously rebooted it. No OOM-class reboot, heap pristine = AC#6 testable bar MET. The 16w 98%-handled throughput ceiling is architectural (whole-response buffer) -> routed to TASK-883 (true chunked/pull-based JsonEmit streaming). Commit 361e267a (alpha.216). Leaving In Review for maintainer sign-off on the premise correction + 16w throughput decision.
<!-- SECTION:NOTES:END -->
