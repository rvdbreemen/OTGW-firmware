---
id: TASK-1527
title: >-
  SAT heat pumps at low load: minimum switching with maximum comfort (study,
  then design with Sergeant D)
status: To Do
assignee: []
created_date: '2026-10-31T13:48:00Z'
labels:
  - sat
  - design
dependencies: []
ordinal: 683000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-31: 'de warmtepomp moet minimaal schakelen. Dus goal voor warmtepomp is minimaal aantal schakelingen en maximaal comfort.' Objective for heat pumps: minimise compressor starts, maximise comfort (time within +-0.3 C of the room target). Finding that triggered it (TASK-1519 rounds 4-11, docs/sat/sim-1519/evaluation-2026-10-31.md section 4): in the model a heat pump that modulates only down to 60 % makes about 27-33 starts per day at low load (30 % type: 5-7), indicative only because the model's cycling is not validated against a heat-pump capture. Current firmware: HP PWM period 1800 s (2400 s option, SATcontrol.ino:440-446), MM kept at max in PWM for heat pumps (SATcontrol.ino:453-462, thermo-nova uses 0), max flow 40 C when nothing is chosen (entry 155), min on 180 s. The curve self-tune (ADR-205) cannot reduce starts. Study first, then propose options to Sergeant D one question at a time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A heat-pump capture (>= 24 h, TASK-1523 tool: flame/compressor, modulation, flow, return, SAT log) is used to validate the model's start count within 25 %
- [ ] #2 Simulation of candidate settings (PWM period, minimum on/off time, flow cap, MM in PWM, hysteresis) scored on compressor starts per day first and comfort second, paired against current settings, over at least 5 winters; data and scripts in docs/sat/sim-hp-lowload/
- [ ] #3 Options with numbers posted to Sergeant D one question at a time; his answers recorded in docs/sat/sergeant-d-corrections.md
<!-- AC:END -->
