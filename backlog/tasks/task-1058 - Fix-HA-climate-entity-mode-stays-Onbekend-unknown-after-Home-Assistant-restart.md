---
id: TASK-1058
title: >-
  Fix: HA climate entity mode stays Onbekend/unknown after Home Assistant
  restart
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-07 20:14'
updated_date: '2026-08-07 20:33'
labels:
  - bug
  - mqtt
dependencies: []
references:
  - >-
    Discord #nederlandse-ondersteuning nico55 2026-08-06/07 msg
    1535369849235054738; capture
    transcript-20260807-211337-1.7.2+728426c-OTGW-otgw-E8DB84DC4538.txt
priority: high
ordinal: 170000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by nico55 (Discord #nederlandse-ondersteuning, 2026-08-06/07) on 1.7.2+728426c. After every HA Core update HA restarts; the OTGW climate card then shows mode 'Onbekend' while current temp (Tr) and target (TrSet) render fine. An HA-side gateway reset does not recover it; only an ESP reboot does. Evidence from his own capture (transcript-20260807-211337): the capture contains a real HA restart at 21:20:22 ('Home Assistant went online!'), the availability topic is retained 'online', and all discovery configs are retained -- but topic central_heating/value/<id>/hvac_mode was published 0 times in 14 minutes and had no retained value in the broker's flush at subscribe. mode_stat_t of the climate config points at exactly that topic. publishHvacMode() (OTGW-Core.ino:1654) only publishes on change or on force, and the last-published value lives in RAM (mqttLastHvacMode), so a device that has been up for days with a stable mode never re-emits it. Per ADR-073 the homeassistant/status->online handler is deliberately a no-op (MQTTstuff.ino:663-667), so nothing re-sends state on HA restart. Same reasoning applies to hvac_action and any other change-gated, non-retained state topic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause confirmed with evidence: identify whether hvac_mode is published non-retained, never published since boot, or both
- [ ] #2 After an HA restart (homeassistant/status -> online) the climate entity shows a real mode within one publish cycle, without an ESP reboot
- [ ] #3 Fix does not reintroduce the ADR-073 bulk discovery republish storm; if the ADR-073 no-op decision must change, a superseding ADR is authored and Accepted first
- [ ] #4 hvac_action and other change-gated state topics are covered by the same fix or explicitly documented as out of scope
- [ ] #5 python build.py --firmware exits 0
- [ ] #6 python evaluate.py --quick shows no new failures
- [ ] #7 Field validation: nico55 confirms entities survive an HA Core update on the fix build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-07 investigation (pre-ADR):
- requestMQTTRepublishAll() (OTGW-Core.ino:1359) already covers every on-change gate: resetMqttTrackedState() clears mqttlastsent[128] + status/VH bit+byte + ASF/RBP/RO slots; requestMQTTStatusRepublish() sets the 4 mqttForceNext*StatusPublish flags. hvac_mode/hvac_action are covered TRANSITIVELY: publishMasterStatusState/publishSlaveStatusState pass forcePublish into publishHvacMode/publishHvacAction (OTGW-Core.ino:1725, 1770). No new force mechanism needed - one call site.
- Storm is bounded by design: resetMqttTrackedState only clears timers. Republish is demand-driven, paced by OT bus traffic as each MsgID next arrives (~1 msg/s), not a synchronous flood. Same path already runs in production on the offline>5min reconnect branch.
- Retained-birth risk CHECKED against nico55 capture: homeassistant/status appears only at mqtt.log lines 1990 (offline) and 2376 (online), both live during the 21:18:55/21:20:22 HA restart, and NOT in the retained flush (which runs to line 352+ of homeassistant/*/config). So HA birth is not retained here (HA default retain=false). BUT retain is user-configurable, and with bHaRebootDetect=false the handler sets bHAcycle=true on every online, so a retained birth would fire a republish on every firmware MQTT reconnect. A guard is warranted.
<!-- SECTION:NOTES:END -->
