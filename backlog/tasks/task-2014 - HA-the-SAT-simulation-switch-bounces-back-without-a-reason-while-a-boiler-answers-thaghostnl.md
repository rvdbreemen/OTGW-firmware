---
id: TASK-2014
title: >-
  HA: the SAT simulation switch bounces back without a reason while a boiler
  answers (thaghostnl)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T06:27:00Z'
updated_date: '2026-11-18T17:41:00Z'
labels:
  - ha
  - sat
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 1032000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by thaghostnl in #alpha-testing 2026-11-17 20:35 UTC (https://discord.com/channels/812969634638725140/1514720723980259460/1572349389857361982), beta.170, PIC 6.8, Remeha Avanta: the discovered SAT simulation switch jumps back to off within a second; telnet shows 'SAT-SIM: enable rejected, boiler present' (refusal by design, TASK-795). Suggestion: do not offer the switch while a boiler is present, or publish it unavailable or with a reason topic. Robert replied 'I fully agree with this feedback' (22:10 UTC, message 1572373470514258021).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In HA the simulation switch is not offered as usable while a boiler answers, or shows why it refused
- [x] #2 Host test or bench evidence on a unit with a boiler
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 MAINTAINER DECISION (Robert, relayed by the coordinator): "A" (Discord DM 1572560403387523154, a reply to the coordinator's A/B/C question 1572511441158934619). A = the HA simulation switch gets a second availability topic (retained sat/simulation_available, avty list with the device LWT plus that topic, avty_mode all), so HA shows it unavailable while a boiler answers. B = keep the switch available and add a sat/simulation_blocked problem binary_sensor (C+). A conflicts with Accepted ADR-102 (HA entity availability reflects only the ESP-MQTT link) and uses the mechanism of Rejected ADR-142, so it needs a new ADR that supersedes ADR-102 for this point; acceptance of that ADR is a hard stop for Robert. Implementation follows the accepted ADR. The B/C+ work is parked as a patch in RvdB/OTGW-worktree-leftovers/wt-95-2014-cplus-20261118/ (not deleted).

2026-11-18 ADR-255 (Proposed) written via adr-kit: 'The HA simulation switch has a second availability topic and is unavailable while a boiler answers (partly supersedes ADR-102)'. User Decision line quotes Robert's 'A' (DM 1572560403387523154). Names ADR-142: same mechanism, closed as Rejected for never being built, not on a technical objection. Reciprocal related links to ADR-101/102/142/174 written with adr relate; README entry added; adr-lint --strict over docs/adr shows the same 62 FAIL files as origin (no new one); tests/test_adr_governance.py OK. Acceptance is a hard stop for Robert (via the coordinator). Implementation waits for acceptance; the plan-A code and test (sat/simulation_available in satPublishCoreState, availSub on streamSatBoolSwitch case 5, tests/test_ha_sat_sim_availability.py, red 5/7 on origin and green 7/7 earlier today) are reconstructed from this session when it is accepted.

2026-11-18 ADR-255 risk 'older HA' closed with two independent primary sources: (1) HA core git: availability list PR #37418 commit 4b3ad0a1cd is in tag 0.113.0, not 0.112.0; availability_mode PR #44987 commit d60fc0de38 (2021-01-11, files mqtt/mixins.py + abbreviations.py) is in tag 2021.2.0, not 2021.1.0 (GitHub compare); (2) HA 2021.2 release notes, home-assistant.io source/_posts/2021-02-03-release-20212.markdown:1109 'Add availability_mode "all" and "any" to MQTT entities (#44987)'. Switch support: mqtt/switch.py:85 MqttSwitch(MqttEntity), entity.py:688 MqttAvailabilityMixin, :823 AVAILABILITY_ALL; switch.mqtt.markdown documents availability (:29) and availability_mode (:52). The project documents no minimum HA version (tested with 2026.10.4, ch04:563). ADR-255 Risks updated (commit d7a206ea2, local, push freeze until 371).

2026-11-18 plan A implemented per ADR-255 (Accepted, f8bf5e97e) in lane-95-2014a: SATcontrol.ino satPublishCoreState() publishes retained sat/simulation_available = bSimulation || !satBoilerHardwarePresent(), re-armed by satRepublishCoreState(); MQTTHaDiscovery.cpp streamSatBoolSwitch() gets an optional availSub, case 5 (simulation switch) passes /sat/simulation_available and gets avty [LWT, that topic] with avty_mode all; every other switch keeps avty_t. Docs: docs/api/MQTT.md (core-state row, republish list, switch paragraph), ch04/h04 SAT_Simulation row. Evidence: tests/test_ha_sat_sim_availability.py (real core-state functions + the real discovery dispatcher via test/host/test_ha_discovery_json.py): OTGW_SRC_REV=origin/dev FAILED (failures=5 of 6), fix 6/6 OK; tests/test_ha_climate_sat_follow.py 29/29 with the satBoilerHardwarePresent stub; evaluate.py --quick 0 FAIL (88 passed, ADR-102 gate included); run_tests.py --changed 109/109. Open: bump (374, after 373), build, bench in HA (09): switch unavailable with a boiler answering or REST force_boiler_present=1, available again at 0, and sat/simulation_available on the broker.

AC1 bench by 09 (2026-11-18, .63 Classic + S3, PIC 6.8 gateway, NO boiler; simulation off, no loopback), 2.0.0-beta.374+1d124db (appelfsha256 f6a9be90... = .elf, full OTA, changed none). 'Boiler answers' EMULATED via POST /api/v2/sat/force-boiler (coordinator decision), not a real boiler. Retained homeassistant/switch/otgw-AC276ECE45D8/sat_simulation_enable/config: avty=[{t:OTGW/value/otgw-AC276ECE45D8},{t:.../sat/simulation_available,pl_avail:true,pl_not_avail:false}], avty_mode all, no avty_t; the 14 other SAT switches keep avty_t only. Retained sat/simulation_available 'true' at idle. 14:27:27.5 force-boiler 1 -> 200 force_boiler_present true, same tenth MQTT simulation_available 'false' (switch unavailable in HA); 14:27:39.6 force-boiler 0 -> 'true' again. Side note: ot_link_lost briefly false after force-boiler, true again after ~35 s (the force hook feeds satNotifyBoilerFrameSeen, not the 2014 change); the PIC reported boilerconnected false throughout. Not verified: a real HA instance showing the entity greyed out (broker-side availability topics only). .63 restored to 360. Evidence: 09 scratchpad t2014/.

2026-11-18 pushed and published (otgw-docs, on the coordinator's GO): 1d124dbc5 (2.0.0-beta.374) with a9ff5dcb8 (ADR-255 Accepted) as a fast-forward 9e6636da7..1d124dbc5, remote HEAD confirmed after a fresh fetch; zip OTGW-firmware-esp32-combo-2.0.0-beta.374+1d124db-flash.zip (6431071 bytes) posted to #alpha-testing by scripts/discord-post-build.sh, http 200, message 1572665769706135653. AC2 basis: the host test tests/test_ha_sat_sim_availability.py (red on origin before the fix, green on it) plus 09's bench run with the boiler emulated by force-boiler; a run with a real answering boiler and a real HA instance greying out the entity is a field check (thaghostnl's Remeha), not an AC. The ot_link_lost side effect of force-boiler is a test-hook effect, not part of the 2014 change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: 09 bench on 2.0.0-beta.374+1d124db (.63): retained discovery of sat_simulation_enable carries avty [device LWT, sat/simulation_available] with avty_mode all and no avty_t, the 14 other SAT switches keep avty_t; sat/simulation_available goes false on force-boiler 1 and true on force-boiler 0 (14:27:27.5 / 14:27:39.6). AC2: tests/test_ha_sat_sim_availability.py, OTGW_SRC_REV=origin/dev red (5 of 6 failed) vs fix 6/6 OK, plus that bench run. Decision: Robert 'A' (DM 1572560403387523154), ADR-255 Accepted. Pushed 1d124dbc5, zip posted (message 1572665769706135653). Not verified: a real boiler answering and a real HA instance showing the entity unavailable (broker-side topics only); field check by the reporter.
<!-- SECTION:FINAL_SUMMARY:END -->
