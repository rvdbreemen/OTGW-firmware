---
id: TASK-1012
title: 'feat(v2-webui): SAT onboarding wizard on enable (HA config-flow parity)'
status: To Do
assignee: []
created_date: '2026-07-05 07:57'
updated_date: '2026-07-05 08:11'
labels: []
dependencies: []
ordinal: 212000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Guided SAT setup that fires when a user enables SAT in the v2 web UI, mirroring the Home Assistant SAT config-flow questions. Second instance of the TASK-997 wizard engine. Design: docs/superpowers/specs/2026-07-05-sat-onboarding-design.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Wizard fires on satenabled false->on transition when sat_onboarded is false; SAT stays disabled until Finish
- [ ] #2 8 screens: Welcome, Heating system, Room temp (BLE roster), Outside temp (OT/weather), Manufacturer, Tuning (auto/manual), Sources health, Done
- [ ] #3 Every answer writes an existing firmware key; only new firmware setting is sat_onboarded bool (default false all installs)
- [ ] #4 Existing SAT users (satenabled true, sat_onboarded false) get the wizard once on next SAT-page visit; dismiss keeps SAT running
- [ ] #5 Re-run entry in both SAT page and Advanced>System
- [ ] #6 Sources health screen reads /api/v2/health + sat/status, green/amber, warns on missing critical source
- [ ] #7 build.py --target esp32 green; evaluate.py --quick no new failures; light+dark, 360px verified on device
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Impl plan: docs/superpowers/plans/2026-07-05-sat-onboarding-wizard.md. 4 tasks: (1) firmware sat_onboarded flag, (2) startSatOnboarding() 6-screen engine, (3) three triggers + re-run buttons, (4) on-device e2e. No source pickers; no raw P/I/D. DO NOT implement yet — design tool picks up wizard UI first.
<!-- SECTION:PLAN:END -->
