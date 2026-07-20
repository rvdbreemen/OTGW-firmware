---
id: TASK-1042
title: >-
  Add -QuietDebugToggles: actively silence telnet debug toggles for leak
  measurement
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 21:20'
updated_date: '2026-07-20 20:13'
labels: []
dependencies: []
priority: medium
ordinal: 161000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up on TASK-1041. The -SkipDebugToggles switch only guarantees that the capture script does not turn toggles ON; it leaves whatever state the device already had. Field run 2026-07-19 (martreides) showed the failure mode: the device still had OTmsg, REST API, MQTT, MQTTGate, Sensors and NTP all on, left over from a browser-driven capture earlier that same day which switches them all on. The heap-leak preset therefore produced a fully verbose capture despite the switch, which is exactly the instrument perturbation the preset exists to avoid.

Needed: an option that actively turns the toggles off for the duration and restores the original state on exit, so a leak measurement is quiet regardless of what the previous session left behind. Keep -SkipDebugToggles as the 'do not touch' semantics and document the difference.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A switch exists that turns the listed debug toggles off at connect when they are on
- [x] #2 Original toggle state is restored when the capture ends, including on Ctrl+C
- [x] #3 capture-heap-leak.bat uses the silencing option instead of the leave-as-is option
- [x] #4 Run summary records which of the two policies was applied
- [x] #5 Verified on a device that starts with all toggles on: the resulting telnet log contains only periodic heap stats and no per-message OT or MQTT gate output
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Geimplementeerd in commit a3c27ade.

- Disable-AllTelnetDebugIfNeeded spiegelt de bestaande enable-functie: zelfde bannerparsing, flipt elke toggle die op [1] staat naar uit, en onthoudt de ingedrukte keys in $script:ToggledOffKeys.
- Restore-TelnetDebugToggles zet exact die keys terug. Draait in het bestaande finally-blok, voor het disposen van de telnet-writer en -client, zodat de stream nog leeft. Bewust best-effort: bij een gecrasht of herstart apparaat is de stream dood, en een mislukt herstel mag de uitkomst van de capture niet maskeren. Meldt gedeeltelijk herstel expliciet in de summary.
- Simulator-toggles blijven onaangeroerd, net als in het enable-pad.
- capture-heap-leak.bat gebruikt nu -QuietDebugToggles in plaats van -SkipDebugToggles.
- De summary onderscheidt nu drie policies: silence-and-restore, leave-as-is, enable-all.

Geverifieerd: payload parseert schoon, beide switches staan in de help, en een begrensde run tegen een dood adres schrijft "Debug toggle policy: silence all that are on, restore on exit (-QuietDebugToggles)" naar het transcript.

AC2 en AC5 blijven open: het flip-en-herstel-pad zelf vraagt een levend apparaat. AC5 (telnet-log bevat alleen periodieke heap-stats) is pas af te vinken op een toestel dat met alle toggles aan begint.

HARDWARE-VERIFICATIE 2026-07-20, bench-toestel 192.168.88.68, build 1.7.2-beta.1+ccb5014, via capture-heap-leak.bat.

Toggle-flip: "Debug toggle actions: OTmsg=off (sent '1'); REST API=already-off; MQTT=already-off; MQTTGate=already-off; Sensors=already-off; NTP=off (sent '6'); SensorSim=skipped (simulator)".
Herstel: "Debug toggles restored: 2 toggle(s) switched back on." Exact de twee die het uitzette, niets meer.
Simulator-toggle onaangeroerd, zoals ontworpen.

Stilte-bewijs (AC5): de telnet-sectie van het transcript bevat precies een regelsoort, logHeapStats. Nul processOT, nul logMQTTValue, nul MQTT gate, nul REST GET. Dat is de meting die we wilden: alleen de heap-trend, geen instrument-ruis.

Heap-basislijn op 1.7.2-beta.1: 19056 vrij, 14032 grootste blok, HEALTHY.

AC2 is hiermee gedekt voor de normale afsluiting. Herstel na Ctrl+C is niet apart uitgelokt; de code loopt via hetzelfde finally-blok, maar dat pad is niet waargenomen.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds -QuietDebugToggles (silence all, restore on exit); superseded in practice by the selective -KeepDebugToggles of TASK-1045 for leak captures, but the restore-in-either-direction machinery it introduced is what 1045 builds on. Verified on hardware 2026-07-20.
<!-- SECTION:FINAL_SUMMARY:END -->
