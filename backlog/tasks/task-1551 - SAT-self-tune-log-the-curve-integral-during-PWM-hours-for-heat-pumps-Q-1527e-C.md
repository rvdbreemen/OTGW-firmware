---
id: TASK-1551
title: >-
  SAT self-tune: log the curve integral during PWM hours for heat pumps (Q-1527e
  C)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T18:57:00Z'
updated_date: '2026-11-02T23:49:00Z'
labels:
  - sat
  - heat-pump
  - sat-selftune
dependencies:
  - TASK-1525
priority: medium
ordinal: 704000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 205 (Q-1527e: C; written as entry 202 before the renumbering of 2026-11-01): no control change; for heat pumps the curve integral is also logged during PWM hours, marked as PWM and not used by the tuner (ADR-205 and entry 174 unchanged), so the field test of the experimental 3 hour type (TASK-1542, entry 195) shows whether real houses run warm in SAT PWM. Basis: docs/sat/sim-hp-lowload/round19.md finding 1 (model: an on/off heat pump in SAT PWM needs curve ~0.8-1.0 instead of 1.8). Builds on TASK-1525's hourly log record.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 For a heat pump (satsource 2), the hourly self-tune log record of a PWM hour carries the hour's mean curve integral and a 'pwm' mark; the tuner's decision ignores it exactly as before (host test: decision unchanged with and without the logged value)
- [x] #2 Gas boilers and hybrids: hourly record unchanged
- [x] #3 MQTT/REST field documented in docs/api where the hourly record is published
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: PWM-tick signal summed (SATcontrol.ino:5486) and logged in SATTUNEH; reason first-wins (:5477), telnet only, no MQTT/REST field. Entry 205 decided.

2026-11-03: the curve signal is already accumulated in PWM hours (satTuneAddTick: satActive && !dhw); added a per-hour pwmTicks counter and, for satsource heat pump only, a pwm=<ticks> field in SATTUNEH (reason shows only the first exclusion, so it is the reliable PWM mark). Tuner unchanged. tests/test_sat_curve_selftune.py 59/59; old code: the 2 heat-pump tests fail (+ the mixed-version source check); gas/hybrid-unchanged and decision-unchanged tests pass on both. AC3: SATTUNEH is published only in the telnet SAT log (no MQTT/REST), documented in docs/sat/sat-log-format.md; nothing in docs/api to change. evaluate 0 failed.
<!-- SECTION:NOTES:END -->
