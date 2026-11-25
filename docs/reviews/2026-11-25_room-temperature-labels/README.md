# Which room temperature is which: label proposal for TASK-3035 (2026-11-25)

Author: otgw-ux-designer. For: otgw-webui (builder), the coordinator. Nothing here is a decision; no code. Mockup: `mockup.html` in this folder (self-contained, open in a browser, follows the OS light or dark theme).

## The problem, from the code (origin/dev 68973c00d)

While SAT is on, "room" means two different readings on screen:

| Surface | Shows | Source | Evidence |
|---|---|---|---|
| Home overview, room tile | SAT's room (the value SAT controls on) | `roomFromSat()`; thermostat value only in a `title` tooltip | `v2.js:3072-3077`, `home.room_sat_tip` `v2.js:745` |
| SAT page: dial caption, "Room" tile, control-loop chart "Room · Outside" | SAT's room (`d.room_temp`) | SAT status | `v2.js:7760`, `v2.js:7812`, `v2.js:7826` |
| Monitor › Graph, series "room" and its tooltip | the thermostat's own reading, OpenTherm MsgID 24 | `OTcurrentSystemState.Tr` stored in the 24 h ring | `graphHistoryStuff.ino:66`, `GSER_LABEL_KEY` `v2.js:3562`, `graph.series_room` `v2.js:346` |

So the Home tile and the Monitor graph both say "room" and can differ (the tester screenshot in TASK-3035: 20.5 against the graph). The SAT page is internally consistent. Two facts limit the fix:

1. **The tooltip is invisible on a phone.** `title` needs hover, and the thermostat value is the only clue today.
2. **The graph history holds only MsgID 24.** The ring has one `room` slot per 5 s (ADR-193 "Storage", 14-byte slot; `graphHistoryStuff.ino:66`). A second series "room (SAT)" would need a firmware change and a larger slot, so it is not a 2.0.0 web-UI item.

## Proposal

### 1. Two names, used consistently

| Term | Means | EN | NL | DE |
|---|---|---|---|---|
| thermostat room | the thermostat's reading, MsgID 24 | room (thermostat) | kamer (thermostaat) | Raum (Thermostat) |
| SAT room | the room value SAT controls on | room (SAT) | kamer (SAT) | Raum (SAT) |

**Rule: a qualifier appears only when the two can differ.** SAT off, or SAT using the thermostat's own reading: plain "Room", exactly as today. SAT on with another source (MQTT push, BLE sensor, area sensor, external): qualify. A page that is wholly about SAT (the SAT page) qualifies only through the source tag, not in every label.

### 2. Per surface

