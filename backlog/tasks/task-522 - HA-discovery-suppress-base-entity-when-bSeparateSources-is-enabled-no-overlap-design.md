---
id: TASK-522
title: >-
  HA discovery: suppress base entity when bSeparateSources is enabled
  (no-overlap design)
status: To Do
assignee: []
created_date: '2026-05-03 09:12'
labels:
  - bug
  - ha-discovery
  - ux
  - 1.5.x
dependencies: []
references:
  - docs/c4/c4-component-integration-layer.md
  - docs/api/MQTT.md
  - docs/adr/ADR-040-mqtt-source-specific-topics.md
  - 'src/OTGW-firmware/MQTTstuff.ino:1773'
  - 'src/OTGW-firmware/MQTTHaDiscovery.cpp:2285'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
**Probleem:** wanneer `bSeparateSources` aanstaat publiceert de firmware voor een MsgID met source-template zowel de base-entity (cfg flag 0x00) ALS drie source-variants (cfg flag 0x07 expanded naar Thermostat/Boiler/Gateway). Alle vier gebruiken dezelfde friendlyName field, bijvoorbeeld `"Room_Temperature"`. HA toont de base als `OTGW_Room_Temperature` en de source-variants als `OTGW_Room_Temperature Thermostat` etc., maar in de praktijk rapporteren gebruikers twee identieke `OTGW_Room_Temperature 21.9 °C` regels naast elkaar (base + Thermostat overlap is verwarrend).

**Aanpak (optie B per gebruikersrichting):** wanneer `bSeparateSources=true`, onderdruk de publicatie van de base-entity voor elke MsgID die een corresponderende ANY_SOURCE-entry heeft in `mqttHaSensors[]`. Alleen de drie source-variants worden in die modus gepubliceerd. Wanneer `bSeparateSources=false` (default), publiceer alleen base-entities zoals nu.

Dit elimineert de redundante overlap volledig, fixt de duplicate-friendly-name UX issue, en maakt `bSeparateSources` een echte binaire "of base-set of source-set" toggle.

**Synergie met TASK (volgt) wipe-on-OTA:** gebruikers die nu `bSeparateSources=true` hebben en zowel base als source-variants gepubliceerd zien, krijgen na deze fix orphan base-entities. De wipe-on-OTA feature (afhankelijke task) zorgt dat die op de volgende firmware-upgrade automatisch worden gewist.

**Code-context:**
- `MQTTstuff.ino:1773-1790` is de huidige loop die per cfg-entry beslist over publicatie
- `MQTTHaDiscovery.cpp:585` mqttHaSensors[] tabel met 306 entries waaronder paren `(MsgID, 0x00)` en `(MsgID, 0x07)` voor source-templated MsgIDs (bv. MsgID 24 op regels 662-663)
- `expandAndStreamSensorSources` op `MQTTHaDiscovery.cpp:2285` blijft ongewijzigd

**Reference:** zie sessie-analyse over twee identieke `OTGW_Room_Temperature` entries gerapporteerd door gebruiker.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Wanneer bSeparateSources=true zijn base-entities (cfg flag 0x00) NIET gepubliceerd voor MsgIDs die een corresponderende ANY_SOURCE-entry (flag 0x07) hebben in mqttHaSensors[]
- [ ] #2 Wanneer bSeparateSources=false (default) is gedrag identiek aan 1.5.0-beta.5: base-entities gepubliceerd, source-variants niet
- [ ] #3 Source-variant publish-logica in expandAndStreamSensorSources() is ongewijzigd
- [ ] #4 MsgIDs zonder een ANY_SOURCE-gepaarde entry (bv. outside_temperature, MsgID 27) blijven hun base-entity publiceren in beide modi
- [ ] #5 python evaluate.py --quick passes; HA UI handmatig geverifieerd geen duplicate friendly names meer bij bSeparateSources=true
<!-- AC:END -->
