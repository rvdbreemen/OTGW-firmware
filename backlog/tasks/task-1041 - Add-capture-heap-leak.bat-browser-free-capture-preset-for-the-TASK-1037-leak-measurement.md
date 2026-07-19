---
id: TASK-1041
title: >-
  Add capture-heap-leak.bat: browser-free capture preset for the TASK-1037 leak
  measurement
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 15:26'
updated_date: '2026-07-19 21:20'
labels: []
dependencies: []
priority: high
ordinal: 160000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The existing capture-mqtt-debug.bat cannot measure the TASK-1037 leak, because it drives a headless Edge against the web UI. In the martreides 1.6.1 capture that produced 365 REST requests/min sustained (13067 hits each on /api/v2/device/time and /api/v2/otgw/otmonitor), plus a crashlog poll every 30s that costs ~1021 ms and visibly drops free heap by ~3.4 KB per poll. That run therefore shows the HTTP fragmentation profile, not the leak ramp: maxBlock pinned at 5352 while free heap held 10-13 KB.

To see the leak we need the device idle apart from its normal MQTT and OpenTherm work, with telnet as the only extra connection.

The underlying script already has the switches needed (-SkipBrowserCapture, -CrashlogPollSeconds, -OutputRoot) and forwards all arguments verbatim from the .bat launcher. So this is a thin preset wrapper, not a second implementation: no duplication of the 2213-line worker, and it inherits every future fix to it.

Preset:
- -SkipBrowserCapture, the whole point.
- -CrashlogPollSeconds 3600 rather than -SkipCrashlogCapture, so a crash is still captured within the hour without the 30s poll perturbing the measurement.
- -OutputRoot logs/heap-leak so these runs do not mix with normal diagnostic captures.
- User arguments forwarded after the preset so -DeviceHost and friends still work and any preset value can be overridden.

The wrapper must also tell the operator what invalidates the run, since that is the failure mode that already cost us one capture: no OTGW web UI open in any browser tab for the duration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/capture-heap-leak.bat exists and forwards user arguments to capture-mqtt-debug.bat after the preset flags
- [ ] #2 A run produces telnet and MQTT logs with zero REST requests from the capture tooling in the telnet log
- [x] #3 Output lands under logs/heap-leak, separate from normal diagnostic captures
- [x] #4 The script prints, before starting, that the web UI must stay closed for the duration
- [x] #5 Referenced from TASK-1040 as the capture method for the no-mdns experiment
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented as a preset wrapper (scripts/capture-heap-leak.bat) plus one new switch on the shared worker.

Scope grew by one item beyond the original description, deliberately: the worker enables OTmsg, REST API, MQTT, MQTTGate, Sensors and NTP on every connect, so -SkipBrowserCapture alone would still have left the capture noisy. Added -SkipDebugToggles to capture-mqtt-debug.bat. Also made the run summary record the toggle policy unconditionally, so a shared transcript always states how it was captured.

Verified: PowerShell payload parses clean, new switch in help, wrapper delegates --help, bounded run against a dead host produced a transcript under logs/heap-leak with all three provenance lines (toggle policy, browser disabled, poll interval 3600s).

AC2 and AC5 remain open: AC2 needs a run against a live device to confirm zero tooling REST requests in telnet.log, AC5 needs TASK-1040 to reference this script. Committed as c7d48646.

2026-07-19 avond: eerste veldrun met capture-heap-leak.bat (martreides, transcript-20260719-210025). Script werkte end-to-end. Alle drie de provenance-regels stonden in de summary: "Debug toggle policy: leave as-is (-SkipDebugToggles)", "Browser capture: disabled (-SkipBrowserCapture)", "Crash-log poll interval: 3600s". Output landde onder logs/heap-leak. De tooling zelf genereerde geen browserlast.

AC2 blijft OPEN, en het criterium was te zwak geformuleerd. Er stonden 14500 REST-requests in de telnet-log, maar geen daarvan kwam van de tooling: het waren twee door de gebruiker geopende webpagina's (zie TASK-1037). Het criterium moet dus onderscheiden tussen tooling-REST en overige REST, anders is het niet af te vinken op een apparaat waar iemand een tabblad open heeft.

ONTWERPGAT gevonden: -SkipDebugToggles levert GEEN stille capture op. Het garandeert alleen dat het script niets aanzet. Dit apparaat had alle toggles (OTmsg, REST API, MQTT, MQTTGate, Sensors, NTP) al op [1] staan, vermoedelijk nog van de browsergedreven capture van diezelfde ochtend die ze allemaal aanzette. De wrapper erfde die toestand en de capture werd alsnog volledig verbose.

Voor een echt rustige meting is een extra optie nodig die de toggles actief UIT zet in plaats van ze met rust te laten, met herstel van de oorspronkelijke stand na afloop. Overweeg -QuietDebugToggles naast de bestaande -SkipDebugToggles, en documenteer het verschil: "raak niets aan" versus "zet stil".
<!-- SECTION:NOTES:END -->