| Surface | Today | Proposed |
|---|---|---|
| Home room tile | "ROOM", 20.5, tooltip names the thermostat value | label **ROOM · SAT**; a visible second line under the value, **thermostat 20.5° · via MQTT** (the tooltip stays as extra). No second line when SAT uses the thermostat's reading |
| SAT page "Room" tile | "ROOM" + age | unchanged label; add the tile tag pattern already used by "SAT → boiler": a small tag **MQTT** (or BLE, area, external, thermostat) naming the source |
| SAT page dial caption | "target · room 25.5°" | unchanged (the page is the SAT context; the tile carries the source) |
| SAT control-loop chart | panel "Room · Outside", legend "Room" | panel and legend **Room (SAT)** |
| Monitor › Graph series, legend, tooltip | "room" | **room (thermostat)** whenever SAT is on with another source; plain "room" otherwise |
| Monitor › Graph, SAT on with another source | nothing about SAT's room | one live line above the legend: **SAT uses 20.5° (MQTT). The graph shows the thermostat's reading.** (current value only: SAT's room is not in the history) |
| Home Assistant entity names | see TASK-2973 | align there: "Room temperature (thermostat)" and "Room temperature used by SAT" |

### 3. Why this and not the alternatives

- **A (recommended for 2.0.0): relabel + a visible sub-line + one live line on the graph.** Web UI only, no new data, fixes the confusion where it is visible on touch. Cost: the graph still cannot show SAT's room over time.
- **B (2.1.0, needs otgw-dev and an ADR-193 amendment): a second graph series "room (SAT)".** Best answer to "why did the heating ignore the cold room": both lines side by side. Cost: bigger slot (RAM on boards without PSRAM, the ADR's tight spot), REST format change.
- **C (rejected): always show both values in the Home tile.** Doubles the number on a tile that should be one glance; B-style detail belongs under the number, only when it differs.
- **Colour:** do not carry the difference by colour alone (WCAG 1.4.1). In the graph, thermostat room keeps its green and gets a dash-dot line pattern when SAT's room is also drawn (option B); with option A the pattern is not needed.

### 4. Copy (EN; NL and DE through the i18n sync, the meaning of existing keys changes)

| Key | EN | NL | DE |
|---|---|---|---|
| `home.room_sat_label` (new) | ROOM · SAT | KAMER · SAT | RAUM · SAT |
| `home.room_thermostat_sub` (new) | thermostat {{temp}} · via {{source}} | thermostaat {{temp}} · via {{source}} | Thermostat {{temp}} · über {{source}} |
| `tools.legend_room_sat` (new, chart panel and legend) | Room (SAT) | Kamer (SAT) | Raum (SAT) |
| `graph.series_room` (value changes only while SAT is on with another source; plain key stays) | room | kamer | Raum |
| `graph.series_room_thermostat` (new) | room (thermostat) | kamer (thermostaat) | Raum (Thermostat) |
| `graph.sat_room_note` (new) | SAT uses {{temp}} ({{source}}). The graph shows the thermostat's reading. | SAT gebruikt {{temp}} ({{source}}). De grafiek toont de meting van de thermostaat. | SAT nutzt {{temp}} ({{source}}). Das Diagramm zeigt den Messwert des Thermostats. |
| `sat.room_source_mqtt` / `_ble` / `_area` / `_external` / `_thermostat` (new) | MQTT / BLE sensor / area sensor / external / thermostat | MQTT / BLE-sensor / ruimtesensor / extern / thermostaat | MQTT / BLE-Sensor / Raumsensor / extern / Thermostat |

The Greek pack goes through the sync workflow like the others; I have not written it.

### 5. Edge cases

- SAT on but its room is lost (`room_temp_lost`): the existing "room lost" behaviour stays; the tile shows the thermostat value with plain "ROOM" and no sub-line (nothing to compare).
- Thermostat has not reported MsgID 24 (the graph's `room_no_reading`): the sub-line reads "thermostat: no reading".
- Source unknown to the UI: use the word "external", never an invented source name.
- Simulation: the same rules; the SIMULATION badge stays.

### 6. Acceptance criteria (replaces or sharpens TASK-3035 AC1-3)

1. With SAT on and a source other than the thermostat, the Home tile reads ROOM · SAT with a visible line naming the thermostat value and the source; with SAT off or the thermostat as source, the tile is unchanged.
2. The SAT control-loop chart panel and legend read "Room (SAT)"; Monitor › Graph reads "room (thermostat)" in that case and plain "room" otherwise; one live line says which value SAT uses.
3. EN, NL and DE (and EL through the sync) agree on the terms of section 1; meaning-changed keys have re-translated NL and DE values.
4. A browser test per state: SAT off, SAT on with thermostat as source, SAT on with MQTT source, SAT room lost, no MsgID 24 reading.
5. Before and after screenshots with synthetic data, light and dark, at 1400 and 390 px (via otgw-bench-tester).

## Open questions

- **For otgw-webui / otgw-dev:** does `/api/v2/sat/status` name the room source? The code shows `external_temp_valid` (`v2.js:7737`, the "Indoor sensor" row: external against OT bus); BLE and area are not told apart there as far as I read. If there is no source field, the sub-line says "external" and the source tag is limited to MQTT/external versus thermostat. I did not verify the REST payload beyond that flag.
- **Option B (second series):** only if Robert or the coordinator wants it for 2.1.0; it needs otgw-dev and an ADR-193 amendment (otgw-architect).

## Not verified

- The mockup is a static design; no browser test, no screenshot via the bench-tester yet.
- I did not run the page against a unit; the tester's screenshot in TASK-3035 is as quoted in the task, not opened by me.
- The design project (claude.ai/design): `DesignSync` may only be used inside the `/design-sync` skill, after the maintainer's `/design-login`. The mockup therefore lives in the repo; uploading it to the project is a step for the maintainer.
