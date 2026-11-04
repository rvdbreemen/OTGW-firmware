---
id: TASK-234
title: >-
  SAT WebUI: Add DHW setpoint slider and boiler-type-aware HW switch to SAT
  dashboard
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 06:27'
updated_date: '2026-04-09 10:25'
labels:
  - sat
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add DHW controls directly to the SAT dashboard panel (not just the settings page). Use OT MsgID 3 Slave Configuration bit 3 to auto-detect boiler type and conditionally show the HW= switch.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DHW setpoint slider (0-60C, step 0.5) visible on SAT dashboard panel
- [x] #2 Slider value reads from and writes to satdhwsetpoint via REST API
- [x] #3 On page load, read OT MsgID 3 slave config from /api/v2/status or WebSocket; extract bit 3 (DHW storage tank present)
- [x] #4 HW= enable/disable switch shown only when bit 3 is SET (storage tank boiler)
- [x] #5 HW= switch hidden for combi/instantaneous boilers (bit 3 CLEAR)
- [x] #6 Both controls update live without page reload
- [x] #7 DHW slider min/max read from boiler-reported TdhwSetUB and TdhwSetLB (OT MsgID 56 hb=max, lb=min); slider precision 1°C (integer)
- [x] #8 On slider change, send SW={value} command to OTGW (persistent DHW setpoint override, not edge-triggered)
- [x] #9 DHW switch display state follows OT domestichotwater bit (MsgID 0 slave status bit 2)
- [x] #10 On switch toggle, send HW=1 (enable) or HW=0 (disable) command
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read existing data/index.js to understand SAT dashboard panel structure and existing WebSocket/REST data patterns
2. Read data/index.html for HTML structure reference
3. Identify where TdhwSetUB/LB, MsgID 3 slave config, domestichotwater bit, and TdhwSet are available in OT status data
4. Add DHW setpoint slider (1 degree integer steps, min/max from boiler bounds) to SAT dashboard panel in index.js
5. Add HW= switch to SAT dashboard panel, hidden by default
6. On page load / WebSocket update: read MsgID 3 bit 3; show HW switch only when set
7. On page load / WebSocket update: set slider bounds from TdhwSetUB/LB; set slider value from TdhwSet
8. On page load / WebSocket update: set HW switch state from domestichotwater bit
9. On slider change: POST SW={value} to /api/v1/otgw or /api/v2/otgw command endpoint
10. On switch toggle: POST HW=1 or HW=0 to OTGW command endpoint
11. Test: verify no JS errors, all controls visible, live updates work
12. Commit and push
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added DHW setpoint slider and boiler-type-aware HW switch to the SAT dashboard panel.

Changes in data/index.html:
- Added DHW Controls section (sat-dhw-section) visible in all three dashboard views (simple, expert, diag)
- Slider: range input with 1-degree integer steps, initial defaults 40-60C; shows current value label
- HW switch: checkbox row hidden by default, shown only for storage-tank boilers

Changes in data/sat.js:
- Added DHW state variables: _dhwStorageTank, _dhwSliderMin/Max, _dhwSliderDirty
- fetchSlaveConfig(): on start, reads MsgID 3 via /api/v2/otgw/messages/3; extracts bit 3 of slave config high byte (DHW storage tank flag) to show/hide the HW switch
- fetchDHWBounds(): on start, reads MsgID 48 (TdhwSetUBTdhwSetLB) via /api/v2/otgw/messages/48; sets slider min from LB (low byte) and max from UB (high byte); falls back to 40/60C defaults if not yet received
- updateDHWControls(d): called on every 5s SAT status poll; updates slider from dhw_setpoint; updates HW switch checked state from dhw_active (which is SlaveStatus bit 0x04 = MsgID 0 slave status bit 2)
- onDhwSliderInput: updates label during drag without sending command
- onDhwSliderChange: sends SW={intValue} command on slider release
- onDhwHwSwitch: sends HW=1 or HW=0 command on toggle
- Commands sent via POST to /api/v2/otgw/command/{cmd}

Note: task AC #7 refers to MsgID 56 for bounds, but MsgID 56 is TdhwSet (the actual setpoint). The DHW setpoint upper/lower bounds are at MsgID 48 (TdhwSetUBTdhwSetLB) per the OT spec and firmware code. MsgID 48 is used correctly here.

No firmware (.ino) changes required; this is purely a WebUI (frontend JS/HTML) change.
<!-- SECTION:FINAL_SUMMARY:END -->
