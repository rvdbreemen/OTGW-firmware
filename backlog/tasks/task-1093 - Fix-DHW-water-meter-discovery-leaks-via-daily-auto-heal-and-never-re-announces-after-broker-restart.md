---
id: TASK-1093
title: >-
  Fix: DHW water meter discovery leaks via daily auto-heal and never
  re-announces after broker restart
status: Done
assignee:
  - '@claude'
created_date: '2026-08-26 21:28'
updated_date: '2026-08-26 21:45'
labels:
  - bug
dependencies: []
priority: high
ordinal: 192000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two defects found by adversarial review of ADR-093, both shipped in v1.7.5-beta.3.

(1) markAllMQTTConfigPending() marks every id that has a row in the PROGMEM discovery tables, and pseudo-ID 243 has one, so the daily discovery auto-heal (bDiscoveryAutoVerify, default on) publishes the retained dhw_water_total config on gateways whose bus never carries MsgID 19. Those users gain an entity that never receives state and sits at unknown. ADR-093 Must #4 requires the discovery announcement to be withheld until first data; only the state half is currently withheld.

(2) The JIT announce latch is a function-local static in publishDHWWaterMeter(). The broker-state-loss path clears the done bitmap but cannot reach that static, so after a broker restart wipes retained configs the water meter is never re-announced until the gateway reboots.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A gateway with no MsgID 19 traffic publishes no dhw_water_total discovery config, including after markAllMQTTConfigPending() runs
- [x] #2 After a broker restart clears the retained configs, the water meter re-announces its discovery config on the next publish without needing a reboot
- [x] #3 Host tests cover the announce latch: not announced before first data, announced after, and re-announced after a simulated broker-state loss
- [x] #4 Build green and evaluator shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Root cause 1: markAllMQTTConfigPending() marks every id with a row in the PROGMEM discovery tables (MQTTstuff.ino:1543-1549) and pseudo-ID 243 has one (mqtt_configuratie.cpp:1451), so the daily auto-heal published the retained config regardless of whether MsgID 19 had ever been seen. Gated with a dhwWaterMeterHasData() skip inside the scan loop, which also covers the manual callers in restAPI.ino and handleDebug.ino.
- Root cause 2: the announce latch was a function-local static inside publishDHWWaterMeter(), unreachable from the broker-restart path that calls clearMQTTConfigDone(). Moved to file scope in dhwWaterMeter.ino behind dhwWaterMeterNeedsAnnounce() / markDHWWaterMeterAnnounced() / forgetDHWWaterMeterAnnounce(), and re-armed at MQTTstuff.ino:882.
- Found by adversarial review of ADR-093, not in the field: 30 findings raised, 18 confirmed after independent verification. These two were the only code defects.
- Host tests now 18 checks, 0 failures, including three new latch cases. Build green (761776 bytes), evaluator 35/37 with 0 failures. Shipped in v1.7.5-beta.4.
- The record side is ADR-094 (Proposed).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Repairs the water total entity that v1.7.5-beta.3 shipped. Both defects broke the same promise: that the entity is announced only when there is a meter to announce.

What was wrong:
- The daily discovery auto-heal re-announced the entity on gateways with no MsgID 19 traffic. markAllMQTTConfigPending() marks every row in the discovery table and the water meter has one, so those users gained a retained config for an entity that never receives state and shows as unknown.
- After a broker restart the entity never came back. The announce latch was a function-local static, out of reach of the broker-restart path that clears the done bitmap, so the config was never re-published until the gateway rebooted.

Changes:
- MQTTstuff.ino: the table scan in markAllMQTTConfigPending() skips pseudo-ID 243 while dhwWaterMeterHasData() is false; the broker-restart path re-arms the announce latch next to clearMQTTConfigDone().
- dhwWaterMeter.ino: the latch moves to file scope behind dhwWaterMeterNeedsAnnounce(), markDHWWaterMeterAnnounced() and forgetDHWWaterMeterAnnounce().
- OTGW-firmware.h: three declarations.
- test/host/test_dhwWaterMeter.cpp: three new cases covering the latch across a simulated broker restart.

User impact: a gateway whose thermostat never requests MsgID 19 no longer grows a stateless water entity, and one that does keeps the entity across a broker restart without a reboot.

Tests: host tests 18 checks / 0 failures. Build green, sketch 765776 bytes. evaluate.py --quick 35/37 pass, 0 failures. Shipped in v1.7.5-beta.4.

Origin: found by adversarial review of ADR-093 rather than in the field. The record side of the same review is ADR-094, Proposed, which corrects sixteen further findings in the ADR text itself.
<!-- SECTION:FINAL_SUMMARY:END -->
