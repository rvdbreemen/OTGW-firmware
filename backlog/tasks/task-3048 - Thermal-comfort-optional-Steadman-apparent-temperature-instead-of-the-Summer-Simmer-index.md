---
id: TASK-3048
title: >-
  Thermal comfort: optional Steadman apparent temperature instead of the Summer
  Simmer index
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T06:24:00Z'
updated_date: '2026-11-28T06:11:00Z'
labels:
  - sat
  - feature
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1151000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Idea from gruijter (#alpha-testing 2026-11-25, https://discord.com/channels/812969634638725140/1514720723980259460/1575033941981790219): the Summer Simmer index was built for outdoor summer heat and sees the room warmer than it feels (22 C at 70 % gives 26.9 C, +4.9 C; 20 C at 50 % gives 22.4 C, +2.4 C). He suggests Steadman's Apparent Temperature AT = T + 0.33 e - 4.00 for mild indoor climates without wind. Our check: e = rh/100 x 6.105 x exp(17.27 T/(237.7+T)) hPa; 22 C at 70 % gives e = 18.45 hPa and AT = 24.09 C (gruijter says about 23.3, which would need e of about 15.8 hPa; asked him which e he used). Both formulas on origin/dev: the SSI at SATcontrol.ino:5229-5235 equals thermo-nova summer_simmer.py and climate.py:276-277 uses it as the room temperature when thermal comfort is on. Using AT instead deviates from the reference and needs Sergeant D's decision (not decided, only an idea; Robert: all DECIDED items go to 2.0.0, this one is not decided). Related: TASK-3043 (SSI comment and rounding).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D decides whether an optional Steadman AT mode is wanted (decisions repo entry)
- [ ] #2 If yes: settings option, formula with the standard Magnus e, host test with 22 C/70 % = 24.1 C and 20 C/50 %, documented in the manual
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (otgw-webui, 2026-11-26, read first, no code before AC1). Facts, SATcontrol.ino on origin/dev 612ca2f5e: (a) Control input: with bThermalComfort on and fresh indoor humidity, satGetRoomTemp() returns satCalcSimmerIndex(otRoom, humidity), but ONLY on the thermostat (MsgID 24) path (2926-2933); BLE, areas, external and simulation return earlier (2893-2899, 2840-2870), so with those room sources the setting does not change control. thermo-nova climate.py:276-277 returns SummerSimmer.index (summer_simmer.py:8-45) for its single inside sensor. Consumers through satGetRoomTemp: PID error, tolerance band, tune, zones (14 call sites). (b) Published: sat/summer_simmer_index + perception (5967-5972). (c) bSummerSimmer (7877-7925) is a separate summer mode on outdoor temperature. (d) Surfaces: satthermalcomfort (settingStuff.ino, restAPI.ino 2336/4431, MQTT sat/thermal_comfort, v2.js 1294/4422/4538), manual ch05 r.551, h05 r.586, MQTT.md 539/1144, firmware-sat-spec 3008/3832. Design: setting satcomfortmodel (0 = Summer Simmer default unchanged, 1 = Steadman AT), read only when satthermalcomfort is on; one helper satComfortIndex(T,rh) used by the thermostat path and the published index; AT = T + 0.33 e - 4.00, e = rh/100 x 6.105 x exp(17.27 T/(237.7+T)); 1 decimal half-even; rh outside 0..100 gives T. Host test on the real function, red first: 22 C/70 % = 24.1 (e 18.46, AT 24.09), 20 C/50 % = 19.8 (e 11.66, AT 19.85), default mode 26.9 and 22.4 unchanged. UI label via t() EN/NL/DE/EL + help; manual EN/NL with the plain fact that AT can be BELOW the room temperature (AT < T when 0.33 e < 4, below about 12 hPa, e.g. 20 C under about 52 % RH); MQTT command sat/comfort_model. Behaviour: default off = no change; on = SAT control input changes and deviates from thermo-nova: AC1 Sergeant D first, no push or beta before his register entry (coordinator 2026-11-26: code in the lane is allowed, no push). Open: perception text stays SSI-only; whether Steadman should also apply on the BLE/external/areas paths (today comfort does not, so gruijter may only see the published index, not control: to ask).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Maintainer go 2026-11-26 (Robert, interactive session: 'Voer taak 3048 optioneel uit'): execute as an OPTIONAL mode. The default stays the Summer Simmer index, so there is no behaviour change unless the user switches the mode. The label needs-maintainer-ok is removed; milestone 2.1.0 stays unless the coordinator says otherwise.

Assignee: otgw-webui (SAT owner). Plan first, then implement.

AC1 (Sergeant D decides whether the optional Steadman mode is wanted) stays open as a parallel question. The implementation does not wait for it (standing rule: build the recommended option meanwhile). It stays a lane until his answer is recorded in the decisions repo: no push before AC1 is recorded, or Robert says otherwise.

Test values for the host test (AC2): 22 C at 70 % gives 24.1 C (description: e = 18.45 hPa, AT = 24.09 C). 20 C at 50 % is not stated in the description; the same formula (AT = T + 0.33 e - 4.00, e = rh/100 x 6.105 x exp(17.27 T/(237.7+T))) gives e = 11.66 hPa and AT = 19.85 C (arithmetic, checked with a script on 2026-11-26).

Read-only finding 2026-11-26 (otgw-webui), by reading, NOT run on a unit: the published sat/summer_simmer_index is computed as satCalcSimmerIndex(satGetRoomTemp(), humidity) (SATcontrol.ino:5970). With thermal comfort on, a thermostat room source and fresh humidity, satGetRoomTemp() already returns the index (2926-2933), so the published index is SSI(SSI(T)) in that case; with BLE/external/areas sources it is SSI(T) as intended. sat/room_temp (5755) then publishes the substituted value too. Control is not affected by the double application (the control uses satGetRoomTemp once). Separate low-priority task to be opened for the published value; no fix inside TASK-3048.

CORRECTION and reading (otgw-webui, 2026-11-26, after the coordinator's register check): the note above about a double application at SATcontrol.ino:5970 is NOT a bug: entry 189 (Robert, 2026-10-31, TASK-1458 closed won't fix) decided that with Thermal comfort on the published sat/summer_simmer_index and perception stay the index of the control temperature, 'the index of the index', exactly as thermo-nova (climate.py:244-245 publishes SummerSimmer.index(self.current_temperature), climate.py:276-277). The separate task I opened for it was archived. Reading of the entries for Steadman mode: entry 114 (Q32 C) says the PID uses the Summer Simmer Index instead of the room temperature, as thermo-nova climate.py:276-277: Steadman changes that control input, so it needs Sergeant D (AC1). Entry 117 (Q57 A): thermal comfort humidity is indoor only (MQTT/REST or BLE room sensor), no weather fallback; without fresh indoor humidity the raw room temperature is used: Steadman keeps exactly that gate. Entry 32: humidity plays a part only with the comfort option; 'Summer Simmer stays' there is the separate summer cutoff (bSummerSimmer), unaffected. Entry 189 is about raw vs control temperature for the published index; it does not speak to a second formula. Options for the published topics in Steadman mode: (a) sat/summer_simmer_index and perception keep following entry 189 literally = the Summer Simmer index of the control temperature (a Summer Simmer applied to the Steadman value; the perception scale then reads a number it was not made for); the Steadman control value is already visible in sat/room_temp (SATcontrol.ino:5755 publishes satGetRoomTemp(), which is the substituted value on the thermostat path), so no new topic is needed; (b) the Steadman value under the same topics: violates entry 189; (c) a new topic sat/comfort_index for the Steadman value, old topics unchanged. Recommendation: (a), with a plain sentence in the manual (published Summer Simmer index = index of the control temperature; in Steadman mode the control temperature is the apparent temperature, shown in sat/room_temp), and (c) only if testers ask for a clean AT topic. No conflict between the entries found: 189 and 114 are about the Summer Simmer mode; the Steadman mode is a new deviation decided by Sergeant D under AC1. Also noted: thermal comfort is applied only on the thermostat (MsgID 24) room path (SATcontrol.ino:2926-2933); with BLE, areas, external or simulation sources the setting does not change control, so a user on those sources sees only the published index.

Redefined for 2.1.0 (coordinator, 2026-11-28, after entries 515 and 516): the Steadman option is dropped (Sergeant D rejected it, entry 515); this task now means: remove the Thermal comfort model (stored setting migration, web UI, REST, MQTT index topics as decided in Q-3048c, queued). The option stays in 2.0.0 as it is (entry 516, Q-3048b answered B). Milestone set to 2.1.0; lane/ba-3048 does not land.
<!-- SECTION:NOTES:END -->
