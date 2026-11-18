---
id: TASK-2014
title: >-
  HA: the SAT simulation switch bounces back without a reason while a boiler
  answers (thaghostnl)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T06:27:00Z'
updated_date: '2026-11-18T12:42:00Z'
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
- [ ] #1 In HA the simulation switch is not offered as usable while a boiler answers, or shows why it refused
- [x] #2 Host test or bench evidence on a unit with a boiler
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 MAINTAINER DECISION (Robert, relayed by the coordinator): "A" (Discord DM 1572560403387523154, a reply to the coordinator's A/B/C question 1572511441158934619). A = the HA simulation switch gets a second availability topic (retained sat/simulation_available, avty list with the device LWT plus that topic, avty_mode all), so HA shows it unavailable while a boiler answers. B = keep the switch available and add a sat/simulation_blocked problem binary_sensor (C+). A conflicts with Accepted ADR-102 (HA entity availability reflects only the ESP-MQTT link) and uses the mechanism of Rejected ADR-142, so it needs a new ADR that supersedes ADR-102 for this point; acceptance of that ADR is a hard stop for Robert. Implementation follows the accepted ADR. The B/C+ work is parked as a patch in RvdB/OTGW-worktree-leftovers/wt-95-2014-cplus-20261118/ (not deleted).

2026-11-18 ADR-255 (Proposed) written via adr-kit: 'The HA simulation switch has a second availability topic and is unavailable while a boiler answers (partly supersedes ADR-102)'. User Decision line quotes Robert's 'A' (DM 1572560403387523154). Names ADR-142: same mechanism, closed as Rejected for never being built, not on a technical objection. Reciprocal related links to ADR-101/102/142/174 written with adr relate; README entry added; adr-lint --strict over docs/adr shows the same 62 FAIL files as origin (no new one); tests/test_adr_governance.py OK. Acceptance is a hard stop for Robert (via the coordinator). Implementation waits for acceptance; the plan-A code and test (sat/simulation_available in satPublishCoreState, availSub on streamSatBoolSwitch case 5, tests/test_ha_sat_sim_availability.py, red 5/7 on origin and green 7/7 earlier today) are reconstructed from this session when it is accepted.

2026-11-18 ADR-255 risk 'older HA' closed with two independent primary sources: (1) HA core git: availability list PR #37418 commit 4b3ad0a1cd is in tag 0.113.0, not 0.112.0; availability_mode PR #44987 commit d60fc0de38 (2021-01-11, files mqtt/mixins.py + abbreviations.py) is in tag 2021.2.0, not 2021.1.0 (GitHub compare); (2) HA 2021.2 release notes, home-assistant.io source/_posts/2021-02-03-release-20212.markdown:1109 'Add availability_mode "all" and "any" to MQTT entities (#44987)'. Switch support: mqtt/switch.py:85 MqttSwitch(MqttEntity), entity.py:688 MqttAvailabilityMixin, :823 AVAILABILITY_ALL; switch.mqtt.markdown documents availability (:29) and availability_mode (:52). The project documents no minimum HA version (tested with 2026.10.4, ch04:563). ADR-255 Risks updated (commit d7a206ea2, local, push freeze until 371).

2026-11-18 plan A implemented per ADR-255 (Accepted, f8bf5e97e) in lane-95-2014a: SATcontrol.ino satPublishCoreState() publishes retained sat/simulation_available = bSimulation || !satBoilerHardwarePresent(), re-armed by satRepublishCoreState(); MQTTHaDiscovery.cpp streamSatBoolSwitch() gets an optional availSub, case 5 (simulation switch) passes /sat/simulation_available and gets avty [LWT, that topic] with avty_mode all; every other switch keeps avty_t. Docs: docs/api/MQTT.md (core-state row, republish list, switch paragraph), ch04/h04 SAT_Simulation row. Evidence: tests/test_ha_sat_sim_availability.py (real core-state functions + the real discovery dispatcher via test/host/test_ha_discovery_json.py): OTGW_SRC_REV=origin/dev FAILED (failures=5 of 6), fix 6/6 OK; tests/test_ha_climate_sat_follow.py 29/29 with the satBoilerHardwarePresent stub; evaluate.py --quick 0 FAIL (88 passed, ADR-102 gate included); run_tests.py --changed 109/109. Open: bump (374, after 373), build, bench in HA (09): switch unavailable with a boiler answering or REST force_boiler_present=1, available again at 0, and sat/simulation_available on the broker.
<!-- SECTION:NOTES:END -->
