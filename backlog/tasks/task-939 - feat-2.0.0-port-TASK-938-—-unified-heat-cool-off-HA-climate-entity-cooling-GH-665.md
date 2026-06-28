---
id: TASK-939
title: >-
  feat-2.0.0: port TASK-938 — unified heat/cool/off HA climate entity (cooling,
  GH #665)
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-27 10:47'
updated_date: '2026-06-28 12:46'
labels:
  - feature
dependencies: []
ordinal: 152000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the otgw-1.x.x cooling fix (7b2d3cdd, GH #665) to the 2.0.0 dev line. Same design: new <pub>/hvac_mode (off/heat/cool) + <pub>/hvac_action (off/idle/heating/cooling) computed in OTGW-Core.ino from the OT status bits (mode=cool if master cooling_enable else heat when connected else off; action=cooling/heating/idle from slave bits, NOT flame; off on thermostat disconnect), and the climate discovery (MQTTHaDiscovery.cpp streamClimateDiscovery): modes [off,heat,cool], mode_stat_t/action_topic -> new topics, max_temp 28->30. 2.0.0 specifics: state.otBus.bThermostatState (not state.otgw); ESP32 build target. Design fully validated against real logs on the 1.x side (idle/DHW/heating/cooling, gas boiler + heatpump).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 hvac_mode/hvac_action published from OT status bits; climate entity modes [off,heat,cool] with mode/action from new topics
- [x] #2 ESP32 build green + evaluate.py green (ESP abstraction boundary respected)
- [x] #3 Behaviour parity with the 1.x implementation (7b2d3cdd)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
IMPLEMENTED + pushed to origin/dev: 94890099 (2.0.0 alpha.279). Port of 1.x 7b2d3cdd. OTGW-Core.ino: publishHvacMode/publishHvacAction (state.otBus.bThermostatState) -> /hvac_mode + /hvac_action; both helpers disconnect-first (no cool+off contradiction, addresses the ADR-156 reviewer note). MQTTHaDiscovery.cpp climate: modes [off,heat,cool], mode_stat_t/action_topic -> new topics (pass-through), max_temp 28->30. ESP32 build SUCCESS (417s, fresh firmware.bin 1963856B); evaluate.py Failed 0 (ESP abstraction clean). ADR-156 (Proposed, adr-quality 0.96 A) documents the decision; reviewer caught + fixed a bit7 spec-polarity error in the brief (no code impact). ADR-098/101/102/106 compliance verified. ADR-156 is READY to flip Proposed->Accepted (port landed + parity verified) via adr-kit. FOLLOW-UP: mirror ADR-156 to the otgw-1.x.x line (cross-worktree ADR coherence); on-device 2.0.0 verification.

ADR-156 (2.0.0) flipped Proposed->Accepted via adr-kit + pushed (5b158a12). Cross-line mirror authored on otgw-1.x.x as ADR-085 (Accepted, quality 0.90 A, lint PASS), pushed df4a3b5d. Both ADRs cross-reference each other; cross-worktree ADR coherence complete. Field validation (on-device + jelvank) deferred per maintainer.

Code landed on dev (publishHvacMode/publishHvacAction OTGW-Core.ino:2057-2085; discovery climate modes [off,heat,cool] + action_topic/hvac_action + mode_stat_t/hvac_mode MQTTHaDiscovery.cpp:2895-2965). Board status was stale at To Do; flipped to In Review. Remaining gate: on-device + field sign-off (jelvank, 1.x twin TASK-938 already cooling-confirmed). No code work outstanding.
<!-- SECTION:NOTES:END -->
