---
id: TASK-722
title: 'feat-2.0.0: SAT MQTT publish on-change + jittered 7-11min heartbeat (ADR-111)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-26 19:11'
updated_date: '2026-05-26 20:43'
labels:
  - feat-2.0.0
  - mqtt
  - sat
  - adr-impl
  - refactor
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
De OTGW32 2.0.0 firmware publiceert ~74 MQTT-topics onder sat/* elke control-cycle (default 30s) zonder change-detection, wat broker en HA-historiek onnodig belast. Deze taak voert ADR-111 uit: alle SAT-publishes gaan door een gedeeld helper-patroon dat publiceert op (a) value-change met per-veld float-tolerantie, of (b) een per-topic gejitterde heartbeat-timer die uniform random 7-11 minuten loopt. Boot-scatter spreidt de eerste heartbeat-golf over 0-11min. MQTT-reconnect reset shadows niet (retained topics herstellen via broker; niet-retained binnen 11min via heartbeat). Bron: live telnet-log otgw-1020BA16C6BC firmware 2.0.0-alpha.71 2026-05-26. Plan: ~/.claude/plans/snug-hatching-bird.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/ADR-111-sat-mqtt-publish-on-change-and-jittered-heartbeat.md bestaat met status Proposed, alle 6 sub-rules, Enforcement-block, en de tolerantie-tabel uit het plan. Vier verificatie-gates (Completeness/Evidence/Clarity/Consistency) passeren via /adr-kit:lint.
- [x] #2 ADR-111 is door de gebruiker reviewed en goedgekeurd; status geflipt naar 'Accepted, YYYY-MM-DD' (geen self-approval).
- [x] #3 src/OTGW-firmware/SATmqttPublish.h aangemaakt met struct-definities SATShadowF/I/B/S, 4 helper-prototypes, tolerantie-constanten (SAT_EPS_*), en inline satRandomHeartbeatMs() + satRandomBootScatterMs() functies (ESP32/ESP8266 conditional via #if defined(ESP32)).
- [x] #4 src/OTGW-firmware/SATmqttPublish.cpp aangemaakt met 4 helper-implementaties die de eligibility-formule uit ADR-111 sub-rule 2 toepassen: first-seen OR value-differs OR heartbeat-due. Float-vergelijking gebruikt fabsf(current - last) >= tolerance. Shadow-write gebeurt voor de sendMQTTData-call.
- [x] #5 satPublishMQTT() in SATcontrol.ino (regels ~1986-2279) gerefactord: elke sendMQTTData(F("sat/..."), ...) vervangen door corresponderende publishIfChanged* helper met file-static shadow naast huidige functie. Geen gedragsverandering anders dan het on-change + heartbeat gate.
- [x] #6 satPressureHealthPublish() in SATpressure.ino (regels ~62-72) gerefactord volgens hetzelfde patroon.
- [x] #7 satBLEPublishStateTopics() in SATble.ino (regels ~570-650) gerefactord. BLE-mutex-snapshot blijft intact; shadow-state buiten de mutex.
- [x] #8 weatherPublishMQTT() in SATweather.ino (regels ~677-724) gerefactord. Weather-poll-timer (15min) blijft ongewijzigd; helpers gaten alleen door heartbeat-check.
- [x] #9 Memory-budget: totale BSS-toename voor SAT-shadows blijft onder 1.5 KB. Verifieerbaar via python build.py --firmware map-file inspectie of nm op de elf.
- [x] #10 evaluate.py heeft nieuwe gate check_sat_publishes_use_helpers die failt als sendMQTTData(F("sat/...") of sendMQTTData(PSTR("sat/...") matcht in file anders dan SATmqttPublish.cpp. python evaluate.py --quick is groen.
- [x] #11 python build.py --firmware returnt exit 0 zonder nieuwe warnings.
- [ ] #12 Beta-build geflasht naar test-device. In telnet-log waarneembaar: significante reductie in sat/* publish-frequentie, en heartbeat-publishes verspreid binnen 7-11min venster per topic. Documenteer voor/na publish-rate in Final Summary.
- [ ] #13 Field-validation: minstens 1 beta-tester in Discord #dev-sat-mqtt bevestigt dat HA-historiek significant kalmer is en geen functionaliteit ontbreekt (geen 'unavailable' sensoren na 11min).
- [ ] #14 HA-dashboard toont nog steeds live waarden binnen acceptabele cadence (WebSocket-pad raakt SAT-publish niet, maar verifieer dat HA-dashboard geen verloren sensoren toont).
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-26: First milestone — helpers + ADR committed.
- ADR-111 written and Accepted (user-approved)
- SATmqttPublish.h: 4 typed shadow structs, 4 helper prototypes, jitter functions (esp_random/os_random), tolerance table (SAT_EPS_*)
- SATmqttPublish.cpp: 4 helper implementations, return-bool (added late to support pid_attributes JSON-blob coherence with its 4 source fields)
- python build.py --firmware: exit 0
- python evaluate.py --quick: 0 failed, 62 passed, 1 unrelated pre-existing warning
Next step: refactor satPublishMQTT() in SATcontrol.ino (~50 call-sites, batched per group).

2026-05-26: Second milestone — full refactor committed.
- satPublishMQTT() (SATcontrol.ino): ~50 publishes routed through helpers; per-veld float tolerance; 4 JSON-blobs gated via anyChanged signal + heartbeat.
- satPressureHealthPublish() (SATpressure.ino): 2 publishes refactored.
- satBLEPublishMQTT()/satBLEPublishStateTopics() (SATble.ino+MQTTstuff.ino): 6 publishes + per-MAC shadow reset on selection change.
- weatherPublishMQTT() (SATweather.ino): 13 publishes refactored (ESP32-only fields conditionally compiled).
- 11 inline manual-gated ON/OFF/1/0 binary publishes consolidated into new publishIfChangedBStr helper.
- One legitimate exception: sat/target event-driven echo in satHandlePreset (preset-clear); marked with // ADR-111 exception comment; gate respects same-line/2-line-back marker.
- evaluate.py gate check_sat_publishes_use_helpers added (forbid raw sendMQTTData(F("sat/...") outside SATmqttPublish.cpp, allows comment-marked exceptions).
- char* topic overloads added to helpers (publishIfChangedF/I/B/S) for runtime-built topics (sat/ble/<mac>/temp etc).
- Build green: ESP8266 RAM 91.0% (74588B; ~104B delta from no-refactor baseline incl. ~1KB shadow allocation), Flash 82.9
<!-- SECTION:NOTES:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Build verifies (python build.py --firmware exit 0) en evaluator green (python evaluate.py --quick) inclusief nieuwe check_sat_publishes_use_helpers gate
- [ ] #2 ADR-111 status is Accepted (na expliciete user-approval, geen self-approval) voordat implementatie naar feature-dev-2.0.0-otgw32-esp32-sat-support gepushed wordt
- [ ] #3 Final Summary documenteert telnet-log evidence (publish-rate voor/na), BSS memory delta, en eventueel Discord-thread voor field-validation
- [ ] #4 Commit message format: feat(sat): on-change + jittered heartbeat publish (ADR-111, TASK-722)
- [ ] #5 Auto-push naar origin/feature-dev-2.0.0-otgw32-esp32-sat-support uitgevoerd zodra build + evaluator groen zijn (per CLAUDE.md push-policy)
<!-- DOD:END -->
