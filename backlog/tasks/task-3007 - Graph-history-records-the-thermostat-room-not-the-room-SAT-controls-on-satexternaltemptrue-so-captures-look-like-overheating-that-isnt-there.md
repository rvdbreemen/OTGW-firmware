---
id: TASK-3007
title: >-
  Graph history records the thermostat room, not the room SAT controls on
  (satexternaltemp=true), so captures look like overheating that isn't there
status: To Do
assignee: []
created_date: '2026-11-21T14:21:00Z'
updated_date: '2026-11-28T11:08:00Z'
labels:
  - graph
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1129000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
will_85903's capture (OTGW-field-data/will_85903/2026-11-21_upload/, beta.392, satexternaltemp=true, MQTT room sensor): the device graph history (ADR-193/245) column 'room' is the thermostat's reading (MsgID 24). In the 45 min of SATTICK, SAT's own room read about 1 C lower (19.3-20.1 vs graph 20.4-21.1). On 20 Nov 15:00-18:40 the graph shows SAT sending CH=1 with setpoints 38-49 C while the graph room was 21.6-22 C against a target of 21, which reads as overheating although SAT's sensor was probably about 1 C lower. Same class as TASK-2946 (Home room readout fixed in beta.402). Design open: store SAT's room in the existing slot while SAT owns the room temperature, or add a field (slot size 14 bytes, ADR-193/245; weigh RAM against value). Graph export and capture tools should label which sensor the column is.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design chosen and recorded (reuse the room field while SAT owns the room, or a separate field), with the RAM cost per slot stated
- [ ] #2 With SAT on and an external room sensor, the graph history and its CSV/JSON export carry the room SAT regulates on (or both values, labelled)
- [ ] #3 Test on the real graph-history code red before / green after; web UI Monitor graph checked
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 (BlueFlame): will_85903 sent openHAB event logs (OTGW-field-data/will_85903/2026-11-21_upload/openhab/, room_sources.csv). OTGW_SATIndoorTemperature (the room SAT regulates on, from his MQTT sensor) vs OTGW_Tr (thermostat): SAT room 0.3-1.1 C lower all day (e.g. 20 Nov 15:00 20.96 vs 21.72; 21 Nov 03:00 18.56 vs 19.62). 20 Nov 13:45-18:55 SAT room 20.54-21.67 against target 21: the afternoon burns were justified; the graph history (thermostat room 21.6-22) made it look like overheating. Confirms this task's premise with a second source. Chart: charts/overview-v2.png, posted to #beta-testing msg 1573713243640430612.

2026-11-21 otgw-webui, UI part (ADR-291 decision 4), lane/ba-3007 on origin/dev a694e4c27, not pushed (waits for ADR-291 Accepted and the firmware part, lane/d3-satroom). v2.js: GSER gets ['satRoom', 'SAT room', '#2dd4bf', 't'] between satTarget and room (own colour, solid line: a measurement, not a setpoint); GSER_LABEL_KEY satRoom -> graph.series_sat_room (EN 'SAT room', NL 'SAT-kamer', DE 'SAT-Raum', EL 'χώρος SAT'); the live 5 s slot takes sat.room from the SAT status the page already polls, only while SAT is enabled and its room is not lost (firmware rule, no extra request); mergeGraphHistory reads the optional satRoom array (tenths / scale, absent or wrong length = empty). CSV order follows GSER: epoch_ms,flow,ret,chSet,roomSet,satTarget,satRoom,room,outside,dhw,mod,flame,dhw_on,ch (sent to otgw-poc-dev, his capture commit a15bed49c matches; his test_byte_identical_to_v2js needs this v2.js). New test tests/webui/graph-sat-room.test.mjs (real GSER/pushGraphSlot/mergeGraphHistory/exportCsv): RED on origin/dev 0/4, GREEN 4/4; graph-sat-target 5/0, graph-dhw-temperature 4/0, i18n-lang 6/0, lang-menu 7/0, i18n-no-hardcoded 3/0; evaluate --quick 0 failed. Not verified: the drawn line in a browser against the real firmware column (bench after the firmware part).
Bench beta.496 2026-11-28: no satRoom graph/history check done (needs SAT active without boiler: waits for Robert's answer to the simulation-window DM). NOT verified, no AC ticked.
<!-- SECTION:NOTES:END -->
