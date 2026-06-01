---
id: TASK-806
title: 'feat-2.0.0: surface active gateway-override values (port of TASK-805)'
status: To Do
assignee: []
created_date: '2026-06-01 18:34'
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
- [ ] #1 Override store (OTOverrideEntry_t ~132B static, ADR-004) + recordOTOverride/isOTOverrideActive; captured in print_f88 by explicit flags (bAnswerOverride / bGatewaySubstituted), kind byte, per-entry lastSeen + ~10min timeout
- [ ] #2 Canonical behaviour unchanged (existing state/REST/MQTT canonical output byte-identical)
- [ ] #3 New additive REST GET /api/v2/otgw/overrides via streaming helpers; no existing route modified; ESP32 guards respected
- [ ] #4 New WebUI 'Active Overrides' view reusing the OT-Direct panel rendering pattern, fed by the new override store (not OT-Direct data); textContent + element checks
- [ ] #5 MQTT retained <base>/<label>/override + HA discovery (streamOverrideSensorDiscovery in MQTTHaDiscovery.cpp via publishDiscoveryFor); Toutside_override number-entity stat_t retargeted to override state topic
- [ ] #6 ADR-118 authored, reviewed, Accepted by maintainer before code merges
- [ ] #7 2.0.0 prerelease bump applied per branch mechanism (alpha.NN), bump-check hook green; all referenced TASK-NNN staged per commit-msg hook
- [ ] #8 python build.py per 2.0.0 targets: grep per-env SUCCESS lines (build.py masks per-env fails); python evaluate.py --quick no new failures
- [ ] #9 Manual: inject override, confirm new WebUI view + REST endpoint + MQTT override topic appear while canonical stays unchanged
<!-- AC:END -->
