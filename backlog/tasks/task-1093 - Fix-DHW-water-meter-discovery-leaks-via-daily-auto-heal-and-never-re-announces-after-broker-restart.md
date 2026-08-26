---
id: TASK-1093
title: >-
  Fix: DHW water meter discovery leaks via daily auto-heal and never
  re-announces after broker restart
status: To Do
assignee: []
created_date: '2026-08-26 21:28'
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
- [ ] #1 A gateway with no MsgID 19 traffic publishes no dhw_water_total discovery config, including after markAllMQTTConfigPending() runs
- [ ] #2 After a broker restart clears the retained configs, the water meter re-announces its discovery config on the next publish without needing a reboot
- [ ] #3 Host tests cover the announce latch: not announced before first data, announced after, and re-announced after a simulated broker-state loss
- [ ] #4 Build green and evaluator shows no new failures
<!-- AC:END -->
