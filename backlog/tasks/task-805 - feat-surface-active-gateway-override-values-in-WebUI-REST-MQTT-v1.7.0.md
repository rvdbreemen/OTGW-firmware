---
id: TASK-805
title: 'feat: surface active gateway-override values in WebUI/REST/MQTT (v1.7.0)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-01 18:33'
updated_date: '2026-06-01 19:47'
labels:
  - enhancement
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make user-injected gateway overrides visible again, additively (canonical behaviour unchanged). Today the boiler-side-worldview gate (is_value_valid_for_master_topic, ADR-069/075) drops the injected value from canonical, so e.g. OT=20.5 (Toutside) on a boiler that returns Data-Invalid shows 0 in the WebUI (Richard_HA, Discord #nederlandse-ondersteuning 2026-06-01). Scope = ALL gateway overrides (answer-overrides A AND thermostat-substitutions T->R). Exposure = WebUI + REST + MQTT/HA. Absorbs TASK-804. Master plan: ~/.claude/plans/mooi-ja-dit-is-ethereal-tulip.md.

Design: capture by explicit flags (rsptype==OTGW_ANSWER_THERMOSTAT && bAnswerOverride) || (rsptype==OTGW_THERMOSTAT && bGatewaySubstituted) in print_f88 (all 9 numeric override ids are ot_f88; ids 99/100 deferred). Store OTOverrideEntry_t{id,kind,value,lastSeen} x11 (~132B static, ADR-004). kind byte distinguishes A-forced-answer vs T-substituted-original. active = (millis()-lastSeen)<~10min, own timestamp (do NOT clear on next valid frame: boiler B passes the gate every poll). Hook = pure RAM write; publish from periodic path (re-entrancy). New additive REST GET /api/v2/otgw/overrides (mirror boiler-support streaming). New WebUI 'Active Overrides' table (Statistics tab) + refreshOtOverrides() mirroring refreshOtSupport(); textContent. MQTT retained <base>/<label>/override + JIT streamOverrideSensorDiscovery via publishDiscoveryFor; retarget Toutside_override stat_t /Toutside -> /Toutside/override (closes TASK-804). Files: OTGW-Core.h, OTGW-Core.ino, restAPI.ino, mqtt_configuratie.cpp, MQTTstuff.ino, data/index.html, data/index.js, version.h.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Override store added (OTOverrideEntry_t, ~132B static, ADR-004 compliant) with recordOTOverride()/isOTOverrideActive(); populated from print_f88 by explicit override flags only (not !validForMaster), with kind byte (A-forced vs T-substituted) and per-entry lastSeen + ~10min active timeout
- [ ] #2 Canonical behaviour unchanged: existing OTcurrentSystemState fields, existing REST endpoints, and existing MQTT canonical topics produce byte-identical output as before for non-override and override traffic
- [ ] #3 New additive REST endpoint GET /api/v2/otgw/overrides returns active overrides as JSON via streaming helpers; no existing route modified
- [ ] #4 New WebUI 'Active Overrides' section shows active overrides (label, value, kind, age); renders with textContent + element-existence checks; existing tabs unaffected
- [ ] #5 MQTT: retained <base>/<label>/override published from the periodic path; HA discovery streamOverrideSensorDiscovery dispatched via publishDiscoveryFor; Toutside_override number-entity stat_t retargeted to the override state topic (TASK-804 closed)
- [ ] #6 ADR-082 'Surface gateway overrides as distinct override state' authored, reviewed, and Accepted by maintainer before code merges
- [ ] #7 Version bumped to 1.7.0 (scripts/autoinc-semver.py + version.h)
- [ ] #8 python build.py exits 0 (firmware + filesystem); python evaluate.py --quick shows no new failures
- [ ] #9 Manual: inject OT=20.5, confirm new WebUI row + /api/v2/otgw/overrides JSON + <base>/Toutside/override MQTT topic appear while canonical /Toutside stays unchanged
<!-- AC:END -->
