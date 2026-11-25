---
id: TASK-3035
title: >-
  Web UI: overview and graph both say 'room' for two different sources while SAT
  is on
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-22T16:32:00Z'
updated_date: '2026-11-25T05:16:00Z'
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
- [x] #1 While SAT is on, the graph series and tooltip name the thermostat's OpenTherm room temperature (MsgID 24) distinctly, and show SAT's room source too or say which source SAT uses
- [x] #2 The overview and graph labels agree in the EN, NL and DE language packs
- [x] #3 Screenshots before and after with synthetic data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-ux-designer: label proposal and mockup in docs/reviews/2026-11-25_room-temperature-labels/ (README.md + mockup.html). Recommended for 2.0.0 (web UI only): vocabulary room (thermostat) / room (SAT), qualifier only when SAT uses another source; Home tile label ROOM · SAT + visible sub-line 'thermostat 19.8° · via MQTT'; SAT chart panel/legend 'Room (SAT)'; Monitor graph series 'room (thermostat)' + one live line 'SAT uses 20.5° (MQTT)'. A second graph series needs a firmware change: the 24 h ring has one room slot, filled from MsgID 24 (graphHistoryStuff.ino:66, ADR-193), so it is a 2.1.0 option. Sharper ACs are in the README section 6. Open question for webui/dev: does /api/v2/sat/status name the room source (only external_temp_valid seen, v2.js:7737)?
2026-11-25 fix (lane/ba-3035): overview ROOM tag reads 'ROOM · SAT' while SAT's room is shown, its tooltip names SAT's source ('Room temperature SAT controls on (BLE sensor); the thermostat reports 19.8°'); the graph series that is always MsgID 24 is 'thermostat room', and while SAT regulates on a room temperature its label says '(SAT uses BLE sensor)'. Source comes from sat/status room_temp_source (TASK-3011); no new API call, no invented value. Packs EN/NL/DE (+EL) agree: graph.series_room, graph.series_room_sat, home.room_tag_sat, home.room_sat_tip. Showing SAT's room as its own graph series stays TASK-3007 (needs the firmware slot, ADR-291). Evidence: tests/webui/room-label-sat-source.test.mjs red on origin/dev data (legend 'room', 0 pass 4 fail), green on the fix (4 pass); home-room-follows-sat 7, graph-room-dropout 1, graph-readout 6, i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4 all pass; evaluate.py --quick 88 pass 0 fail, same as without the change. Before/after screenshots with synthetic data: OTGW-build-artifacts-safe/evidence-TASK-3035/ (before/, after/, SHA256SUMS). Not verified: on a real unit (only the host Chrome harness); full webui suite runs on the NAS via 09. Pre-existing, not changed: the Home note 'set 20.7° (thermostat 2…' is cut off at the card edge on origin/dev too.
<!-- SECTION:NOTES:END -->
