---
id: TASK-1959
title: >-
  SAT simulation: own sim-only boiler config (low-off allowed) so PWM can be
  benched without a real boiler
status: To Do
assignee: []
created_date: '2026-11-15T07:53:00Z'
labels:
  - sat
  - simulation
  - bench
dependencies: []
priority: low
ordinal: 1003000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert decided "PWM bench = A" by Discord DM 1571429393681879091 (2026-11-15, acknowledged by the coordinator in 1571429471997792327): file a low task "SAT simulation: own sim-only boiler config (e.g. low-off allowed) so PWM can be benched without a real boiler; real boilers unaffected, entry 215 stays".

Finding (d3, TASK-1942/1933 bench, 2026-11-15): satPwmPermitted() needs bSlaveCfgValid, and its only setter is satNoteSlaveConfig() (SATcycles.ino:1310), called from the MsgID 3 path in OTGW-Core.ino:3243. Since commit bdaadc5f9 (TASK-1857/1903) a loopback MsgID 3 no longer calls satNoteSlaveConfig(), so in SAT simulation without a real boiler bSlaveCfgValid stays false and PWM can never start. TASK-1933 (PWM to continuous) and TASK-1942-B could therefore not be benched.

Scope: while SAT simulation is on, SAT uses its own simulated slave config (a valid MsgID 3 with HB bit 4 = 0, low-off allowed) instead of the boiler cache. The real cached slave config, the bSlaveCfgValid of a real boiler and every non-sim path stay unchanged. Ending the simulation restores the real state (as TASK-1952 does for PID/HCR).

Sergeant D entries checked (docs/sat/sergeant-d-corrections.md): entry 213 (bit 4 = 1 blocks every PWM entry path), 214, 215 ("No PWM until a valid reply. A gateway that never gets one never uses PWM.") and 272 (keep the last valid MsgID 3). No conflict for real boilers: all four rules keep applying to the real MsgID 3. Tension, named and not resolved by us: in simulation the gateway uses PWM without a valid reply from a real boiler, which the literal wording of entry 215 excludes. Reading acted on (Robert's decision): the simulated slave config counts as the valid reply inside the simulation only; entry 215 stays for real boilers. Sergeant D is told when this is planned; if he objects, this task stops.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With SAT simulation on and no real boiler, satPwmPermitted() can become true and PWM starts on the bench (bench log from 09)
- [ ] #2 With simulation off, a real boiler MsgID 3 decides PWM exactly as before: host test shows the entry 213/215/272 behaviour unchanged
- [ ] #3 Ending the simulation restores the real slave config state (host test)
- [ ] #4 The sim path does not write the real slave config cache or the MQTT slave-config topics (code check plus host test)
<!-- AC:END -->
