---
id: TASK-523
title: Wipe and republish HA discovery topics on OTA upgrade (per-device)
status: To Do
assignee: []
created_date: '2026-05-03 09:13'
labels:
  - feature
  - ha-discovery
  - ota
  - 1.5.x
dependencies:
  - TASK-522
references:
  - docs/c4/c4-component-integration-layer.md
  - docs/api/MQTT.md
  - docs/adr/ADR-040-mqtt-source-specific-topics.md
  - docs/adr/ADR-077-streaming-mqtt-ha-discovery.md
  - docs/adr/ADR-089-heap-tier-machine.md
  - docs/adr/ADR-090-mqtt-autoconfig-reentry-guard.md
  - 'C:\Users\rvdbr\.claude\plans\zie-de-analyse-van-harmonic-melody.md'
  - 'src/OTGW-firmware/MQTTstuff.ino:1547'
  - 'src/OTGW-firmware/MQTTHaDiscovery.cpp:2285'
  - 'src/OTGW-firmware/OTGW-ModUpdateServer-impl.h:301'
  - 'src/OTGW-firmware/OTGW-firmware.ino:186'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
**Probleem:** HA caches MQTT-discovery configs in `core.entity_registry`. Wanneer de firmware nieuwe templates publiceert die qua `unique_id` of `name` afwijken van eerdere versies, blijven oude entities zichtbaar als schaduw-entries naast de nieuwe. Een gebruiker rapporteerde twee identieke `OTGW_Room_Temperature` entities in HA met dezelfde waarde door precies dit mechanisme.

**Doel:** bij elke OTA-upgrade (firmware of filesystem) eenmalig alle HA discovery topics voor dit specifieke device wissen op de broker, en daarna fris opnieuw publiceren via de bestaande drip-mechaniek. Werkt per `nodeId` (= `sUniqueid`) zodat andere OTGW devices op dezelfde broker onaangeroerd blijven.

**Waarom afhankelijk van TASK-522 (disambiguation):** als de huidige firmware-templates zelf nog dubbele namen produceren (zie TASK-522), lost een wipe alleen het symptoom op (oude entries weg) maar niet de oorzaak (nieuwe entries blijven verwarrend). TASK-522 fixt het ontwerp; deze wipe-task ruimt vervolgens orphan entries op die uit de oude ontwerpkeuze achterbleven.

**Aanpak:**
1. Setting `bool bWipeHaDiscoveryOnBoot = true` (default) in `MQTTSettingsSection` (`MQTTstuff.h:41-60`).
2. OTA-finish hook (`OTGW-ModUpdateServer-impl.h:331` + ESP32-pad) zet flag op true en persisteert via `writeSettings(false)` voor reboot.
3. Boot-hook in `OTGW-firmware.ino:setup()` na `startMQTT()` checkt flag, voert `wipeHaDiscoveryTopicsForNode()` synchroon uit (~500 publishes met `feedWatchDog()` per iter, RAII guard via `MQTTAutoConfigSessionLock` voorkomt re-entry), persisteert flag-clear, triggert `markAllMQTTConfigPending()` voor async drip-republish.
4. Default `true` op struct-init zorgt dat bestaande gebruikers zonder de key in settings.ini de wipe ook eenmalig krijgen → ingebouwde forced migration zonder release notes.

**Te wissen topic-matrix per `<nodeId>`:**
- `<haPrefix>/sensor/<nodeId>/<label>[/<source>]/config` (306 base + 3 source-variants per ANY_SOURCE-flagged)
- `<haPrefix>/binary_sensor/<nodeId>/<label>/config` (53)
- `<haPrefix>/climate/<nodeId>/{climate,dhw_control}/config` (2)
- `<haPrefix>/number/<nodeId>/Toutside_override/config` (1)
- `<haPrefix>/switch/<nodeId>/<obj_id>/config` (14 SAT switches)
- `<haPrefix>/select/<nodeId>/sat_heating_system/config` (1)

Source-variants worden ALTIJD gewist ongeacht huidige `bSeparateSources`-waarde, want oude firmware kan ze gepubliceerd hebben.

**Plan-file:** `C:\Users\rvdbr\.claude\plans\zie-de-analyse-van-harmonic-melody.md` bevat volledige design + crash-recovery analyse + verificatie-testplan.

**Reference:** sessie-analyse over duplicaat HA entities; ADR-040 source-specific topics; ADR-077 streaming MQTT discovery; ADR-089 heap tier machine; ADR-090 RAII session-lock.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Een succesvolle firmware-OTA OF filesystem-OTA zet MQTTwipeHaOnBoot=true in settings.ini vóór reboot (zowel ESP8266 als ESP32 OTA-pad)
- [ ] #2 Eerste boot na upgrade verwijdert alle retained <haPrefix>/{sensor,binary_sensor,climate,number,switch,select}/<nodeId>/.../config topics uitsluitend voor dit sUniqueid; andere devices op dezelfde broker blijven onaangeroerd
- [ ] #3 Direct na de wipe wordt drip-republish geactiveerd via markAllMQTTConfigPending(); binnen ~15 minuten zijn alle topics opnieuw beschikbaar en bevat HA UI geen dubbele entities meer
- [ ] #4 De wipe gebeurt maximaal 1x per upgrade: MQTTwipeHaOnBoot=false na succesvolle wipe gepersisteerd; subsequent boots zonder nieuwe OTA doen geen wipe
- [ ] #5 Default-gedrag: als de key ontbreekt in settings.ini (bestaande gebruikers) wordt de wipe op eerste boot na upgrade alsnog uitgevoerd door C++ struct-default true
- [ ] #6 python evaluate.py --quick passes; verificatie-testplan uit plan-file end-to-end uitgevoerd
<!-- AC:END -->
