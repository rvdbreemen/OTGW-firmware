---
id: TASK-3007
title: >-
  Graph history records the thermostat room, not the room SAT controls on
  (satexternaltemp=true), so captures look like overheating that isn't there
status: To Do
assignee: []
created_date: '2026-11-21T14:21:00Z'
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
