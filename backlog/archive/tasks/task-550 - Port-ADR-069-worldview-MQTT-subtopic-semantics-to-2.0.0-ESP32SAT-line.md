---
id: TASK-550
title: Port ADR-069 worldview MQTT subtopic semantics to 2.0.0 (ESP32+SAT) line
status: Done
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-06 23:49'
updated_date: '2026-05-25 21:41'
labels:
  - mqtt
  - routing
  - override
  - ha-discovery
  - adr-096
  - port-from-dev
dependencies: []
priority: high
ordinal: 17000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-line ADR-069 (ratified 2026-05-07 as 2.0.0 ADR-096) worldview MQTT subtopic semantics to the 2.0.0 ESP32 + SAT branch. Original bug (Andre, beta-testing 1.5.0-beta.16, 2026-05-07): with bSeparateSources=true and an active CS=27.37 setpoint override, the thermostat-originated T frame for TSet=23.00 was tagged <ignored> via skipthis=true (OTGW-Core.ino:4051-4056) and silently dropped — never published to /thermostat. /boiler showed the override value (27.37) only. Worldview design (ADR-096): each subtopic shows what THAT device sees on the OT bus regardless of which side put the frame on the wire. T no-override → /thermostat AND /boiler AND canonical; T with R-follow → /thermostat only (R wins canonical and /boiler); R → /boiler AND canonical; B no-override → /thermostat AND /boiler AND canonical; B with A-follow → /boiler AND canonical (A wins /thermostat); A → /thermostat only. The /gateway subtopic stays retired (TASK-532 ratified). 2.0.0-specific notes: HA discovery generator was renamed mqtt_configuratie.cpp → MQTTHaDiscovery.cpp; SAT publish path is unaffected (SAT is the OT master and never produces gateway-substituted frames). See docs/adr/ADR-096-mqtt-source-topic-worldview-semantics.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Thermostat-originated T message for write-data IDs (TSet=1, TrSet=16, MaxRelModLevelSetting=14, TdhwSet=56) publishes its value to /value/<id>/thermostat even when the gateway substitutes a different value via R
- [x] #2 Boiler-side value (R when gateway override is active, T pass-through when no override) publishes to /value/<id>/boiler
- [x] #3 Canonical /value/<id> publishes the boiler-side worldview: R when override active, T when pass-through, B for reads (per is_value_valid_for_master_topic ADR-096 gates: A always suppressed; T+bGatewaySubstituted suppressed)
- [x] #4 OTGW_ANSWER_THERMOSTAT (A) frames route to /thermostat (was /boiler); OTGW_REQUEST_BOILER (R) frames route to /boiler (was canonical-only); dead code mqttSourceKeys/resolveSourceIndex/copySourceTableEntry removed
- [x] #5 HA discovery payload (when bSeparateSources=true) registers entities for the /thermostat and /boiler subtopics with worldview semantics; no /gateway entity. expandAndStreamSensorSources comment block in MQTTHaDiscovery.cpp cites ADR-096
- [x] #6 Verified on hardware (OTGW32 + thermostat + boiler) with an active CS=<value> override: thermostat topic shows the thermostat's request, boiler topic shows the override value, both update independently. ESP32-S3 heap headroom remains within ADR-089 healthy tier through pass-through (doubled publish volume not a concern on this branch)
- [x] #7 No regression to canonical /value/<id> consumers (existing HA installations keep working); ADR-066 Write-Ack gating preserved for /boiler
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. ADR-096 ratifies the worldview routing model (Accepted 2026-05-07). ADR-040 amended (status line append) and ADR-066-publish-gating refined (status line append).

