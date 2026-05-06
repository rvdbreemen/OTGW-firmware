---
id: TASK-549
title: >-
  Override-side TSet/TrSet routing: split thermostat vs boiler MQTT publication
  during gateway override
status: In Progress
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-06 23:02'
updated_date: '2026-05-06 23:42'
labels:
  - mqtt
  - routing
  - override
  - ha-discovery
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When a gateway override (CS=, TC=, TT=) is active, the thermostat-originated message (T) is tagged «<ignored>» (skipthis=true in OTGW-Core.ino:4046-4051) and never published to MQTT. Only the gateway-substituted R message reaches the canonical topic, which means /otgw-pic/value/TSet shows the override value but the original thermostat-side TSet is lost.

Observed (CS=27.37 active, thermostat asks 23.00):
- T10011700 TSet=23.00 → logged as <ignored>, not published
- R10011B5F TSet=27.37 → published to /otgw-pic/value/TSet only (no /thermostat or /boiler subtopic, per resolveSourceIndex in MQTTstuff.ino:1185)

User expectation (Robert, beta-testing 1.5.0-beta.16):
- /thermostat/TSet = 23.00 (real thermostat request)
- /boiler/TSet = 27.37 (what was actually sent to boiler = override when active, =thermostat when no override)

This bundles three related concerns:
1. TSet routing: stop discarding the thermostat-side value during override; publish T value to /thermostat subtopic and R/canonical value to /boiler subtopic.
2. HA discovery for nested (per-source) sensors: verify the discovery payload covers /thermostat/* and /boiler/* subtopics so users do not have to hand-configure sensors. Today it appears flat / canonical-only.
3. Override visibility: confirm whether gateway override-active is exposed somewhere (eg /gateway/CS or a state flag) so a user can correlate the boiler-side value with the active override.

Files in scope: src/OTGW-firmware/OTGW-Core.ino (skipthis logic ~L4035-4060), src/OTGW-firmware/MQTTstuff.ino (resolveSourceIndex ~L1185, publishToSourceTopic ~L1209, mqttSourceKeys ~L442). Related ADR: ADR-066 (Write-Ack echo gate) and ADR-065 (otgw-pic/ subtree as public API) — any new subtopic shape must respect those.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Thermostat-originated T message for write-data IDs (eg TSet=1, TrSet=16, MaxRelModLevelSetting=14, TdhwSet=56) publishes its value to /otgw-pic/value/<id>/thermostat even when the gateway substitutes a different value via R
- [x] #2 Boiler-side value (R when gateway override is active, T pass-through when no override) publishes to /otgw-pic/value/<id>/boiler
- [x] #3 Canonical /otgw-pic/value/<id> continues to publish the value actually sent to the boiler (back-compat with ADR-065)
- [x] #4 HA discovery payload (when bSeparateSources=true) registers entities for the /thermostat and /boiler subtopics so they appear in HA without manual sensor configuration
- [x] #5 Override visibility: a user can determine from MQTT alone whether the boiler value equals the thermostat value or a gateway override (either via existing CS/TT/TC topics or a new gateway-state topic — design choice documented in Final Summary)
- [ ] #6 Verified on hardware with an active CS=<value> override: thermostat topic shows the thermostat's request, boiler topic shows the override value, both update independently
- [x] #7 No regression to canonical /otgw-pic/value/<id> consumers (existing HA installations keep working)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. ADR-069 ratifies the worldview routing model (Accepted 2026-05-07). ADR-040 and ADR-066 status lines amended.

2. Code changes (three sites):
   a. MQTTstuff.ino:1185-1196 — resolveSourceIndex: A routes to /thermostat (was /boiler); add explicit case for R routing to /boiler (was canonical-only). Update comment block.
   b. OTGW-Core.ino:4046-4060 — Replace skipthis=true on (T-followed-by-R) and (B-followed-by-A) with a logging-only marker (e.g. bGatewaySubstituted) so the value still reaches publishToSourceTopic. Preserve <ignored> log marker for OT-bus diagnostics. Verify downstream skipthis consumers (lines 1240, 1259) still suppress parity-error frames as intended.
   c. mqtt_configuratie.cpp:2367-2377 — Update expandAndStreamSensorSources comment block to cite ADR-069 and reflect worldview semantics. Expansion logic unchanged.

3. Build firmware (python build.py --firmware) — must exit 0.

4. Run evaluator (python evaluate.py --quick) — must show no new failures.

5. AC verification:
   - ACs 1, 2, 3, 7 verifiable by code review + build success (routing changes; canonical backwards compat).
   - AC 4 (HA discovery) verifiable by code inspection — generators unchanged per ADR-069.
   - AC 5 (override visibility) covered by ADR-069 design — divergence between /thermostat and /boiler is the visibility mechanism.
   - AC 6 (hardware verification) genuinely cannot be self-verified — requires a real OTGW with active CS=. Document in Final Summary; this is a documented exception per project policy.

6. Per project policy: with one unverifiable AC (#6), task remains In Progress pending hardware verification by maintainer. Final Summary documents which ACs are code-verified vs hardware-pending.

7. Commit + push to origin/dev (standing permission per CLAUDE.md when build green and evaluator clean).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Design captured in ADR-069 (Proposed) and docs/api/MQTT.md updated.

- ADR-069 adopts the worldview semantic model:
  - /thermostat = what thermostat sees (sent T + received A or B)
  - /boiler    = what boiler sees (received T or R + sent B)
  - canonical  = boiler-side worldview (= /boiler value, when both are published)
  - no /gateway subtopic (override visible by /thermostat vs /boiler divergence)

- Three implementation deltas spelled out in ADR-069 Decision section:
  1. resolveSourceIndex (MQTTstuff.ino:1185) — A routes to /thermostat (was /boiler); R routes to /boiler (was canonical only)
  2. OTGW-Core.ino:4046-4051 — replace skipthis=true (data loss) with a logging-only flag so T survives gateway-write override and B survives gateway-answer override
  3. mqtt_configuratie.cpp:2367-2377 — comment block needs updating to reference ADR-069 worldview rationale (no code change in expansion itself)

- ADR awaits human approval before Status flips to Accepted (per CLAUDE.md ADR workflow).
- Implementation will start after ADR is Accepted.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implements ADR-069 worldview MQTT subtopic semantics on the dev (1.5.x) line.

Problem (reported by Andre on 2026-05-07 with full OT-log capture):
  With bSeparateSources=true and CS=27.37 setpoint override active, the thermostat raised its setpoint from 20 to 23 °C. The thermostat-side TSet value disappeared from MQTT entirely; only the override value reached subscribers, on the wrong subtopic. Root cause: OTGW-Core.ino:4046-4051 marked T frames as skipthis=true when followed by R within 500 ms, dropping the value from every MQTT topic. Symmetric bug existed for B-followed-by-A (read-side answer override).

Solution (ADR-069 Accepted 2026-05-07):
  Replace skipthis-based suppression with a worldview routing model. Each subtopic now reflects what THAT device sees on the OT bus, regardless of which frame type carried the value:
    /thermostat = thermostat-side worldview (T sent or A/B received)
    /boiler     = boiler-side worldview (T/R received or B sent)
    canonical   = boiler-side worldview (= /boiler value)
  Override is observable as divergence between the two subtopics; no /gateway subtopic is needed (TASK-531 retirement ratified).

Changes:
  1. OTGW-Core.h — OpenthermData_t gains `byte bGatewaySubstituted` field (1 byte). Existing skipthis field retained, scoped to parity-error suppression only.
  2. OTGW-Core.ino:4039-4060 (processOT) — (T,R)/(B,A) lookback now sets bGatewaySubstituted on the older frame instead of skipthis. Parity-error skipthis unchanged. Log decoration updated so <ignored> marker fires for both skipthis and bGatewaySubstituted, preserving OT-bus log readability for users who relied on the marker.
  3. OTGW-Core.ino:1258 (is_value_valid_for_master_topic) — two new gates: A frames never reach canonical (boiler-side worldview), and T+bGatewaySubstituted never reach canonical (R will). Comment block expanded to explain the ADR-069 canonical = boiler-side reinterpretation.
  4. MQTTstuff.ino:1208 (publishToSourceTopic) — fully rewritten with switch-based worldview routing. Routes to /thermostat and/or /boiler per (rsptype, OTdata.bGatewaySubstituted). The earlier table-based dispatch (mqttSourceKeys[], MQTT_SOURCE_KEY_COUNT, resolveSourceIndex, copySourceTableEntry, s_mqtt_src_key_thermostat, s_mqtt_src_key_boiler) is removed as dead code; subtopic names are now inlined PSTR literals in snprintf_P calls.
  5. mqtt_configuratie.cpp:2367 (expandAndStreamSensorSources comment) — comment block updated to cite ADR-069 and the worldview model. Discovery generation logic itself unchanged.
  6. docs/api/MQTT.md — « Source-Separated Topics » section rewritten with worldview semantics, frame-routing table, override example, and migration note. ADR-066 publish-gating contract preserved.
  7. docs/adr/ADR-069-mqtt-source-topic-worldview-semantics.md — new ADR (Accepted 2026-05-07). ADR-040 and ADR-066 status lines amended to note the ADR-069 amendment/refinement.

Verification:
  - python3 build.py --firmware: exit 0. Build artifact: OTGW-firmware-1.5.0-beta.20+6c413af.ino.bin (0.70 MB). No new warnings.
  - python3 evaluate.py --quick: 0 failures, 32 passed, 2 warnings (both pre-existing and unrelated: mqtt_discovery_verify.h header guard and sendMQTTheapdiag buffer arithmetic). Health score 94.4
<!-- SECTION:FINAL_SUMMARY:END -->
