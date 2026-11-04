---
id: TASK-1361
title: >-
  SAT: apply the per-system cold setpoint (28.2/21) to the
  overshoot-at-flame-off check, as thermo-nova device/status.py:121
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T20:26:00Z'
updated_date: '2026-11-04T07:13:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 496000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-199 open question 1): port thermo-nova's COLD_SETPOINT use in device/status.py:121 (did_overshoot_at_flame_off returns False when the setpoint is below COLD_SETPOINT) with our per-system cold setpoint (entries 7/11: 28.2 radiators / 21 underfloor). The area.py:175/:322/:331 sites go with the zone port (Q18, Sergeant D). Touches cycle/flame classification: plan shown to Sergeant D first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The firmware's overshoot-at-flame-off check ignores setpoints below the per-system cold setpoint (host test red on HEAD, green); plan posted to Sergeant D first
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no cold-setpoint gate at SATcontrol.ino:818. Decided (ADR-199), plan to Sergeant D.

2026-11-04 Robert decided (maintainer decision, entries 257-262; Sergeant D informed msg 1567417361798144021, may object). See docs/sat/sergeant-d-corrections.md.

2026-11-04 implemented (sonnet agent; lead reran on the integration tree): tests/test_sat_status_display.py 8 OK (old code 7 failures), mqtt_contract_1485 15, sim_port_equivalence 4, last_cycle_expiry 10, overshoot_sustain 62 OK, evaluate 0 failed. Tag beta.67 (one commit for TASK-1360, TASK-1356, TASK-1361).
<!-- SECTION:NOTES:END -->
