---
id: TASK-522
title: >-
  HA discovery: suppress base entity when bSeparateSources is enabled
  (no-overlap design)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-03 09:12'
updated_date: '2026-05-03 09:27'
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
- [x] #1 Wanneer bSeparateSources=true zijn base-entities (cfg flag 0x00) NIET gepubliceerd voor MsgIDs die een corresponderende ANY_SOURCE-entry (flag 0x07) hebben in mqttHaSensors[]
- [x] #2 Wanneer bSeparateSources=false (default) is gedrag identiek aan 1.5.0-beta.5: base-entities gepubliceerd, source-variants niet
- [x] #3 Source-variant publish-logica in expandAndStreamSensorSources() is ongewijzigd
- [x] #4 MsgIDs zonder een ANY_SOURCE-gepaarde entry (bv. outside_temperature, MsgID 27) blijven hun base-entity publiceren in beide modi
- [x] #5 python evaluate.py --quick passes; HA UI handmatig geverifieerd geen duplicate friendly names meer bij bSeparateSources=true
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Lees `MQTTstuff.ino:1773-1790` (doAutoConfigure loop) en `MQTTstuff.ino:1855-1870` (doAutoConfigureMsgid loop). Verifieer (MsgID, flag) pair-patroon in `mqttHaSensors[]` rond regel 660 (MsgID 24 voorbeeld).
2. Implementatie via runtime-bitmap: scan eenmalig alle ANY_SOURCE-flagged MsgIDs in een 256-bit bitmap (8 × uint32_t = 32 bytes). Helper `msgIdHasAnySourceEntry(id)` returnt true voor MsgIDs met een 0x07-pair.
3. Voeg in beide publish-loops (`doAutoConfigure`, `doAutoConfigureMsgid`) toe: voor flag 0x00 cfg-entries, skip als `settings.mqtt.bSeparateSources && msgIdHasAnySourceEntry(cfg.id)`.
4. Bitmap-build is lazy/idempotent via static-init flag, kost ~306 PROGMEM reads bij eerste publish-ronde.
5. Run `python evaluate.py --quick` voor PROGMEM/lint-check.
6. Manueel verifieren dat `bSeparateSources=false` gedrag identiek blijft (alle 5 ACs).
7. Commit met descriptive title (geen task-ID).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementatie via lazy-built bitmap (32 bytes static, 8 × uint32_t) van MsgIDs-met-ANY_SOURCE-pair, eenmalig opgebouwd op eerste call door scanning van `mqttHaSensors[]`. O(1) bit-test per check na build. Helper `msgIdHasAnySourceEntry()` toegevoegd in `MQTTstuff.ino` direct vóór `doAutoConfigure()`.

Skip-conditie als `else if` toegevoegd in beide publish-loops: `doAutoConfigure` (regel ~1407) en `doAutoConfigureMsgid` (regel ~1479). Plaats: tussen de bestaande ANY_SOURCE-tak en de fallback streamSensorDiscovery-tak. Behoudt setMQTTConfigDone(cfg.id) call na het if-else-blok zodat de done-bitmap correct gemarkeerd blijft.

ADR-040 (source-specific topics) blijft het uitgangspunt: bSeparateSources is nu een echte binaire toggle "of base-set of source-set", geen overlap meer. Geen aanpassing aan expandAndStreamSensorSources nodig.

Verificatie:
- python evaluate.py --quick: 31 pass, 0 nieuwe failures (1 pre-existing ADR-references fail, 2 pre-existing warnings, allemaal niet-MQTT-discovery gerelateerd)
- python build.py --firmware: succesvol, artifact `OTGW-firmware-1.5.0-beta.5+ac1cc5c.ino.bin` (0.70 MB)
- Code review: AC #1-4 gevalideerd uit diff
- AC #5 hardware-verificatie open: gebruiker dient HA UI te controleren na flash op een test-device met `bSeparateSources=true`
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix landed in commit `4c95acd8 fix(mqtt-ha): drop redundant base sensor when bSeparateSources publishes source-variants` on dev.

**Wat veranderde:** wanneer `bSeparateSources=true` worden base-entities (cfg flag 0x00) niet meer gepubliceerd voor MsgIDs die een corresponderende ANY_SOURCE-entry (flag 0x07) hebben in `mqttHaSensors[]`. Alleen de drie source-variants (`Thermostat`/`Boiler`/`Gateway`) blijven over voor die MsgIDs. Voor MsgIDs zonder source-pair (zoals `outside_temperature`, MsgID 27) blijft de base-entity ongewijzigd in beide modi. Default `bSeparateSources=false` gedrag is identiek aan 1.5.0-beta.5.

**Implementatie:** lazy-built bitmap (32 bytes static, 8 × uint32_t) in `MQTTstuff.ino` via nieuwe helper `msgIdHasAnySourceEntry()`. Eenmalig opbouwen door scanning van `mqttHaSensors[]` op eerste call, daarna O(1) bit-test. Skip-conditie als `else if`-tak toegevoegd in beide publish-loops (`doAutoConfigure` en `doAutoConfigureMsgid`) tussen de bestaande ANY_SOURCE-tak en de fallback `streamSensorDiscovery`-aanroep. `setMQTTConfigDone(cfg.id)` blijft buiten de if-chain zodat de done-bitmap consistent gemarkeerd blijft (geen drip-retry-loop voor bewust geskipte MsgIDs).

**Diff:** 23 inserts, 0 deletes in `src/OTGW-firmware/MQTTstuff.ino`.

**Synergie met TASK-523 (wipe-on-OTA):** gebruikers die op de pre-fix firmware `bSeparateSources=true` gebruikten en zowel base als source-variants in HA hadden, krijgen na deze fix orphan base-entities. De wipe-feature uit TASK-523 ruimt die op tijdens de eerstvolgende OTA-upgrade.

**Verificatie:**
- `python evaluate.py --quick`: 31 pass; 1 pre-existing ADR-references fail + 2 pre-existing warnings, geen MQTT-discovery gerelateerd, geen regressie veroorzaakt door deze change.
- `python build.py --firmware`: succesvol, artifact `OTGW-firmware-1.5.0-beta.5+ac1cc5c.ino.bin` (0.70 MB).
- AC #5 hardware-deel afgevinkt op signaal van project-eigenaar (`intussen taak 522 naar done zetten`).
<!-- SECTION:FINAL_SUMMARY:END -->
