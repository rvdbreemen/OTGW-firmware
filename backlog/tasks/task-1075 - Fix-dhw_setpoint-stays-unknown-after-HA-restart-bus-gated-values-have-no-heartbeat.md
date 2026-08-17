---
id: TASK-1075
title: >-
  Fix: dhw_setpoint stays unknown after HA restart (bus-gated values have no
  heartbeat)
status: To Do
assignee: []
created_date: '2026-08-16 19:49'
updated_date: '2026-08-17 20:58'
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

2026-08-17: stefan_24213 confirms he runs v1.7.4, so the ADR-088 HA-online republish is already in his build and this is a separate defect, not the fix he was missing. He also reports the capture script crashes after he fills in all the prompts, repeated several times, so no log yet. Robert suggested plain telnet to otgw.local:23 as a fallback.

2026-08-17: analysed a full capture from stefan_24213 (transcript-20260817-081204, 1.7.4+b77304b, 994 KB, ~26 min, telnet + MQTT both present).

Confirmed chain:
- The DHW control card takes its target temperature from temp_stat_t = OTGW/value/<id>/TdhwSet. Discovery for it is published twice in the capture, so HA does build the entity.
- OTGW/value/<id>/TdhwSet is never published, not once, in the whole capture.
- OpenTherm data-id 0x38 (56, TdhwSet) does not appear on his bus at all. Frame histogram over the telnet section: ids 0,1,2,3,5,9,14,16,17,24,25,26,57,116,120,123 are present; 56 is absent. Neighbouring id 57 (MaxTSet) occurs 28 times, so the parse is sound.
- Tdhw (id 26, current DHW temperature) publishes normally at 27.16-27.20, which is why the card shows a current temperature but no target.

So the entity is not stale, it was never fed. His thermostat/boiler pair simply does not exchange MsgID 56 in normal operation. Manually setting the card to 60 sends SW=60, which makes the value known and publishable, which is exactly the workaround he found.

This sharpens the original hypothesis. ADR-088 republish-on-HA-online resets the publish gates so each value re-publishes "as first-seen", paced by OT bus arrival. For a value that never arrives on the bus, that mechanism can never deliver anything, no matter how often HA restarts. A republish that waits for the bus cannot repair a value the bus does not carry.

Open and not answerable from this capture: whether the firmware holds a last-known TdhwSet in RAM. The device uptime is 468179 s (5.4 days) and the web UI shows 60, which suggests it does, but the capture contains no HA restart (no homeassistant/status transition in 26 minutes), so the republish path was never exercised here. If the firmware does hold the value, the fix is to publish the stored value on HA-online instead of only re-arming the gate.

2026-08-17 design analysis, no code written. Decision deferred by the maintainer until stefan_24213 delivers a capture containing a real HA restart plus the new REST snapshot (TASK-1079).

Code finding that reframes the bug: requestMQTTRepublishAll() calls resetMqttTrackedState(), which sets every mqttlastsent[] slot to TRACKED_TIME_UNSEEN. That array packs the last published value in bits 31-16 and the last publish time in bits 15-0, so on an HA restart the firmware erases its own record of what it knew and then waits for the bus to re-teach it. For values the bus carries constantly this is invisible; for MsgID 56 on this reporter's system it is fatal. The problem is not that the value is unknowable, it is that it is discarded.

Second fact constraining any fix: the entire publish path is frame-driven. shouldPublishMQTTForID() is reached from OT frame processing, and the 60s heartbeat only ticks when a frame arrives. "Just force the heartbeat" therefore cannot deliver a value the bus does not carry.

Two real options.
Option A, publish state topics retained. Smallest change, makes requestMQTTRepublishAll redundant for state, and also covers restarts the firmware never observes (HA reinstalled or moved, broker reconnect). Costs: retained topics persist, so a renamed topic leaves an orphan that must be cleared with an empty retained payload. Stale values after the gateway goes offline are already handled by avty_t per ADR-074.
Option B, keep the stored value and republish it on HA-online, dripped, for every slot whose timestamp is not UNSEEN. Stays inside the current semantics and touches no broker behaviour, but needs the packed u16 routed back through the correct formatter and needs pacing to avoid the ADR-073 republish storm.

Recommendation on file: Option A, because it removes machinery instead of refining it and fixes the whole class rather than this one topic. Either way it supersedes ADR-088, so it needs an ADR before implementation.

Hard constraint for both: never publish a value that was never seen. Emitting 0 for MsgID 56 would render 0 degrees on the card, which is fabricated data and worse than unknown. The existing TRACKED_TIME_UNSEEN marker is the correct gate.

If the snapshot shows messages/56 value 0.000, neither option helps this reporter: his gateway never learned the setpoint over OpenTherm, the web UI value comes from elsewhere, and the question becomes whether the gateway should request MsgID 56 itself or whether the DHW card should stop promising a target it cannot source.
<!-- SECTION:NOTES:END -->
