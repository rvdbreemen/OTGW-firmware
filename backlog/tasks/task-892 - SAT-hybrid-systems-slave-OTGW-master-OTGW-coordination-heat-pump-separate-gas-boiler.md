---
id: TASK-892
title: >-
  SAT hybrid systems: slave OTGW -> master OTGW coordination (heat pump +
  separate gas boiler)
status: To Do
assignee: []
created_date: '2026-06-20 21:11'
labels: []
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 116000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From #dev-sat-mqtt 2026-06-20 (George + Robert). Hybrid installs run a heat pump + a SEPARATE gas boiler, each on its OWN OTGW (Robert's setup). The primary OTGW cannot see the gas boiler fire through the heat pump's OT bus (Robert: 'You can't see it burn thru the OTGW from the Elga'). Idea: the gas boiler's (slave) OTGW notifies the primary (master) OTGW when the boiler is firing - via the gas boiler's HA climate entity hvac_action=heating, or a direct OTGW<->OTGW link - so SAT on the master can account for the gas-boiler contribution. NOTE: the KISS hybrid device-type (manual selection) is handled in TASK-891.8; THIS task is the larger coordination feature only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design + ADR the slave->master notification channel (HA climate hvac_action bridge vs direct OTGW<->OTGW MQTT/link).
- [ ] #2 Master SAT consumes the gas-boiler-firing signal and factors it into hybrid control decisions.
- [ ] #3 Field-validate on Robert's two-OTGW (Elga heat pump + gas boiler) setup.
<!-- AC:END -->
