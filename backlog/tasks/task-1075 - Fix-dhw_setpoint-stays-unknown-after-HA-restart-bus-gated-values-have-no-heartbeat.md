---
id: TASK-1075
title: >-
  Fix: dhw_setpoint stays unknown after HA restart (bus-gated values have no
  heartbeat)
status: To Do
assignee:
  - '@claude'
created_date: '2026-08-16 19:49'
updated_date: '2026-08-21 19:47'
labels:
  - bug
  - wontfix-pending
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Option B, scalar-only replay of stored values on HA-online. ADR first (supersedes ADR-088), no code before maintainer approval.

1. ADR (Proposed): replay stored scalar values on HA-online instead of only re-arming the publish gates. Must address pacing, because ADR-088 Consequences explicitly rest on "the burst is paced by OpenTherm bus arrival rather than emitted synchronously".

2. New function republishStoredScalarValues(), called from the homeassistant/status offline->online handler alongside the existing requestMQTTRepublishAll().
   - Set: the 16 restLastUpdated slots MINUS REST_UPDATED_STATUSFLAGS and REST_UPDATED_ASFFLAGS = 14 scalar MsgIDs. The two status words fan out to bit topics and already have their own force path via requestMQTTStatusRepublish() (OTGW-Core.ino:1365-1369).
   - Seen-gate: getMsgLastUpdated(id) != 0. Sound: TRACKED_TIME_UNSEEN is 0xFFFF and currentTrackedSeconds() ranges [0,65534], so no collision (OTGW-Core.ino:341-342).
   - Value: getOTGWValue(id), which formats from OTcurrentSystemState and needs no OTdata. Copy into a local immediately: it returns a shared static char[32] and the publish path can yield.
   - Topic: messageIDToString(id) into a local char[OT_TOPIC_LEN], same as print_f88 does at OTGW-Core.ino:2004.
   - Publish: sendMQTTData(topic, msg). Master topic only; publishToSourceTopic deliberately not called in this change.
   - Pacing: static cursor, one MsgID per doBackgroundTasks tick behind the existing heap gate. No synchronous loop.
   - mqttlastsent[] deliberately not updated; at worst the next real frame republishes once as first-seen.

3. Do NOT reuse decodeAndPublishStatusAndConfigValue(). It routes through print_f88, which reads the global OTdata frame rather than its argument (OTGW-Core.ino:1977), so it is frame-bound and would publish the wrong value off-frame.

4. Bench reproduction on the Docker Mosquitto rig, no boiler needed: send SW=60 to populate TdhwSet, publish homeassistant/status offline then online, assert OTGW/value/<id>/TdhwSet appears. This is the AC #2 pass/fail.

5. python build.py --firmware exit 0 + python evaluate.py --quick no new failures.

6. AC #4 needs stefan_24213 to confirm on his own system, so this task stays In Progress after the code is green.

Open for maintainer: whether the 2.0.0 worktree gets a sibling task (MQTT/HA contract change, CLAUDE.md says almost always yes).
<!-- SECTION:PLAN:END -->

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

2026-08-21: analysed transcript-20260821-193910 (1.7.4+b77304b, 11 min, telnet + MQTT + REST snapshot). This is the capture the task was blocked on: it contains a real HA restart AND the TASK-1079 REST snapshot.

The open question is now answered. The firmware DOES hold the value.
- REST snapshot /api/v2/otgw/messages/56 returns TdhwSet 60.000 C, and otmonitor returns dhwsetpoint 60.000. restAPI.ino:1166 gates that otmonitor entry behind getMsgLastUpdated(OT_TdhwSet), so the entry is only emitted for a message the firmware has actually seen. The 60 C is a learned value, not a default.
- HA restarted during the capture: homeassistant/status offline followed by online in the MQTT stream. The ADR-088 republish path was therefore exercised.
- OTGW/value/<id>/TdhwSet was still never published, not once, before or after the HA-online transition. The only occurrences of TdhwSet in the MQTT stream are the discovery config for the sensor and the dhw_control climate config whose temp_stat_t points at that topic.
- MsgID 56 is again absent from the bus. Frame histogram over the telnet section: ids 0,1,2,3,5,9,14,16,17,24,25,26,57,116,120,123 present, 56 absent. Same as the 2026-08-17 capture, so this is his steady state and not a sampling artefact.

Consequence for the two options on file: the branch "if the snapshot shows messages/56 value 0.000, neither option helps" is ruled out. The gateway knows 60 C, keeps knowing it, and still cannot deliver it to HA across a restart. Option A (retained state topics) and Option B (republish the stored value on HA-online) both remain viable, and the hard constraint about never publishing an unseen value is not violated by either, since this value is seen.

AC #1 is met by this capture: root cause confirmed against reporter data rather than code reading. Not flipping it here because the fix decision and ADR still need the maintainer.

2026-08-21: maintainer decided not to fix this. Status set back to To Do and unassigned; the implementation plan stays on the task as the record of what the fix would have been, should this come back.

What is settled and does not need redoing if it does come back: root cause confirmed against two of the reporter捕 captures, the gateway holds TdhwSet 60 C across an HA restart, MsgID 56 is absent from his bus, and getOTGWValue + restLastUpdated are the frame-independent primitives a fix would use. decodeAndPublishStatusAndConfigValue is NOT usable (frame-bound via global OTdata).

Not done: no ADR written, no code, no 2.0.0 sibling task. Reporter has a working workaround (set the DHW card to 60 manually, which sends SW=60).
<!-- SECTION:NOTES:END -->
