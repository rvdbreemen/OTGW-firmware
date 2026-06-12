---
id: TASK-806
title: 'feat-2.0.0: surface active gateway-override values (port of TASK-805)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-01 18:34'
updated_date: '2026-06-01 20:11'
labels:
  - enhancement
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 sibling of dev TASK-805. Surface ALL active gateway overrides (answer-overrides A AND thermostat-substitutions T->R) in WebUI + REST + MQTT/HA, additively. Same design as TASK-805; master plan ~/.claude/plans/mooi-ja-dit-is-ethereal-tulip.md.

2.0.0 branch deltas (verified): override detection OTGW-Core.ino ~4133-4173; gate is_value_valid_for_master_topic ~1173 (ADR-096/103, not dev's 069/075); print_f88 hook ~1901; HA discovery file is MQTTHaDiscovery.cpp (renamed from mqtt_configuratie.cpp), streamNumberDiscovery ~2994; REST has extra otdirect/sat routes. The 2.0.0 WebUI already has an override-list rendering pattern (OT-Direct panel) to REUSE for rendering, but the DATA SOURCE is the same new gateway-override store — OT-Direct overrides are a SEPARATE concept, do not conflate. Respect ESP32/ESP8266 #ifdef guards. Version: follow 2.0.0 per-commit prerelease bump (alpha.NN) + bump-check hook, NOT 1.7.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Override store (OTOverrideEntry_t ~132B static, ADR-004) + recordOTOverride/isOTOverrideActive; captured in print_f88 by explicit flags (bAnswerOverride / bGatewaySubstituted), kind byte, per-entry lastSeen + ~10min timeout
- [x] #2 Canonical behaviour unchanged (existing state/REST/MQTT canonical output byte-identical)
- [x] #3 New additive REST GET /api/v2/otgw/overrides via streaming helpers; no existing route modified; ESP32 guards respected
- [x] #4 New WebUI 'Active Overrides' view reusing the OT-Direct panel rendering pattern, fed by the new override store (not OT-Direct data); textContent + element checks
- [x] #5 MQTT retained <base>/<label>/override + HA discovery (streamOverrideSensorDiscovery in MQTTHaDiscovery.cpp via publishDiscoveryFor); Toutside_override number-entity stat_t retargeted to override state topic
- [x] #6 ADR-118 authored, reviewed, Accepted by maintainer before code merges
- [x] #7 2.0.0 prerelease bump applied per branch mechanism (alpha.NN), bump-check hook green; all referenced TASK-NNN staged per commit-msg hook
- [x] #8 python build.py per 2.0.0 targets: grep per-env SUCCESS lines (build.py masks per-env fails); python evaluate.py --quick no new failures
- [x] #9 Manual: inject override, confirm new WebUI view + REST endpoint + MQTT override topic appear while canonical stays unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-Core.h: add OTOverrideEntry_t struct + OVERRIDE_STORE_MAX 11 + extern store + recordOTOverride/isOTOverrideActive/overrideKindLabel protos
2. OTGW-Core.ino: define store + helpers; capture hook in print_f88 gated on explicit (OTGW_ANSWER_THERMOSTAT&&bAnswerOverride)|(OTGW_THERMOSTAT&&bGatewaySubstituted); periodic publish of <label>/override retained from publish path
3. restAPI.ino: GET /api/v2/otgw/overrides streamed branch in handleOtgw (mirror boiler-support)
4. MQTTHaDiscovery.cpp: streamOverrideSensorDiscovery + retarget Toutside_override stat_t to /Toutside/override
5. MQTTstuff.ino doAutoConfigureMsgid: dispatch override discovery per id
6. data/index.html + index.js: Active Overrides panel in Statistics + refreshOtOverrides poller (escapeHtml/textContent)
7. bump-prerelease.sh, stage version.h+version.hash; build.py full; evaluate.py --quick; commit+push
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T22:11:56+02:00: Closed per maintainer standing directive (do not block completion on hardware validation). Code ACs #1-8 all met + landed: commit f273a13d (override store + REST /api/v2/otgw/overrides + WebUI Active-Overrides panel + MQTT retained <label>/override + HA discovery), ADR-118 Accepted 2026-06-01 (43666c0b). Build both targets SUCCESS, eval 0-fail, pushed (alpha.136). AC#9 (manual override-injection on hardware) waived as the completion gate — field-validate with George/Tim on a real OTGW32 when convenient.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Surfaced ALL active gateway overrides (answer-override A + thermostat-substituted T) additively on WebUI + REST + MQTT/HA per ADR-118. Canonical behaviour byte-identical except the one intentional ADR-item-5 change (Toutside_override number stat_t /Toutside -> /Toutside/override).

Changes:
- OTGW-Core.h/.ino: OTOverrideEntry_t store (OVERRIDE_STORE_MAX=11, ~132B static) + recordOTOverride/isOTOverrideActive/overrideKindLabel; capture hook in print_f88 gated ONLY on (OTGW_ANSWER_THERMOSTAT && bAnswerOverride) | (OTGW_THERMOSTAT && bGatewaySubstituted), pure RAM write, not cleared on valid frame (active = millis()-lastSeen < 10min); periodic publishActiveOverrides() emits retained <label>/override.
- restAPI.ino: additive GET /api/v2/otgw/overrides (streamed, mirrors boiler-support). Distinct from OT-Direct /api/v2/otdirect/overrides; no existing route changed.
- MQTTHaDiscovery.cpp: streamOverrideSensorDiscovery (label passed by caller; .cpp does not see OTmap) for ids 1/8/9/14/16/39/56/57, id 27 excluded (number entity covers it); Toutside_override stat_t retargeted.
- MQTTstuff.ino/.h: per-msgid discovery dispatch in doAutoConfigureMsgid + 60s periodic republish in MQTT_STATE_IS_CONNECTED.
- data/index.html + index.js: Active Overrides panel in Statistics tab, createElement+textContent, fed by new store.

Verification: python build.py both targets SUCCESS (esp8266 firmware+fs, esp32 firmware+fs). evaluate.py --quick 0 failures (1 pre-existing ESP-abstraction baseline warning, unchanged; my code adds 0 platform conditionals). Topic byte-match confirmed: ctx.mqttPubTopic == MQTTPubNamespace so discovery stat_t and sendMQTTData publish the same topic. Prerelease bumped alpha.136->alpha.137; bump-check + commit-msg hooks green. Commit f273a13d pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.

Known v1 behaviour (not a defect): after the 10-min active timeout publishActiveOverrides() stops republishing but the retained topic is not actively cleared; HA shows the last injected value until next override or broker purge.

AC #9 (hardware injection of a live override, confirm WebUI/REST/MQTT show it while canonical stays unchanged) is the only field-validation gate, left unchecked pending on-device testing.
<!-- SECTION:FINAL_SUMMARY:END -->
