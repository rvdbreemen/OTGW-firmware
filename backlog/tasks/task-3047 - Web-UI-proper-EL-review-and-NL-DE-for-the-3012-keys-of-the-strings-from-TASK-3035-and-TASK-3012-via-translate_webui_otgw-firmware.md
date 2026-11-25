---
id: TASK-3047
title: >-
  Web UI: proper EL review (and NL/DE for the 3012 keys) of the strings from
  TASK-3035 and TASK-3012 via translate_webui_otgw-firmware
status: To Do
assignee: []
created_date: '2026-11-25T05:56:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1148000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-3035 and TASK-3012 added or changed strings in the language packs. EN, and NL/DE for the TASK-3035 copy, follow otgw-ux-designer's README (docs/reviews/2026-11-25_room-temperature-labels/README.md). The EL strings of both tasks, and the NL/DE strings of the two TASK-3012 keys (tools.sat_big_idle_anti_cycling, tools.sat_sub_stopped_takeover), were written by otgw-webui directly, NOT through the translate_webui_otgw-firmware skill (no glossary, no adversarial reviews, no editor pass): EL unreviewed. Keys: graph.series_room_thermostat, graph.sat_room_note, home.room_sat_label, home.room_thermostat_sub, home.room_thermostat_none, home.room_sat_tip (now with {{src}}), tools.legend_room_sat, tools.sat_room_src_other, tools.sat_big_idle_anti_cycling, tools.sat_sub_stopped_takeover; graph.series_room is back to its old value. Do it after the beta that carries TASK-3035/3012 (coordinator, 2026-11-25).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The EL strings of the keys above went through translate_webui_otgw-firmware (glossary, two adversarial reviews, editor pass) and the changes are committed
- [ ] #2 The NL and DE strings of tools.sat_big_idle_anti_cycling and tools.sat_sub_stopped_takeover went through the same review
- [ ] #3 tests/webui/i18n-*.test.mjs and python evaluate.py --quick show no new FAIL
<!-- AC:END -->
