---
id: TASK-281
title: Refactor mqttha_progmem naar leesbare OTlookup_t-stijl
status: Done
assignee:
  - '@claude'
created_date: '2026-04-17 05:38'
updated_date: '2026-04-17 06:44'
labels:
  - refactor
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Huidige mqttha_progmem.cpp gebruikt onleesbare geheugenblobs met byte-offsets. Refactor naar OTlookup_t patroon: named PROGMEM strings per entry, struct met PGM_P pointers, helper accessor readMqttHaCfgEntry(). Generator script produceert leesbare output met geformateerde JSON.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Gegenereerde .cpp heeft named PROGMEM strings per entry (ha_topic_X, ha_msg_X)
- [ ] #2 JSON messages zijn geformateerd over meerdere regels
- [ ] #3 Struct gebruikt PGM_P pointers i.p.v. pool offsets
- [ ] #4 readMqttHaCfgEntry() helper volgt PROGMEM_readAnything patroon
- [ ] #5 mqttha.cfg hersteld als bron + documentatie
- [ ] #6 Oude generator bewaard als documentatie
- [ ] #7 ESP8266 build slaagt
- [ ] #8 evaluate.py --quick slaagt
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Superseded by TASK-282 (compact array + streaming constructors). The readable-strings approach was a halfway measure. Analysis showed 95% of JSON content is identical boilerplate, making full-string storage wasteful. TASK-282 reduces flash from 168KB to ~17KB with a fundamentally better architecture.
<!-- SECTION:FINAL_SUMMARY:END -->
