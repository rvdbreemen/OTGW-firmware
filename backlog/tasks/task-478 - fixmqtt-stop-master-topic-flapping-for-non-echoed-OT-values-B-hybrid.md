---
id: TASK-478
title: 'fix(mqtt): stop master-topic flapping for non-echoed OT values (B-hybrid)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-28 19:47'
updated_date: '2026-04-28 20:05'
labels:
  - mqtt
  - regression
  - bug
  - ot-v4.2
dependencies: []
references:
  - src/OTGW-firmware/OTGW-Core.ino
  - src/OTGW-firmware/MQTTstuff.ino
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - docs/api/MQTT.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sinds 1.4.1 zien gebruikers dat sommige OpenTherm waardes (Tr, TrSet, MaxRelModLevelSetting) flapperen tussen 0 en de echte waarde in MQTT en HA UI. Oorzaak: `is_value_valid` (`OTGW-Core.ino:1244-1246`) accepteert sinds 1.4.1 zowel `OT_WRITE_DATA` als `OT_WRITE_ACK` als geldig voor de master MQTT-topic. Voor MsgIDs waar de boiler geen zinvolle waarde echo't, levert dat een fake-zero op die continu de echte meting overschrijft.

## Aanpak (B-hybrid + volledige spec-audit)

**Phase 0: Spec audit (eerst)**
Alle ~50 OT MsgIDs categoriseren als echo / non-echo door OT v4.2 spec te lezen. Bron: `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`. Output: `docs/api/MQTT-message-id-echo-audit.md` met rij per MsgID + `bSlaveEchoesValue` boolean + spec-citaat. Conservatieve default: bij twijfel `true` (publiceer).

**Phase B: Code-wijzigingen**
1. `OTlookup_t` struct uitbreiden met `bool bSlaveEchoesValue` veld; alle MsgID-entries vullen op basis van Phase 0 audit.
2. Nieuwe helper `is_value_valid_for_master_topic` in `OTGW-Core.ino`: gedrag van v1.3.5 (alleen WRITE-DATA voor master topic, geen WRITE-ACK).
3. Call-site in publish-flow splitsen: master krijgt strict-WRITE-DATA filter, source-subtopics blijven via bestaande `is_value_valid`.
4. `publishToSourceTopic` in `MQTTstuff.ino` krijgt vroege return op `OT_MSGTYPE_WRITE_ACK && !OTlookup.bSlaveEchoesValue` (skip /boiler subtopic voor non-echo MsgIDs).
5. `docs/api/MQTT.md` paragraaf toevoegen die naar de echo-audit doc verwijst.
6. CHANGELOG entry voor 1.5.0-beta.

## Plan-bestand

`C:\Users\rvdbr\.claude\plans\the-design-package-still-elegant-globe.md` (vol detail).

## Out of scope

- Default flippen `bSeparateSources` (blijft `false`).
- Migration-script voor oude retained MQTT values (eerste write-data na flash overschrijft).
- ADR (geen architectuur-wijziging, alleen data-veld toevoegen).
- HVAC/Solar message-ID extensies buiten OT v4.2 core 0..127.
- Backport naar 1.4.1 release branch (deze fix landt in 1.5.0).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/api/MQTT-message-id-echo-audit.md bestaat met een rij per MsgID 0..127 (subset die OT v4.2 reference-doc dekt) inclusief bSlaveEchoesValue waarde en spec-citaat
- [x] #2 OTlookup_t struct heeft bSlaveEchoesValue veld; alle MsgID array-entries gevuld op basis van Phase 0 audit
- [x] #3 is_value_valid_for_master_topic helper bestaat in OTGW-Core.ino met v1.3.5 semantics (alleen WRITE-DATA voor WRITE / RW)
- [x] #4 Master-topic publish call-site gebruikt is_value_valid_for_master_topic; source-publish call-site gebruikt is_value_valid (ongewijzigd)
- [x] #5 publishToSourceTopic skipt /boiler topic voor OT_MSGTYPE_WRITE_ACK wanneer bSlaveEchoesValue=false
- [x] #6 Build groen op zowel ESP8266 als ESP32
- [ ] #7 Smoke test: MQTT topic OTGW/value/{id}/Tr toont alleen 20.06 (geen flap meer); /boiler subtopic voor Tr ontvangt geen 0.00 publicatie als bSeparateSources=true
- [ ] #8 HA Room Temperature sensor blijft stabiel op gemeten waarde (geen flapping); entity-IDs ongewijzigd, geen migratie-impact
- [ ] #9 MaxTSet (echo) blijft werken: master toont thermostat-/gateway-waarde, /boiler toont boiler-clamped waarde (regressie-vrij)
- [x] #10 docs/api/MQTT.md heeft een note die naar de echo-audit doc verwijst
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementatie afgerond op dev branch (commit 8f87bfaa). Build groen op ESP8266 + ESP32.

Files changed:
- docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md (NEW, Proposed status; structural classification per ADR-080)
- docs/api/MQTT-message-id-echo-audit.md (NEW, canonical per-MsgID classification met spec-citaten)
- docs/api/MQTT.md (uitgebreid met publish-gating sectie)
- CHANGELOG.md ([Unreleased] entry)
- src/OTGW-firmware/OTGW-Core.h: OTlookup_t struct uitgebreid met bSlaveEchoesValue veld; alle 133 OTmap[] entries bijgewerkt via Python script (default true; 14/16/23/24/37/98 op false)
- src/OTGW-firmware/OTGW-Core.ino: is_value_valid_for_master_topic helper toegevoegd; 8 call-sites in print_f88/s16/s8s8/u16/u8_alias/u8_single gewrapt met master-topic guard
- src/OTGW-firmware/MQTTstuff.ino: publishToSourceTopic vroege return op WRITE_ACK + !bSlaveEchoesValue

AC1-6+10 source-verifieerbaar. AC7-9 wachten op hardware flash + telnet/HA observatie.
<!-- SECTION:NOTES:END -->
