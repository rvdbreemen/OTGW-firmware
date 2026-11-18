---
id: TASK-2014
title: >-
  HA: the SAT simulation switch bounces back without a reason while a boiler
  answers (thaghostnl)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T06:27:00Z'
updated_date: '2026-11-18T10:48:00Z'
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
- [ ] #2 Host test or bench evidence on a unit with a boiler
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 MAINTAINER DECISION (Robert, relayed by the coordinator): "A" (Discord DM 1572560403387523154, a reply to the coordinator's A/B/C question 1572511441158934619). A = the HA simulation switch gets a second availability topic (retained sat/simulation_available, avty list with the device LWT plus that topic, avty_mode all), so HA shows it unavailable while a boiler answers. B = keep the switch available and add a sat/simulation_blocked problem binary_sensor (C+). A conflicts with Accepted ADR-102 (HA entity availability reflects only the ESP-MQTT link) and uses the mechanism of Rejected ADR-142, so it needs a new ADR that supersedes ADR-102 for this point; acceptance of that ADR is a hard stop for Robert. Implementation follows the accepted ADR. The B/C+ work is parked as a patch in RvdB/OTGW-worktree-leftovers/wt-95-2014-cplus-20261118/ (not deleted).

2026-11-18 ADR-255 (Proposed) written via adr-kit: 'The HA simulation switch has a second availability topic and is unavailable while a boiler answers (partly supersedes ADR-102)'. User Decision line quotes Robert's 'A' (DM 1572560403387523154). Names ADR-142: same mechanism, closed as Rejected for never being built, not on a technical objection. Reciprocal related links to ADR-101/102/142/174 written with adr relate; README entry added; adr-lint --strict over docs/adr shows the same 62 FAIL files as origin (no new one); tests/test_adr_governance.py OK. Acceptance is a hard stop for Robert (via the coordinator). Implementation waits for acceptance; the plan-A code and test (sat/simulation_available in satPublishCoreState, availSub on streamSatBoolSwitch case 5, tests/test_ha_sat_sim_availability.py, red 5/7 on origin and green 7/7 earlier today) are reconstructed from this session when it is accepted.
<!-- SECTION:NOTES:END -->
