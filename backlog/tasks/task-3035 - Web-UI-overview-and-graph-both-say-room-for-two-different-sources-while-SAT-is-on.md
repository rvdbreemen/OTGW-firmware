---
id: TASK-3035
title: >-
  Web UI: overview and graph both say 'room' for two different sources while SAT
  is on
status: To Do
assignee: []
created_date: '2026-11-22T16:32:00Z'
updated_date: '2026-11-25T05:13:00Z'
labels:
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1143000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Evidence: will_85903 beta.410 screenshots (OTGW-field-data/will_85903/2026-11-22_beta.410_capture/followup_1574099094702395584/): at 12:00:55 the overview card shows 20.5 (SAT's room source, MQTT push; v2.js roomFromSat/showRoom, TASK-2946) and the graph tooltip 'room' shows 21.1 (MsgID 24, graphHistoryStuff.ino:66); the tester read them as one value. Both are labelled 'room' while they come from different sources when SAT regulates on another source than the thermostat. Related: TASK-3007 (graph history satRoom column, ADR-291, deferred to 25 Nov) and TASK-3011/3015 (sat/room_temp_source). No code until the token budget hold ends (Wed 25 Nov).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 While SAT is on, the graph series and tooltip name the thermostat's OpenTherm room temperature (MsgID 24) distinctly, and show SAT's room source too or say which source SAT uses
- [ ] #2 The overview and graph labels agree in the EN, NL and DE language packs
- [ ] #3 Screenshots before and after with synthetic data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-ux-designer: label proposal and mockup in docs/reviews/2026-11-25_room-temperature-labels/ (README.md + mockup.html). Recommended for 2.0.0 (web UI only): vocabulary room (thermostat) / room (SAT), qualifier only when SAT uses another source; Home tile label ROOM · SAT + visible sub-line 'thermostat 19.8° · via MQTT'; SAT chart panel/legend 'Room (SAT)'; Monitor graph series 'room (thermostat)' + one live line 'SAT uses 20.5° (MQTT)'. A second graph series needs a firmware change: the 24 h ring has one room slot, filled from MsgID 24 (graphHistoryStuff.ino:66, ADR-193), so it is a 2.1.0 option. Sharper ACs are in the README section 6. Open question for webui/dev: does /api/v2/sat/status name the room source (only external_temp_valid seen, v2.js:7737)?
<!-- SECTION:NOTES:END -->
