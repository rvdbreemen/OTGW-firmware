---
id: TASK-1896
title: >-
  SAT: a PIC-measured outside temperature (DS18B20 on the gateway, boiler
  without MsgID 27) never counts as valid, SAT stays handed back
  (outside_temp_missing)
status: To Do
assignee: []
created_date: '2026-11-13T09:55:00Z'
updated_date: '2026-11-13T10:06:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 965000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report: indigo_light (Discord 301077214374658048), #nederlandse-ondersteuning https://discord.com/channels/812969634638725140/815561033036333076/1570736532719935529 (2026-11-13 09:46 UTC, addressed to Robert), beta.218, Classic + S3 Mini, PIC 6.8, SAT on: SAT stays inactive with outside_temp_missing:true and external_outdoor_valid:false while the SAT page shows 5.6 C; outside temperature from a DS18B20 on the PIC, the boiler sends no MsgID 27; meanwhile the thermostat's CH demand passes unfiltered (that is the designed hand-back of entries 56/57: CS=0, CH=1, MM=T). Code (origin/dev d79210e23, unchanged in beta.219-221): satResolveOutsideTemp() SATcontrol.ino:2550-2569 accepts only external (MQTT/REST), boiler MsgID 27, weather, or the restored value; the MsgID 27 freshness stamp state.otBus.iToutsideBoilerMs is set only on a live boiler Read-Ack (rsptype OTGW_BOILER, type READ_ACK, not bGatewaySubstituted, not replayed) at OTGW-Core.ino:6097-6102. A gateway-made answer to the thermostat (OTGW_ANSWER_THERMOSTAT 'A' frame) or a substituted B frame never stamps it, and the PS=1 summary never stamps (ADR-181). The SAT page value comes from OTcurrentSystemState.Toutside, which those frames do set, hence 5.6 C on screen but outside_temp_missing. Inference, not verified: the PIC reports its own sensor as an A frame for MsgID 27 (or a substituted B frame); a capture is needed to see the actual frame type. Behaviour question: entry 57 lists the sources as external, weather and boiler MsgID 27; whether a gateway-measured value counts is a SAT decision (Sergeant D) and the fix scope Robert's. Workaround until then: the weather service (Settings > SAT > Weather) or an external outside value over MQTT/REST.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture from the reporter (or a bench reproduction) shows how the PIC reports the DS18B20 outside value (frame type for MsgID 27)
- [ ] #2 Sergeant D decides whether a gateway-measured outside value counts as a valid source (recorded as an entry)
- [ ] #3 If yes: host test red on the old stamp rule, green on the fix; outside_temp_missing clears with only the PIC sensor present
- [ ] #4 Bench or field confirmation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 09:56 UTC follow-up from indigo_light (https://discord.com/channels/812969634638725140/815561033036333076/1570739066360561805): 'de weersafhankelijke regeling krijgt ook geen data door als ik mijn coordinaten ingeef en slaat dus niet aan' - the weather workaround also gets no data after entering coordinates. Not analysed: needs the capture (weather fetch log lines, sat/status weather fields). Possibly a separate weather-fetch problem.

2026-11-13 weather trace (comms, read-only, origin/dev 01e465054). Provider: Open-Meteo over plain HTTP, http://api.open-meteo.com/v1/forecast?latitude=..&longitude=.. (SATweather.ino:95/101), fetched on the webhook worker task (weatherRequestFetch :475-500), connect 2 s / read 5 s (:381-382). Preconditions in weatherLoop() (:605-640): settings.sat.bWeatherEnable (:608), 5 min after boot (WEATHER_POLL_MIN_SEC 300, :28, :611), coordinates not both 0 (:477-480, :630), heap not CRITICAL (:483-489); SAT on is NOT required. Interval: settings.sat.iWeatherInterval (default 900 s), first-fetch retry 60 s doubling to the interval (:557-565). LIKELY CAUSE of the follow-up: SATweather.ino:617  (TASK-511, f12a753eb) skips every weather fetch while the OT state holds a non-zero outside temperature. The PIC's own DS18B20 value fills OTcurrentSystemState.Toutside (the 5.6 C on his SAT page) but is rejected by satResolveOutsideTemp() (only a live boiler Read-Ack stamps it, OTGW-Core.ino:6098-6102). So the weather fallback never starts and SAT has no valid source at all: the two guards use different definitions of 'the boiler provides an outside temperature'. Failure reporting: telnet lines 'Weather: skipping fetch, no coordinates configured', 'Weather: no connection to the weather server', 'Weather: HTTP <code>', 'Weather: heap critical, fetch not started'; REST GET /api/v2/sat/weather fields enabled, valid, latitude, longitude, fetch_errors (:659-682). With the :617 skip there is NO log line and fetch_errors stays 0, valid false. No existing task for this skip (TASK-511 added it, archived). Fix direction (not decided): make :617 use the same validity as satResolveOutsideTemp (iToutsideBoilerMs fresh), not the raw state value.
<!-- SECTION:NOTES:END -->
