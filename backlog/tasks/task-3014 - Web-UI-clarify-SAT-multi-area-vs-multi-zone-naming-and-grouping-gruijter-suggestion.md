---
id: TASK-3014
title: >-
  Web UI: clarify SAT multi-area vs multi-zone naming and grouping (gruijter
  suggestion)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T16:14:00Z'
updated_date: '2026-11-26T10:18:00Z'
labels:
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1131000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573733241079468133 (2026-11-21): 'multi-area' vs 'multi-zone' is confusing, and SAT/thermo-nova 'areas' correspond to our 'zones'. Suggestions: (1) rename multi-area to 'Averaged room sensors'; (2) Web UI: 'Zone count' sits under Areas while zone timeout and headroom sit under Sensors, so make two groups side by side, 'Averaged room sensors' and 'Zones', each with a one-line explanation; (3) number areas 1-4 like zones (now 0-3). Tester request: needs Robert's OK. Labels/UI text only, setting keys and MQTT/REST names unchanged unless Robert decides otherwise.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides which of the three suggestions to take
- [x] #2 Web UI groups and labels changed as decided; settings keys, MQTT and REST unchanged; screenshots before/after
- [x] #3 Docs/help text use the same names
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: Robert agrees with gruijter's suggestion (#alpha-testing msg 1573741601946345514) and asks for a design proposal to approve before implementation. Next: a short design proposal (names, two groups side by side with one-line explanations, areas numbered 1-4, before/after mock) to Robert; no code before his OK.

