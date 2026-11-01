---
id: TASK-1551
title: >-
  SAT self-tune: log the curve integral during PWM hours for heat pumps (Q-1527e
  C)
status: To Do
assignee: []
created_date: '2026-11-01T18:57:00Z'
updated_date: '2026-11-01T20:41:00Z'
labels:
  - sat
  - heat-pump
  - sat-selftune
dependencies:
  - TASK-1525
priority: low
ordinal: 704000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 205 (Q-1527e: C; written as entry 202 before the renumbering of 2026-11-01): no control change; for heat pumps the curve integral is also logged during PWM hours, marked as PWM and not used by the tuner (ADR-205 and entry 174 unchanged), so the field test of the experimental 3 hour type (TASK-1542, entry 195) shows whether real houses run warm in SAT PWM. Basis: docs/sat/sim-hp-lowload/round19.md finding 1 (model: an on/off heat pump in SAT PWM needs curve ~0.8-1.0 instead of 1.8). Builds on TASK-1525's hourly log record.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 For a heat pump (satsource 2), the hourly self-tune log record of a PWM hour carries the hour's mean curve integral and a 'pwm' mark; the tuner's decision ignores it exactly as before (host test: decision unchanged with and without the logged value)
- [ ] #2 Gas boilers and hybrids: hourly record unchanged
- [ ] #3 MQTT/REST field documented in docs/api where the hourly record is published
<!-- AC:END -->
