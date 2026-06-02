---
id: TASK-809
title: >-
  investigate(sat-webui): outside temp + boiler setpoint read null after page
  refresh (OTGW32)
status: To Do
assignee: []
created_date: '2026-06-02 05:27'
updated_date: '2026-06-02 16:28'
labels:
  - sat
  - webui
  - field-report
  - needs-repro
  - 2.0.0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report @sergeantd (alpha.99, OTGW32, 2026-05-30, immediately after a device restart): "After a refresh the outside temperature and setpoint are null." DISTINCT from TASK-764 (all four SAT tiles blank, Done/fixed at alpha.92, field-confirmed): here only outside_temp + final_setpoint (2 of 4 tiles) read null, specifically after a browser refresh. Code: data/sat.js fetches /api/v2/sat/status on load and sets sat-outside-temp = fmtTemp(d.outside_temp), sat-flow-temp = fmtTemp(d.final_setpoint) (sat.js:147, 206-207). Open question: does the backend return null for those two fields transiently right after a restart (weather forecast not yet fetched / SAT not yet computed = legitimate transient), or does the client drop them on refresh (bug)? NEEDS from @sergeantd: raw /api/v2/sat/status JSON captured right after the refresh, and how long the null persists (seconds vs permanent). Verify on current alpha.139 (finding is alpha.99-era). Related: TASK-764. 2.0.0-only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause identified with evidence: sat/status JSON after refresh shows whether outside_temp/final_setpoint are null at the source (backend) or dropped client-side
- [ ] #2 Outside temperature + boiler setpoint display correct values after a refresh when SAT is active and weather/BLE data is present; OR the null is documented as a legitimate post-restart transient with the recovery time
- [ ] #3 python build.py green (fw+fs); evaluate.py --quick no new failures
- [ ] #4 Field-confirmed by @sergeantd on OTGW32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-02 11:26 — @sergeantd confirmed STILL PRESENT on 2.0.0-alpha.139+c880a02 (replied to his original 'outside temp + setpoint null after refresh' finding). So it is a real persistent bug, not a one-off transient. Note: alpha.139 does NOT contain any SAT-dashboard fix (it is the WS heap-gate build); root-cause still needs the raw /api/v2/sat/status JSON right after a refresh (to confirm null-at-source vs client-drop) — to be captured with @sergeantd tonight.
<!-- SECTION:NOTES:END -->