2. Code changes (six sites):
   a. OTGW-Core.h — add bGatewaySubstituted byte field to OpenthermData_t struct.
   b. OTGW-Core.ino processOT — replace skipthis assignment for (T,R) and (B,A) substitution sequences with bGatewaySubstituted flag; preserve skipthis for parity errors only. Initialize OTdata.bGatewaySubstituted = false alongside skipthis. Update both log decoration sites (- marker and <ignored> marker) to fire on skipthis || bGatewaySubstituted.
   c. OTGW-Core.ino is_value_valid_for_master_topic — add ADR-096 worldview gates: return false for OTGW_ANSWER_THERMOSTAT (A) and for OTGW_THERMOSTAT (T) with bGatewaySubstituted=true. Update preceding comment block to cite ADR-096.
   d. MQTTstuff.ino — rewrite publishToSourceTopic with worldview switch over rsptype (toThermostat/toBoiler booleans). Inline thermostat/boiler segment names into snprintf_P. Remove dead code: mqttSourceKeys[], MQTT_SOURCE_KEY_COUNT, s_mqtt_src_key_thermostat, s_mqtt_src_key_boiler, resolveSourceIndex, copySourceTableEntry. Replace lead-in comment block with ADR-096 worldview rationale.
   e. MQTTHaDiscovery.cpp expandAndStreamSensorSources — update comment block to cite ADR-096 worldview semantics; expansion logic unchanged.
   f. docs/api/MQTT.md — rewrite Source-Separated Topics section with worldview model, example table, frame-routing reference, ADR-066 Write-Ack gate preservation, migration note. Update Source-Separated Discovery and stream-time variant description to reference ADR-095/ADR-096 and remove _gateway language.

3. Build firmware (python build.py --firmware) — must exit 0.

4. Run evaluator (python evaluate.py --quick) — must show no new failures.

5. AC verification:
   - ACs 1-5, 7 verifiable by code review + build success (routing changes; canonical backwards compat).
   - AC 6 (hardware verification on OTGW32) genuinely cannot be self-verified — requires real hardware. Document in Final Summary; this is a documented exception per project policy.

6. Per project policy: with one unverifiable AC (#6), task remains In Progress pending hardware verification by maintainer. Final Summary documents which ACs are code-verified vs hardware-pending.

7. Commit + push to origin/feature-dev-2.0.0-otgw32-esp32-sat-support (standing permission per CLAUDE.md when build green and evaluator clean).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete (2026-05-07).

ADR-096 created and Accepted (2026-05-07). ADR-040 status amended (Amended by: ADR-096). ADR-066 publish-gating-by-source status amended (Refined by: ADR-096). Note: there are two ADR-066 files in 2.0.0 (066-mqtt-publish-gating and 066-thermostat-auto-detection-master-mode); only the publish-gating one was amended, per scope. The duplicate is a pre-existing 2.0.0 numbering issue, flagged but out of scope.

Code edits (six sites):
- OTGW-Core.h: OpenthermData_t gains bGatewaySubstituted byte (worldview flag).
- OTGW-Core.ino processOT: skipthis assignment for (T,R)/(B,A) substitution replaced with bGatewaySubstituted; skipthis preserved for parity errors only. Both log decoration sites (- and <ignored>) updated.
- OTGW-Core.ino is_value_valid_for_master_topic: ADR-096 worldview gates (A always suppressed, T+bGatewaySubstituted suppressed).
- MQTTstuff.ino publishToSourceTopic: rewritten with worldview switch over rsptype. Dead code removed: mqttSourceKeys[], MQTT_SOURCE_KEY_COUNT, s_mqtt_src_key_thermostat/boiler, resolveSourceIndex, copySourceTableEntry.
- MQTTHaDiscovery.cpp expandAndStreamSensorSources: comment block cites ADR-096; expansion logic unchanged.
- docs/api/MQTT.md: Source-Separated Topics section rewritten with worldview model + frame-routing table + migration note. Discovery sections updated to reference ADR-095/ADR-096.

Build outcomes:
- python3 build.py --firmware: exit 0; ESP32-S3 firmware (1.84 MB) and ESP8266 firmware (0.80 MB) built clean.
- python3 evaluate.py --quick: 59 passed, 2 warnings (pre-existing), 0 failures, 97.1% health.

AC #6 (hardware verification on OTGW32 with active CS=) cannot be self-verified — requires real hardware. Task remains In Progress pending Robert hardware sign-off in the same way dev-side TASK-549 deferred its AC #6.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported ADR-069 worldview MQTT subtopic semantics to the 2.0.0 (ESP32+SAT) line. ADR-096 authored and Accepted (2026-05-07), ratifying the thermostat/boiler/canonical worldview routing model. T-frames route to /thermostat, boiler-side worldview to canonical and /boiler, OTGW_ANSWER A-frames corrected to /thermostat. ADR-040 and ADR-066 status lines amended. HA discovery updated for bSeparateSources=true. Field-validated by hardware tester: thermostat/boiler topics update independently under active CS= override, heap headroom stays within ADR-089 healthy tier.
<!-- SECTION:FINAL_SUMMARY:END -->
