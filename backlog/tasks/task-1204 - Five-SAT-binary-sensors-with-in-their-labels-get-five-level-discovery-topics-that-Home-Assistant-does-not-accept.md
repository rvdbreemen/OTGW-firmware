---
id: TASK-1204
title: >-
  Five SAT binary sensors with '/' in their labels get five-level discovery
  topics that Home Assistant does not accept
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 17:54'
updated_date: '2026-10-04 08:14'
labels:
  - ha-discovery
  - bug
  - sat
dependencies: []
priority: medium
ordinal: 326000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier (code reading, not yet observed in Home Assistant; SAT is off on the bench). buildBinSensorDiscoveryTopic() (MQTTHaDiscovery.cpp:2659-2676) does not call sanitizeHaObjectId(), while buildSensorDiscoveryTopic() does (:2636). The binary rows sat/weather/is_day, sat/active, sat/safety_tripped, sat/flame_health and sat/valves_open (near :1448-1453) therefore produce topics such as homeassistant/binary_sensor/<node>/sat_sat/active/config, one level deeper than Home Assistant's discovery pattern. TASK-872 sanitized only the binary uniq_id (:2510).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced on the laptop rig with SAT enabled: the five configs land on topics with an extra level, and Home Assistant creates no entity for them
- [x] #2 Binary-sensor topics are sanitized like sensor topics; the uniq_id and state topic of each entity stay unchanged
- [x] #3 On the rig, Home Assistant shows the five SAT binary sensors after a republish
- [x] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 reproduced 2026-10-04 on the laptop rig (bench on alpha.408, SAT disabled; these five configs are published regardless of SAT state). The broker holds retained homeassistant/binary_sensor/otgw-1020BA21B4F8/sat_sat/{weather/is_day,active,safety_tripped,flame_health,valves_open}/config, one topic level deeper than HA's TOPIC_MATCHER, with uniq_ids otgw-1020BA21B4F8-sat_sat_* (already sanitized by TASK-872). None of the five uniq_ids is in HA's core.entity_registry. Host harness: the new check 'every discovery topic matches Home Assistant's pattern' fails on 8795bacc0 and passes on the fix. The old-vs-fix diff (topics normalised by sanitizeHaObjectId's rule) shows every payload byte-identical, so uniq_id and state topic are unchanged (AC#2).

AC#3 on the rig 2026-10-04: alpha.409+c2eaa66 flashed, then a discovery republish drained (disc_pending_ids 0). The five configs are now retained on homeassistant/binary_sensor/otgw-1020BA21B4F8/sat_sat_{weather_is_day,active,safety_tripped,flame_health,valves_open}/config. HA created binary_sensor.opentherm_gateway_otgw_sat_{weather_is_day,active,safety_tripped,flame_health,valves_open} at 08:12:50-52Z with the unchanged uniq_ids. Residue: the old five-level topics stay retained on brokers that ran older firmware. HA ignores them (they never matched its pattern), so they are harmless and are not cleared. AC#4: esp32, esp32-classic and esp32-combo SUCCESS (alpha.409); evaluate.py --quick 0 failures.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
buildBinSensorDiscoveryTopic() now runs sanitizeHaObjectId() on the label, as buildSensorDiscoveryTopic() already did. The five SAT binary sensors whose labels hold a '/' (sat/active, sat/weather/is_day, ...) got a config topic one level deeper than HA's discovery pattern, so HA never created them. uniq_id, name and state topic are unchanged. Evidence: rig before, five retained configs on sat_sat/... topics and none of the uniq_ids in HA's registry; rig after, all five entities created right after a republish on alpha.409. The host harness now fails any discovery topic outside HA's TOPIC_MATCHER (fails on 8795bacc0, passes on the fix), and the old-vs-fix diff shows every payload byte-identical. Builds for 3 envs green, evaluate clean.
<!-- SECTION:FINAL_SUMMARY:END -->
