---
id: TASK-241
title: 'Investigate: Tado X DHW control not reflecting correctly via OTGW'
status: Done
assignee: []
created_date: '2026-04-09 16:49'
updated_date: '2026-04-09 20:36'
labels:
  - investigated
  - not-a-bug
dependencies: []
references:
  - 'Discord #beta-testing'
  - user crashevans
  - '2026-04-09'
  - 'Discord #english-support'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User crashevans reports that with Tado X controller + Ideal Logic Combi ESP1 35 boiler + OTGW, DHW temperature set via Tado app does not align with boiler display / OTGW / HA. App defaults to 43°C, changing it in app does not propagate correctly. Also: ID 14 (MaxRelModLevelSetting) is sent as 0.00% and boiler replies Unknown-Data-Id, causing setpoint-led cycling instead of modulation. Previously worked with Tado V3 + same setup. Sergeant D explains that Ideal boilers do not support max relative modulation management via OT — boiler only accepts CS/TSet control. DHW issue needs logs to determine if firmware or config.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Determine if DHW control misalignment is a firmware bug or Tado X / config issue
- [x] #2 If firmware bug: DHW setpoint sent by Tado X is correctly propagated through OTGW to boiler and HA
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-09: crashevans shared 8 log attachments in #beta-testing (not accessible via Discord MCP). Sergeant D (sergeantd) confirmed Ideal boilers do not support max relative modulation management — Tado therefore drives via TSet causing cycling. The 43°C DHW default comes from the mqttha.cfg initial value. Need raw OTGW logs and HA screenshots to isolate DHW discrepancy.

2026-04-09: Analysed DHW_Debug.txt (6000 lines, 10:54-10:58, captured during reported DHW changes).

Findings:
- OT ID 56 (TdhwSet) ABSENT from all 6000 lines, including both test moments
- Tado X does NOT send DHW setpoint via OT — only sets DHW enable flag (ID 0 bit 1)
- Ideal boiler declares DHW setpoint as read-only (ID 6 RBP: rbp_rw_dhw_setpoint=OFF)
- No SW command active during log period (no event_report SW response)
- ID 14 cycling confirmed as Ideal boiler hardware limitation
- 43 C in HA is mqttha.cfg initial default, not a real OT reading

Conclusion: NOT a firmware bug. Tado X relies on boiler internal thermostat for DHW temp.
Replied to crashevans in #beta-testing with full explanation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Investigation closed: not a firmware bug.

Tado X does not use OT ID 56 (TdhwSet) for DHW temperature control — it only sets the DHW enable flag (ID 0 bit 1) and leaves temperature control to the boiler internal thermostat. The Ideal boiler also declares DHW setpoint as read-only (rbp_rw_dhw_setpoint=OFF), meaning even OTGW SW override commands may be rejected.

The 43 C default shown in HA is the mqttha.cfg initial placeholder, never updated because ID 56 never appears on the bus.

ID 14 (MaxRelModLevelSetting) cycling is a confirmed Ideal boiler hardware limitation, not fixable in firmware.

User notified in #beta-testing with full explanation and workaround guidance (use maxdhwsetpt MQTT command to set SW override if DHW temp control is needed).
<!-- SECTION:FINAL_SUMMARY:END -->