2026-11-21 otgw-webui DESIGN PROPOSAL (for Robert's approval; nothing built). Labels and grouping only: setting keys (satsensorarea0-3, satareaweight0-3, satmultiarea, satmultiareacount, satzonecount, satzonetimeout, satzoneheadroom), MQTT topics and REST names stay as they are.

Today (origin/dev, SAT page): Control view group 'Areas' = Area 0-3 sensor, Enable multi-area, Area count, Area 0-3 weight AND Zone count. Technical view group 'Sensors' = Error monitoring, Zone sensor timeout, Zone headroom. Settings search shows the sub 'Multi-area'. Found while checking: NL already translates area as 'zone', so NL shows 'Aantal zones' for both Area count and Zone count (two different settings, one name); DE uses 'Bereich' for areas and 'Zone' for zones.

What the two things are (from the existing help texts in settingsHelp.h, no new claims): areas = up to 4 room sensors (DS18B20 or BLE) whose weighted average becomes SAT's room temperature; zones = up to 4 extra rooms with their own PID that can only raise the main room's demand, with a sensor timeout and a headroom.

Proposal:
(1) New names. 'Areas'/'Multi-area' becomes 'Averaged room sensors'; 'Area N' becomes 'Sensor N'; 'Zones' keeps its name.
(2) Two groups next to each other in the Control view, each with a one-line explanation:
    'Averaged room sensors': Average several room sensors (on/off), Number of sensors, Sensor 1-4, Weight sensor 1-4. Line: 'SAT uses the weighted average of up to 4 room sensors (DS18B20 or BLE) as the room temperature.'
    'Zones': Number of zones, Zone sensor timeout, Zone headroom (timeout and headroom move from Technical > Sensors to here). Line: 'Extra rooms with their own control; a zone can only raise the main room's heat demand.'
    Technical > Sensors keeps Error monitoring.
(3) Numbering 1-4 in the labels (Sensor 1-4, Weight sensor 1-4), like zones; the keys keep 0-3. The help texts in settingsHelp.h get the same names (area 0 -> sensor 1).

Mock, Control view at 1400 px:
BEFORE
  [ Areas                         ]
    Area 0 sensor     [BLE ATC_1 v]
    Area 1 sensor     [-        v]
    Area 2 sensor     [-        v]
    Area 3 sensor     [-        v]
    Enable multi-area [x]
    Area count        [2]
    Area 0 weight     [1]   Area 1 weight [1]  ...
    Zone count        [0]
  (Technical > Sensors: Error monitoring, Zone sensor timeout 300 s, Zone headroom 5 C)
AFTER
  [ Averaged room sensors              ]  [ Zones                              ]
    SAT uses the weighted average of up     Extra rooms with their own control;
    to 4 room sensors as the room temp.     a zone can only raise the main
    Average several room sensors [x]        room's heat demand.
    Number of sensors      [2]              Number of zones        [0]
    Sensor 1   [BLE ATC_1 v] Weight [1]     Zone sensor timeout    [300] s
    Sensor 2   [-        v]  Weight [1]     Zone headroom          [5] C
    Sensor 3   [-        v]  Weight [1]
    Sensor 4   [-        v]  Weight [1]
  (Technical > Sensors: Error monitoring)

Labels EN / NL / DE:
  group areas:        Averaged room sensors / Gemiddelde kamersensoren / Gemittelte Raumsensoren
  group zones (new):  Zones / Zones / Zonen
  satmultiarea:       Average several room sensors / Meerdere kamersensoren middelen / Mehrere Raumsensoren mitteln
  satmultiareacount:  Number of sensors / Aantal sensoren / Anzahl Sensoren
  satsensorareaN:     Sensor 1-4 / Sensor 1-4 / Sensor 1-4
  satareaweightN:     Weight sensor 1-4 / Weging sensor 1-4 / Gewichtung Sensor 1-4
  satzonecount:       Number of zones / Aantal zones / Anzahl Zonen
  satzonetimeout:     Zone sensor timeout / Time-out zonesensor / Timeout Zonensensor (unchanged)
  satzoneheadroom:    Zone headroom / Zonemarge / Zonen-Reserve (unchanged)
  line A:  SAT uses the weighted average of up to 4 room sensors (DS18B20 or BLE) as the room temperature. / SAT gebruikt het gewogen gemiddelde van maximaal 4 kamersensoren (DS18B20 of BLE) als kamertemperatuur. / SAT nutzt den gewichteten Mittelwert von bis zu 4 Raumsensoren (DS18B20 oder BLE) als Raumtemperatur.
  line B:  Extra rooms with their own control; a zone can only raise the main room's heat demand. / Extra kamers met een eigen regeling; een zone kan de warmtevraag van de hoofdkamer alleen verhogen. / Zusätzliche Räume mit eigener Regelung; eine Zone kann den Wärmebedarf des Hauptraums nur erhöhen.
  EL follows the same pattern in the build.

Cost when approved: v2.js (labels, SAT_L2/L3 groups, two group explanation lines), i18n packs, settingsHelp.h (+ its gz) help texts, a browser test that checks the groups and labels, screenshots before/after, one build + bench. About half a day, low risk (no behaviour change).

Open point for otgw-dev, not part of this task: settingsHelp.h says Zone headroom is 'unused with 4 zones or fewer' while Zone count allows 1 to 4, so as written the headroom never applies. Either the help text or the range is off; worth a check before it gets a prominent place in the Zones group.

2026-11-21 Robert approved the proposal as written (coordinator). Built on lane/ba-3014: SET_META labels (Sensor 1-4, Weight sensor 1-4, Average several room sensors, Number of sensors, Number of zones; sub 'Averaged room sensors'), SAT_L2 groups 'Averaged room sensors' (on/off, count, sensor+weight pairs 1-4) and 'Zones' (count, timeout, headroom) with a one-line explanation each (new .set-group-desc), Technical 'Sensors' keeps Error monitoring; settings.group.areas -> averaged_room_sensors, multi_area removed, zones and group_desc added; NL/DE/EL labels (NL no longer has 'Aantal zones' twice); settingsHelp.h texts area N -> sensor N+1 and settingsHelpGz.h regenerated (test_progmem_assets_deflated 4 passed). Keys, MQTT and REST unchanged. Test tests/webui/sat-areas-zones-groups.test.mjs: RED on origin/dev 0/2 (group 'Areas'; NL 'Aantal zones' twice), GREEN 2/2; sat-area-sensor-mapping, v2-settings-sat-followups, setting-hints-two-lines, settings-help 29/0, i18n-lang 6/0, lang-menu 7/0, i18n-no-hardcoded 3/0; evaluate --quick 0 failed. Screenshots before/after light+dark: OTGW-build-artifacts-safe/evidence-TASK-3014/shots/. Note on 'side by side': the SAT settings use a CSS multi-column flow (columns:300px), so the two groups are adjacent in the flow; whether they render next to or under each other depends on the width and the other groups (in the 1400 px screenshot with only these groups they stack). A fixed two-up row would leave that layout. AC3 (manuals ch04/ch05 EN and h04/h05 NL still say area/multi-area) goes to otgw-docs (deferred to 25 Nov).

2026-11-26 otgw-webui check against origin/dev 14539282e: AC2 holds. Code ddf4b3a22 is in tags v2.0.0-beta.418, .428, .432 (CHANGELOG beta.415 line); test sat-areas-zones-groups red on old, green on new, screenshots in OTGW-build-artifacts-safe/evidence-TASK-3014/shots/ (note of 2026-11-21). AC3 open on one point: EN manual ch05 5.14 uses 'Averaged room sensors' and 'Zones' (81f1e5dc0), but NL manual h05 line 471 says 'Gemiddelde ruimtesensoren' while the NL web UI pack (i18n/nl.json:958) says 'Gemiddelde kamersensoren'. h05 line 42 and 469/487 still use 'Multi-area'. Not closed; needs the one NL word fixed (otgw-docs or me with a docs slot).

2026-11-26 AC3 verified on origin/dev 9f3b8af60 (otgw-docs): NL h05 uses 'Gemiddelde kamersensoren', 'Meerdere kamersensoren middelen' (lines 42, 469, 487), group names match nl.json; EN ch05 5.14 matches since 81f1e5dc0. Residual wording in h05 line 471: 'temperatuurinvoerzones' and 'area-temperaturen' (old terms, not group names).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Robert agreed 2026-11-21 (all three suggestions, design approved as written). AC2: ddf4b3a22, in beta.418/.428/.432; tests/webui/sat-areas-zones-groups.test.mjs red on old (0/2), green on new (2/2); settings-help 29/0, i18n gates green, evaluate --quick 0 failed; before/after screenshots light+dark in OTGW-build-artifacts-safe/evidence-TASK-3014/shots/; keys, MQTT, REST unchanged. Not verified: groups render side by side only when the multi-column flow allows (stacked in the 1400 px shot). AC3: EN ch05 (81f1e5dc0) and NL h05 (9f3b8af60) use the UI group names; checked by grep on origin/dev 9f3b8af60.
<!-- SECTION:FINAL_SUMMARY:END -->
