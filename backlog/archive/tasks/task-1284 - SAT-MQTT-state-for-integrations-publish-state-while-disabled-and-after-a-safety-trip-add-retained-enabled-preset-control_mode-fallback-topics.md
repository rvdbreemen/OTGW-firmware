---
id: TASK-1284
title: >-
  SAT MQTT state for integrations: publish state while disabled and after a
  safety trip, add retained enabled/preset/control_mode/fallback topics
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 18:11
updated_date: '2026-10-21T21:39:00Z'
labels:
  - sat
  - mqtt
  - ha
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 413000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feedback gruijter 2026-10-21 (#alpha-testing, Homey app author), checked against dev d23ef153. Commands are fine; state is not reliable after a Homey or gateway reboot. 1) No state while SAT is disabled: satPublishMQTT() returns when !bEnabled (SATcontrol.ino ~2489) and satDisable() publishes nothing, so retained sat/active=true and sat/mode=continuous stay on the broker after sat/enabled false. 2) A safety trip is never published: both trip paths (~4515, ~4837) end in satDisable() without a publish, later loops return early (~4419); sat/safety_tripped is non-retained. Missing retained state topics: 3) sat/enabled (on/off matching the command; sat/active is not a substitute), 4) sat/preset and sat/control_mode (commands exist, no state to read back; sat/mode is the actual mode), 5) sat/fallback_active (REST only). 6) SATexternaltemp ('use MQTT/REST as indoor temp source') has no MQTT set or state topic. 7) HA discovery: the PV-boost number/switch entities use sat/boost_* while the firmware uses sat/pv_boost_* (MQTTstuff.ino ~2552-2554), so they are dead. Payload formats are fine. Mind ADR-111 (SAT publish on-change + heartbeat), ADR-101 flat topics, ADR-052 publish eligibility.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After sat/enabled false the retained SAT state reflects disabled (sat/enabled false, sat/active false), checked with a broker capture
- [x] #2 A safety trip publishes sat/safety_tripped (retained) and the disabled state
- [x] #3 Retained state topics sat/enabled, sat/preset, sat/control_mode, sat/fallback_active exist and follow the commands
- [x] #4 MQTT set + state for the external indoor-temp source (SATexternaltemp)
- [x] #5 HA PV-boost entities use the topics the firmware publishes (discovery capture)
- [x] #6 docs/api/MQTT.md and the manual list the new topics
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-21 RED broker capture (alpha.447+1f111ee, OTGW32 .88.61, broker 192.168.88.32): SAT active (simulation): retained sat/active true, sat/mode continuous; no sat/enabled, sat/preset, sat/control_mode, sat/fallback_active, sat/safety_tripped retained. After POST sat/enable/0 (REST: enabled false, active false) the broker still holds sat/active true and sat/mode continuous 40 s later. Evidence %TEMP%\bench-1284\red-after-disable.txt.

2026-10-21 GREEN broker capture (alpha.447+0b73c08 = this diff, OTGW32 .88.61, broker 192.168.88.32), foreground steps: (1) SAT active: retained enabled true, preset none, control_mode auto, fallback_active false, safety_tripped false, external_temp_enable true (red: none of these existed). (2) MQTT set/.../sat/external_temp 0 -> settings satexternaltemp false and sat/external_temp_enable false; 1 -> true. (3) POST sat/enable/0 -> retained enabled false, active false, mode off (red: active true, mode continuous stayed). (4) room-temp safety trip (external source off, no room temp, SAT_MAX_SKIP_COUNT ticks): REST safety_tripped true; retained safety_tripped true, enabled true, active false, mode off (red: a trip was never published). (5) re-enable with room temp: safety_tripped false, active true, mode continuous. (6) homeassistant/status online -> discovery: all six PV-boost switch/number entities have stat_t and cmd_t sat/pv_boost_* (old code: sat/boost_*), and the external_temp switch config is present. Two earlier background captures were killed by host memory pressure; this run was done in the foreground. Evidence %TEMP%\bench-1284\green-step*.txt, green-disc.txt.

2026-10-21 review workflow (3 lenses: correctness, rules, tests; 2 adversarial skeptics per finding; 39 agents): 18 findings, 16 upheld, deduplicated to 6 issues: (A) sat/mode published continuous/pwm while disabled/tripped after a control_mode command; (B) satDisable/satClearSafetyTrip publish from the async_tcp task (REST enable/settings POST), racing the loop task on shadows and the OT publish gate (TASK-1176); (C) external_temp_enable not published while disabled; (D) fallback_active can never read true over MQTT (the fallback only runs while MQTT is down) -> docs only, no new mechanism (YAGNI); (E) docs switch count 15->16, ch04 tables; (F) shadow-reset test checked zero shadows (slice ended at the forward declaration). Refuted: external_temp vs externaltemp naming; external_temp_enable published only while enabled (superseded by C). Sent back for fixes.

2026-10-21 round-1 fixes on the bench (alpha.447+0b73c08 build of the fixed diff), foreground broker capture: R1 enabled: active true, mode continuous, enabled true, control_mode auto, safety_tripped false, external_temp_enable true. R2 REST POST sat/enable/0 (async_tcp) -> published by the loop task: active false, mode off, enabled false. R3 while disabled: MQTT control_mode pwm -> sat/control_mode pwm, sat/mode stays off, REST control_mode 0 (eControlMode untouched); MQTT external_temp 0 -> external_temp_enable false (now published while disabled). R4 re-enable: active true, mode continuous. Tests: test_sat_mqtt_state 13 OK (HEAD: 8 failures + 1 error; mutation runs verified the shadow-reset and core-publish guards), evaluate --quick 72/0, whitelist/follow_thermostat/pwm_override/webui green (agent run). Docs: MQTT.md, MANUAL.md, ch04/h04 (state-only table, switch count 16, fallback_active reads false over MQTT), ch05/h05, CHANGELOG. Bench restored, settings diff vs baseline empty.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT state over MQTT is now reliable for integrations (gruijter's Homey report, 7 points). AC1: after a disable the retained state is enabled=false, active=false, mode=off (old: active=true, mode=continuous stayed). AC2: a safety trip publishes safety_tripped=true retained with active=false/mode=off, false again after re-enable (old: never published). AC3: new retained sat/enabled, sat/preset, sat/control_mode (requested mode), sat/fallback_active. AC4: MQTT command sat/external_temp + retained sat/external_temp_enable + HA switch. AC5: PV-boost HA entities point at sat/pv_boost_* (old sat/boost_*). AC6: docs. The core state is published from one place on the loop task every pass (REST/settings disables are not published from async_tcp), and sat/mode is always off while disabled or tripped, also after a control_mode command. Evidence: foreground broker captures (red + green steps), tests/test_sat_mqtt_state.py 13 OK (9 red on HEAD), 39-agent review workflow with adversarial verify, findings fixed and re-checked on the bench. Not verified: a real Homey integration; sat/fallback_active=true can not be seen over MQTT by design (the fallback only runs while MQTT is down; documented).
<!-- SECTION:FINAL_SUMMARY:END -->
