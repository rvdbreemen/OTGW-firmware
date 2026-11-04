---
id: TASK-997
title: >-
  feat(v2-webui): first-time-setup onboarding wizard (from design OTGW
  Onboarding.dc.html)
status: Done
assignee:
  - '@claude'
created_date: '2026-07-03 22:45'
updated_date: '2026-07-03 23:13'
labels: []
dependencies: []
ordinal: 209000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the 4-screen onboarding from the design system project eb26ec05: Welcome -> Appliance (gas/heat-pump -> SAT heating source) -> Home Assistant (MQTT broker/port/user/pass/topic/discovery + test connection) -> Done. Translate the DesignCanvas sc-if/DCLogic prototype into vanilla v2 DOM. Trigger on first-time (new settings.ui.bOnboarded flag, default false; set true on finish/skip; re-runnable from Settings). Wire to real settings + MQTT test endpoint. Validate on-device.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 4 screens render matching the design (welcome/appliance/HA/done) with progress bar
- [x] #2 Appliance choice writes SAT heating source; MQTT fields write mqtt settings; Test uses the real MQTT test
- [x] #3 Shows once on first-time (bOnboarded=false), dismissable, re-runnable from Settings
- [x] #4 On-device validated with evidence
<!-- AC:END -->
