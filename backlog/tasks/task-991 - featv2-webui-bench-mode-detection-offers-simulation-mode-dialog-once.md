---
id: TASK-991
title: 'feat(v2-webui): bench-mode detection offers simulation-mode dialog once'
status: To Do
assignee: []
created_date: '2026-07-03 04:12'
labels: []
dependencies: []
ordinal: 203000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When device/info shows PIC interface, no boiler+thermostat+otgw connected, not already simulating: show sim-modal offering POST settings simulation=true. Remembered in localStorage. Firmware refuses sim when real boiler present (satBoilerHardwarePresent).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Modal appears once on bench setup (PIC, empty bus)
- [ ] #2 Enable simulation POSTs setting and toasts
- [ ] #3 Never appears on OT-Direct or with live bus
<!-- AC:END -->
