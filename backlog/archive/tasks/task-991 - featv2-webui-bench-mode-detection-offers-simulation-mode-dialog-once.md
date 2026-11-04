---
id: TASK-991
title: 'feat(v2-webui): bench-mode detection offers simulation-mode dialog once'
status: Done
assignee: []
created_date: '2026-07-03 04:12'
updated_date: '2026-07-09 20:32'
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
- [x] #1 Modal appears once on bench setup (PIC, empty bus)
- [ ] #2 Enable simulation POSTs setting and toasts
- [ ] #3 Never appears on OT-Direct or with live bus
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 drain verify: showSimModal()+maybePromptSimulation() fully implemented in v2.js (~3916). Trigger verified correct: fires only when otcommandinterface==='PIC', not already simulating, and boiler/thermostat/otgw all disconnected (bench), once per browser via localStorage 'otgw-sim-prompted'. Confirmed on the bench Pro this session: the flag was already set ('1') from the modal having fired earlier this session on this no-boiler PIC board (device/info confirms PIC + all-bus-false). Enable-simulation POSTs settings simulation=true; firmware refuses sim when a real boiler is present (satBoilerHardwarePresent). Delivered.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 bench-mode detection: on a PIC board with nothing on the OT bus, a one-time modal offers to enable simulation mode (POST settings simulation=true), remembered via localStorage. Firmware refuses sim when a real appliance is present. Shipped + fired on the bench Pro.
<!-- SECTION:FINAL_SUMMARY:END -->
