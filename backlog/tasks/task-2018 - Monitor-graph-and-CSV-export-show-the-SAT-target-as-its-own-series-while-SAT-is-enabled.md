---
id: TASK-2018
title: >-
  Monitor graph and CSV export: show the SAT target as its own series while SAT
  is enabled
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T09:30:00Z'
updated_date: '2026-11-18T12:31:00Z'
labels:
  - webui
  - sat
  - graph
dependencies: []
priority: medium
ordinal: 1034000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
92's analysis of indigo's graph CSVs (via coordinator, 2026-11-18): the Monitor graph and its CSV export plot roomSet = MsgID 16 TrSet (v2.js:2392, live model; graphHistoryStuff.ino:61 for the device ring), also while SAT is active. The setpoint that actually drives the boiler under SAT is the SAT target, which the graph never shows. Checked 2026-11-18 in wt-ba-2015: the 24 h/7 d device history ring (ADR-193, ADR-245) does NOT store the SAT target: GraphHistSlot (graphHistoryStuff.h:62-72) holds flow, ret, chSet, roomSet, room, outside, mod, flags, static_assert 14 bytes. Options: A) add an int16 satTarget column (slot 14 -> 16 bytes, ring RAM/PSRAM +14 %, ring format and /graph-history response change: needs an ADR amendment of ADR-193/245 and Robert's OK); B) live-only series in the browser from /v2/sat/status (no ring change, history panel shows it only for the open-tab span); C) write the SAT target into the roomSet column while SAT is active (no format change, but the column's meaning becomes mode-dependent). Do after TASK-2015 / beta.371.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The Monitor graph shows a separate SAT target series while SAT is enabled, with its own legend entry and translation in en/nl/de/el
- [ ] #2 The CSV export carries the SAT target as its own column
- [ ] #3 Decision on the history source (ring column vs live-only) recorded in the task; a ring format change goes through an ADR amending ADR-193/245
- [ ] #4 Web UI test drives the real v2.js: red on the old code, green on the fix; screenshot with synthetic data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Groundwork 2026-11-18 (otgw-webui, read-only, no code). Ring sizes from graphHistoryStuff.h:41-47: without PSRAM the ring is 126 slots of 30 s = 1 h (GRAPH_HIST_SMALL_SLOTS, static array graphHistSmallSlots, graphHistoryStuff.ino:21, internal RAM), not 24 h; with PSRAM 5046 slots of 120 s = 7 d (GRAPH_HIST_LARGE_SLOTS, platformPsramAllocOnce, graphHistoryStuff.ino:29). The 24 h figure is the default response window (GRAPH_HIST_DEFAULT_WINDOW_S), not a ring size.

Option A cost (exact, slot 14 -> 16 bytes, int16 satTarget): no PSRAM 126 x 14 = 1764 B -> 126 x 16 = 2016 B, +252 B internal RAM (.bss). PSRAM 5046 x 14 = 70644 B -> 5046 x 16 = 80736 B, +10092 B PSRAM (of 8 MB). Response (estimate, not measured): one more JSON column of at most GRAPH_HIST_MAX_EMIT = 1008 values at about 4-5 chars each (e.g. 205, or null,), about +4-5 KB on a full 7 d response. Touches: GraphHistSlot + static_assert (graphHistoryStuff.h:62-72), graphHistEmptySlot, the writer (graphHistoryStuff.ino:57-71, source state.sat.fTargetTemp SATtypes.h:527, NO_TEMP while SAT is off), the emitter (graphHistoryStuff.h:~293), v2.js TCOLS (:3335) and series list (:3212), GSER_LABEL_KEY (:3423), i18n en/nl/de/el, tests that build ring JSON (graph-history-merge, graph-7day-window, graph-room-dropout, test_capture_device_data.py). The ring is RAM only, so no persisted-format migration; old browsers ignore an extra column. Needs an ADR amending ADR-193/245 (slot size is in both) and the needs-maintainer-ok label.

Option C (SAT target written into roomSet while SAT is on) breaks both: (1) legend and CSV: the series is labelled thermostat setpoint (v2.js:3212, i18n graph.series_room_set) and the CSV header is roomSet (exportCsv v2.js:5884-5888), so a SAT target would be exported and shown under the thermostat's name; test graph-control-setpoint-source.test.mjs:153 pins that label to MsgID 16. (2) the column turns mode-dependent with no marker in the CSV: the slot flags only carry flame/DHW/CH (graphHistoryStuff.h:54-56), so a window that spans a SAT on/off switch mixes two quantities in one column, the exact confusion 92 hit in indigo's CSVs. (3) the thermostat's own setpoint is lost while SAT runs, which the room tile shows on purpose (room-tile-sat-target.test.mjs). (4) live model (MsgID 16, v2.js:2392) and ring would disagree unless both change. Option B (live-only from /v2/sat/status) gives no history after a page reload and adds a poll path for the graph (2-in-flight cap).

Recommendation: A. Cost is small and exact (+252 B internal RAM without PSRAM, +10 KB PSRAM, ~+4-5 KB on the largest response), the column keeps one meaning, CSV and legend stay truthful, and both setpoints stay visible. Main risk: the ring format change touches every test fixture that builds ring JSON and needs the ADR amendment plus Robert's OK; mitigate by adding the column at the end of the emitter so older readers ignore it. C is rejected for semantic overload; B only as a fallback if Robert declines the format change. Label needs-maintainer-ok to be added if A is chosen.

Maintainer decision (Robert, 2026-11-18 12:14:15 UTC, Discord DM 1572585729098383494), verbatim reply: 'A'. It answers the coordinator's question 1572584352477618247, option A verbatim: "A) Eigen kolom in de grafiekhistorie (slot 14 -> 16 B): +252 B intern RAM zonder PSRAM, +10 KB PSRAM, eerlijke lijn + CSV-kolom; vraagt een ADR die 193/245 aanvult." Label needs-maintainer-ok removed. Next: Proposed ADR amending ADR-193/245 (User Decision), then the implementation in lane wt-ba-2018.
<!-- SECTION:NOTES:END -->
