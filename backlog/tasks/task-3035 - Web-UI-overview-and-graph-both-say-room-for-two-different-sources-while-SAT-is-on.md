---
id: TASK-3035
title: >-
  Web UI: overview and graph both say 'room' for two different sources while SAT
  is on
status: Done
assignee:
  - '@claude'
created_date: '2026-11-22T16:32:00Z'
updated_date: '2026-11-25T10:03:00Z'
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

2026-11-25 follow-up (lane/ba-3035) to the ux proposal docs/reviews/2026-11-25_room-temperature-labels/README.md; the first commit c2a2ed0d1 only covered part of it. Now: Home tile ROOM · SAT + visible line 'thermostat 19.8° · via MQTT/REST sensor' (tooltip kept; text squeezed with textLength when wider than the schematic edge), only while SAT controls on a source other than the thermostat (roomDiffers); SAT control-loop chart panel + legend 'Room (SAT)'; Monitor graph series 'room (thermostat)' only in that case, plain 'room' otherwise, plus one live line 'SAT uses 20.5° (MQTT/REST sensor). The graph shows the thermostat reading.'; thermostat without MsgID 24: 'thermostat: no reading'; room lost / thermostat source / SAT off: unchanged tile. Source names are the existing tools.sat_room_src_* strings (MQTT/REST sensor, BLE sensor, averaged room sensors, thermostat) and 'external' for an unknown source (new tools.sat_room_src_other), not the README's shorter MQTT, because external covers REST pushes too. room_temp_source comes from sat/status (TASK-3011); no new API call. EN, NL, DE, EL packs (EL written by me, not through the sync). README AC mapping: AC1 Home line, AC2 chart + graph + live line, AC3 terms in 4 packs, AC4 five states in tests/webui/room-label-sat-source.test.mjs, AC5 screenshots 1400 and 390 px light and dark in OTGW-build-artifacts-safe/evidence-TASK-3035/v2-ux/ (own harness; the ux proposal wanted them via the bench-tester on a unit, not done). Evidence: test red 0/6 on origin/dev data, green 10/10 on the fix; home-room-follows-sat 7, graph-room-dropout 1, graph-readout 6, graph-legend-dashed 2, i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4, sat-chart-empty-states 2, room-tile-sat-target 1, sat-chart-actual-flow 4 (expected key updated to legend_room_sat); evaluate.py --quick 88 pass 0 fail. Not done: SAT page room tile source tag (ux section 2, not in its ACs; the existing 'Source:' line from TASK-3011 names it); second graph series stays 2.1.0.

2026-11-25 EL unreviewed: the Greek strings of this task (and of TASK-3012) were written by otgw-webui directly, not through the translate_webui_otgw-firmware skill (no glossary, adversarial reviews or editor pass). NL/DE/EN follow the ux README as agreed. Proper EL review is TASK-3047, after the beta that carries 3035/3012.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Room labels name the thermostat reading (MsgID 24) when SAT controls on another source. Evidence: tests/webui/room-label-sat-source.test.mjs red on origin/dev data and green on the fix; python evaluate.py --quick 88 pass/0 fail at the time; EN/NL/DE packs agree (EL written by otgw-webui, unreviewed, TASK-3047). Bench screenshots in OTGW-field-data/bench-63/2026-11-25_beta-candidate/screenshots/ (build 4f303da, banner beta.428, light and dark): A1 S1 (tag ROOM, no second line), A4 S8 light+dark (ROOM · SAT, thermostat: no reading, no cut-off, no overlap), S4b graph with BLE source (legend room (thermostat) + line SAT uses 25.7 (BLE sensor)). Not reachable on the bench (no thermostat on the bench unit): A2 with a differing thermostat value (S3/S5), A3 (S6/S7), A5 (S9), the room tooltip, S16/S17 (anti-cycling idle, takeover STOPPED) and the Messages-button accessible name. The nl, de and el packs were only fetched (HTTP 200, JSON parses), not seen rendered. Those states are covered by host browser tests only. Published in tag v2.0.0-beta.428.
<!-- SECTION:FINAL_SUMMARY:END -->
