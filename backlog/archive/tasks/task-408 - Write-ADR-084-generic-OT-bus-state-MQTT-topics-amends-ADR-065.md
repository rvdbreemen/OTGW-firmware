---
id: TASK-408
title: 'Write ADR-084: generic OT-bus state MQTT topics (amends ADR-065)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 19:56'
updated_date: '2026-04-24 20:05'
labels:
  - adr
  - mqtt
  - ha-discovery
  - breaking-change
  - 2.0.0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Author docs/adr/ADR-084-generic-ot-bus-state-topics.md documenting the 2.0.0 decision to move three OT-bus presence values (boiler_connected, thermostat_connected, otgw_connected) out of the hardware-specific subtrees into the generic value namespace.

Background: on the feature-dev-2.0.0-otgw32-esp32-sat-support branch, OT-bus presence values are published both under otgw-pic/* (PIC path) and otgw-otdirect/* (OTDirect path), with ot_online renamed inconsistently on the OTDirect side. Source of truth is single (state.otBus.*). The two HA discovery entries (MQTTHaDiscovery.cpp:1038-1039) carry MQTT_HA_FLAG_IS_PIC_ENTRY, which disables them on OTGW32 boards (HAS_PIC=0) and points stat_t at the PIC subtree copy. This must change to a single generic topic.

This ADR formally amends ADR-065 (otgw-pic/ subtree as stable public API) so the contract is updated: "PIC subtree" now means strictly PIC-coprocessor properties (version, deviceid, firmwaretype, designer, picavailable, settings/*).

The ADR must explicitly justify the deviation from ADR-065's prescribed dual-publish migration strategy (2 minor releases of parallel publishing): major version 2.0.0 is an acceptable breaking boundary, and the self-healing retained cleanup in firmware + migration guide in docs replace the need for dual-publish.

Related ADRs to reference: ADR-065 (amends, otgw-pic-mqtt-subtree), ADR-040 (source-specific topics, unchanged), ADR-077 (streaming discovery, unchanged), ADR-063 (OTGW32 hardware support, context), ADR-060 (PIC availability guard, context).

Follow the template in docs/adr/README.md (sections: Status / Context / Decision / Alternatives Considered / Consequences / Related Decisions). Status: Accepted. Date: 2026-04-24.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New file docs/adr/ADR-084-generic-ot-bus-state-topics.md exists
- [x] #2 ADR follows the template in docs/adr/README.md (Status/Context/Decision/Alternatives Considered/Consequences/Related Decisions)
- [x] #3 Status is Accepted, date 2026-04-24
- [x] #4 Context explains the PIC/OTDirect coexistence, single source of truth in state.otBus.*, and HA-discovery breakage on OTGW32
- [x] #5 Decision names the three migrated keys and specifies the new canonical topics under OTGW/value/<uniqueId>/
- [x] #6 Decision explicitly retires otgw-otdirect/ot_online in favour of otgw_connected
- [x] #7 Alternatives Considered includes: (a) dual-publish per ADR-065 strategy, (b) rename discovery stat_t without moving publishes, (c) keep as-is — with reasons for rejection
- [x] #8 Consequences explicitly justifies the deviation from ADR-065's 2-minor-release dual-publish rule (major version boundary + self-healing cleanup + migration docs)
- [x] #9 Consequences lists: breaking for consumers on otgw-pic/* and otgw-otdirect/* for these three keys; ADR-065 amended to strictly-PIC-coprocessor scope; HA entities Boiler/Thermostat connected now discovered on OTGW32
- [x] #10 Related Decisions points to ADR-065 (amends), ADR-040, ADR-077, ADR-063, ADR-060
- [x] #11 Related section mentions TASK for firmware implementation and TASK for docs (once task IDs are assigned)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Authored docs/adr/ADR-084-generic-ot-bus-state-topics.md. ADR amends ADR-065 by narrowing the "otgw-pic/ subtree" contract to strictly PIC-coprocessor properties (version, deviceid, firmwaretype, designer, picavailable, settings/*, plus climate mode_stat_t). The three OT-bus presence values (boiler_connected, thermostat_connected, otgw_connected) are moved to the generic OTGW/value/<uniqueId>/<label> namespace. otgw-otdirect/ot_online is retired in favour of the consistent otgw_connected name.

Deviation from ADR-065's dual-publish migration strategy is explicitly justified on three grounds: (1) 2.0.0 is a major version boundary at which semantic-versioning sanctions breaking changes; (2) self-healing retained cleanup in firmware delivers the same user-facing outcome as dual-publish (no broker ghost state); (3) a rich migration guide in docs/api/MQTT.md with mosquitto_pub examples satisfies the "documented, announced, actionable" obligations of ADR-065's strategy.

Alternatives Considered covers: (a) dual-publish per ADR-065, (b) fix-discovery-only, (c) keep-as-is. All rejected with concrete reasons.

Related Decisions links ADR-065 (amended), ADR-040, ADR-077, ADR-063, ADR-060. References include TASK-410 (firmware), TASK-411 (docs), TASK-409 (future cleanup), TASK-412 (ADR-renumber).

All 11 acceptance criteria met. File is in English, no em dashes used.
<!-- SECTION:FINAL_SUMMARY:END -->
