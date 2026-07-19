---
id: TASK-1041
title: >-
  Add capture-heap-leak.bat: browser-free capture preset for the TASK-1037 leak
  measurement
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 15:26'
updated_date: '2026-07-19 22:31'
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
- [x] #2 A run produces telnet and MQTT logs with zero REST requests from the capture tooling in the telnet log
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

2026-07-20 hardware: run tegen bench-toestel 192.168.88.68 leverde een telnet-log met nul REST GET-regels en uitsluitend logHeapStats. AC2 daarmee gedekt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds scripts/capture-heap-leak.bat, a browser-free capture preset for heap-leak measurement, plus the -SkipDebugToggles switch it needed on the shared worker.

Why: the existing capture-mqtt-debug.bat drives a headless browser at 365 REST requests/min and switches on every telnet debug toggle. A field capture made that way shows the HTTP fragmentation path rather than the leak under investigation (TASK-1037).

Changes:
- New thin preset wrapper (not a second implementation) so it inherits future fixes to the 2213-line worker: -SkipBrowserCapture, -CrashlogPollSeconds 3600, -OutputRoot logs/heap-leak, with user arguments forwarded after the preset.
- New -SkipDebugToggles switch on capture-mqtt-debug.bat.
- Run summary now records the toggle policy unconditionally, so a shared transcript always states how it was captured.
- The wrapper warns before starting that the web UI must stay closed, since one open dashboard invalidates the run.

Verified on hardware (bench device 192.168.88.68, 2026-07-20): a real run produced a telnet log with zero REST GET lines from the tooling and all three provenance lines in the summary.

Follow-up: -SkipDebugToggles turned out not to guarantee a quiet capture, since it only promises not to switch anything on. TASK-1042 adds -QuietDebugToggles for that.
<!-- SECTION:FINAL_SUMMARY:END -->
