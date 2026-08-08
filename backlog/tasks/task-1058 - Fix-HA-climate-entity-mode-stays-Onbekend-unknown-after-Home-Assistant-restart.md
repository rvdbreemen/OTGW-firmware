---
id: TASK-1058
title: >-
  Fix: HA climate entity mode stays Onbekend/unknown after Home Assistant
  restart
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-07 20:14'
updated_date: '2026-08-08 09:47'
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
- [x] #1 Root cause confirmed with evidence: identify whether hvac_mode is published non-retained, never published since boot, or both
- [x] #2 After an HA restart (homeassistant/status -> online) the climate entity shows a real mode within one publish cycle, without an ESP reboot
- [x] #3 Fix does not reintroduce the ADR-073 bulk discovery republish storm; if the ADR-073 no-op decision must change, a superseding ADR is authored and Accepted first
- [x] #4 hvac_action and other change-gated state topics are covered by the same fix or explicitly documented as out of scope
- [x] #5 python build.py --firmware exits 0
- [x] #6 python evaluate.py --quick shows no new failures
- [x] #7 Field validation: nico55 confirms entities survive an HA Core update on the fix build
- [x] #8 hvac_mode/hvac_action latch their RAM cache only on a confirmed send, so a dropped publish retries instead of stranding the topic until the mode changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-07 investigation (pre-ADR):
- requestMQTTRepublishAll() (OTGW-Core.ino:1359) already covers every on-change gate: resetMqttTrackedState() clears mqttlastsent[128] + status/VH bit+byte + ASF/RBP/RO slots; requestMQTTStatusRepublish() sets the 4 mqttForceNext*StatusPublish flags. hvac_mode/hvac_action are covered TRANSITIVELY: publishMasterStatusState/publishSlaveStatusState pass forcePublish into publishHvacMode/publishHvacAction (OTGW-Core.ino:1725, 1770). No new force mechanism needed - one call site.
- Storm is bounded by design: resetMqttTrackedState only clears timers. Republish is demand-driven, paced by OT bus traffic as each MsgID next arrives (~1 msg/s), not a synchronous flood. Same path already runs in production on the offline>5min reconnect branch.
- Retained-birth risk CHECKED against nico55 capture: homeassistant/status appears only at mqtt.log lines 1990 (offline) and 2376 (online), both live during the 21:18:55/21:20:22 HA restart, and NOT in the retained flush (which runs to line 352+ of homeassistant/*/config). So HA birth is not retained here (HA default retain=false). BUT retain is user-configurable, and with bHaRebootDetect=false the handler sets bHAcycle=true on every online, so a retained birth would fire a republish on every firmware MQTT reconnect. A guard is warranted.

2026-08-07 adversarial review (4 lenses, 7 findings raised, 6 refuted, 1 confirmed):
CONFIRMED and fixed: publishHvacMode/publishHvacAction (OTGW-Core.ino:1665/1678) discarded sendMQTTData bool and latched mqttLastHvacMode/Action unconditionally. resetMqttTrackedState() does not cover those two caches, and the force flag is cleared at :1707/:1751 BEFORE the fan-out, so a single dropped send stranded hvac_mode until the mode genuinely changed - defeating ADR-088 Confirmation step 2 on the exact topic the ADR was written about. Pre-existing (the reconnect branch at MQTTstuff.ino:873 had the same exposure), not introduced by this change.
Fix: latch on confirmed send, else fall back to the -1 unset sentinel so the next OT frame retries. Matches the ADR-076 commit-on-success pattern already used by publishStatusBitMQTT at :1557. No new force flag or mechanism, so ADR-088 Must Not is respected.
Note: reviewer attributed the drop to HEAP_LOW throttling; verified the exposure is wider - sendMQTTData has four false-return paths including an ordinary endPublish TCP failure, and the ADR-088 capture gateway was HEAP_HEALTHY at 18-19KB.

2026-08-08 field validation CONFIRMED. nico55 in Discord #nederlandse-ondersteuning, 07:38 UTC: "Bovenstaande zoals omschreven uitgevoerd en helemaal goed nu. Klasse" (did exactly as described, all good now). That closes AC#7.
AC#2 was verified on bench hardware beforehand (otgw1.local, 1.7.3-beta.2+290dddb): a real offline->online transition logged "Home Assistant went online!" at 08:03:16.881 and hvac_mode published 419ms later at 08:03:17.300, with no reboot. hvac_mode appeared exactly once in the whole 10-minute capture, and that once was the republish.
AC#8 implemented as commit-on-success in publishHvacMode/publishHvacAction; logic verified by inspection, and indirectly on device since every observed publish latched correctly. Forcing a sendMQTTData failure on demand is not practical on the bench.
Shipped in v1.7.3-beta.2; v1.7.3-beta.3 adds the TASK-1060 heartbeat on top.
<!-- SECTION:NOTES:END -->
