---
id: TASK-1776
title: 'Monitor graph and CSV export: add the DHW temperature (MsgID 26) series'
status: To Do
assignee: []
created_date: '2026-11-10T09:41:00Z'
updated_date: '2026-11-14T21:40:00Z'
labels:
  - web-ui
  - feature-request
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
ordinal: 873000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester request (fegyosz_47807, #alpha-testing 2026-11-10 08:45:41 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1569619039276245013): "I am missing the DHW temperature from the graph and from the CSV export. My boyler has a small tank in it for DHW." Today the 24 h graph ring and its CSV export hold flow, return, control setpoint, room setpoint, room, outside, modulation and the flame/DHW/CH bits, but no DHW temperature (MsgID 26). The ring layout is fixed by ADR-193 (14-byte slots), so adding a series changes the slot layout and probably needs an amending ADR. Robert decides whether and how.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decided whether the DHW temperature is added (and whether an amending ADR to ADR-193 is needed)
- [ ] #2 If added: the graph shows a DHW temperature series and the CSV export has a DHW temperature column, shown as a dash when the boiler does not report MsgID 26
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Time correction (2026-11-10): the report time above is local CET. From the message snowflake the correct time is 2026-11-10 07:45:41 UTC (msg 1569619039276245013).

2026-11-14 Robert (DM 1571274763941449850, relayed by the coordinator, answer 'A'): A, yes, only if it fits in the existing ADR-193 14-byte slot; otherwise it comes back to Robert as an ADR-193 amendment. Label needs-maintainer-ok removed.

2026-11-14 (92): does NOT fit the existing slot, so per Robert's condition it goes back to him. GraphHistSlot (graphHistoryStuff.h:52-62, static_assert ==14 at :63) is full: six int16 temperatures (flow, ret, chSet, roomSet, room, outside) = 12 B + mod uint8 + flags uint8 = 14 B; the flags byte has 3 bits in use, not enough for a temperature. Options for Robert: A) slot 14 -> 16 B with an int16 DHW column, an ADR-193 amendment (superseding ADR via adr-kit): .bss +252 B on boards without PSRAM (1764 -> 2016 B), PSRAM block +1452 B (10164 -> 11616 B), JSON +1 column, full history after a reload; B) a live-only DHW series in the browser from MsgID 26 (v2.js:2380 already decodes it into model.dhw): no firmware/ADR change, the graph and the CSV (exportCsv uses the browser samples, v2.js:5817) show it from page load on, but no backfill after a reload; C) leave it. Recommendation from 92: B now (zero RAM, covers the tester's ask while the page is open), A only if the 24 h history after a reload is wanted, because the 252 B come from the tight internal RAM of the non-PSRAM boards.
<!-- SECTION:NOTES:END -->
