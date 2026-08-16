---
id: TASK-1075
title: >-
  Fix: dhw_setpoint stays unknown after HA restart (bus-gated values have no
  heartbeat)
status: To Do
assignee: []
created_date: '2026-08-16 19:49'
updated_date: '2026-08-16 19:54'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning'
  - stefan_24213
  - '2026-08-14'
  - msg 1537891894216884255
priority: medium
ordinal: 179000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by stefan_24213 (Shadowfall) in Discord #nederlandse-ondersteuning on 2026-08-14. After a Home Assistant restart the DHW control card falls back to 21 C and sensor.opentherm_gateway_otgw_otgw_dhw_setpoint stays 'onbekend', while the OTGW web UI shows the correct 60 C. Manually changing the card to 60 makes the value appear again.

Suspected mechanism (code read, not yet verified on device): MQTTstuff.ino:652-673 handles homeassistant/status offline->online by calling requestMQTTRepublishAll(), which only resets the publish gates. A value is re-published when it next arrives on the OpenTherm bus. MsgID 56 (TdhwSet) is an RW remote parameter that many thermostats rarely or never request spontaneously, so the entity stays unknown until someone writes it. v1.7.4 therefore does not fix this case. hvac_mode/hvac_action got a 5-minute heartbeat in 1.7.3-beta.3; purely bus-gated values have no such fallback.

Needs from reporter: firmware version in use, and a telnet capture showing whether MsgID 56 appears on his bus at all.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause confirmed against a capture from the reporter (or a bench reproduction) rather than code reading alone
- [ ] #2 After an HA Core restart, dhw_setpoint shows the gateway's known value without waiting for a bus event or a manual write
- [ ] #3 Fix does not introduce a publish flood: republish stays paced, consistent with ADR-088
- [ ] #4 Reporter confirms the fix on his own system
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-16: Robert asked stefan_24213 for a capture in Discord #nederlandse-ondersteuning. Blocked on that reply: no investigation or fix work until the log arrives, since the root cause is code-reading only so far and he may simply be on pre-1.7.4.
<!-- SECTION:NOTES:END -->
