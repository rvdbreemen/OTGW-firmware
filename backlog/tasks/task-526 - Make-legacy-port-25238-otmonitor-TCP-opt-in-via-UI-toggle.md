---
id: TASK-526
title: Make legacy port 25238 (otmonitor TCP) opt-in via UI toggle
status: To Do
assignee: []
created_date: '2026-05-03 10:48'
labels:
  - feature
  - ui
  - settings
  - network
  - 1.5.x
dependencies: []
references:
  - docs/c4/c4-component-network.md
  - docs/c4/c4-component-integration-layer.md
  - other-projects/pyotgw-master/
  - other-projects/otmonitor-6.6/
  - src/OTGW-firmware/networkStuff.ino
  - src/OTGW-firmware/OTGW-Core.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
**Doel:** maak de TCP-server op port 25238 (het otmonitor-protocol dat door legacy clients gebruikt wordt zoals de HA OpenTherm Gateway Python integratie van mvn23 en het oorspronkelijke otmonitor desktop-tool) een **bewuste opt-in** in plaats van altijd-aan. Gebruikers die alleen MQTT gebruiken hoeven port 25238 niet open te hebben staan, en gebruikers die de Python integratie wel gebruiken activeren het bewust.

**Achtergrond:**
- Port 25238 is de TCP-bridge voor het otmonitor protocol (Schelte Bron's reference). pyotgw + de HA OpenTherm Gateway component lezen rauwe OT-frames van deze poort.
- Voor MQTT-only gebruikers is port 25238 dood gewicht: open netwerksurface zonder gebruik.
- Recente issues (zie sessie-analyse rond TASK-522/523, _reuzenpanda_) leerden dat gebruikers die zowel de HA Python integratie als MQTT actief hadden, dubbele entiteiten en verwarring zagen. Een bewuste opt-in dwingt gebruikers expliciet te kiezen welk integratiepad zij gebruiken.

**Aanpak:**
- Nieuwe setting `bLegacyPort25238Enabled` (default `false`) in `OTGW-firmware.h:MQTTSettingsSection` (of een passende NetworkSettingsSection als die er is). Hungarian `b...` prefix per ADR-051.
- JSON read/write key `LegacyPort25238Enabled` toevoegen aan `settingStuff.ino` (analoog aan `MQTTseparatesources` patroon).
- UI toggle op de Settings-pagina van de webUI, sectie MQTT of Network. Label: "Legacy: enable otmonitor TCP port 25238". Help-tekst: "Required for the Home Assistant OpenTherm Gateway Python integration and other tools that read raw OT frames over TCP. Disable if you only use MQTT."
- Server-startup conditie: lokaliseer de bestaande port-25238 listener (waarschijnlijk via `OTGWstream` of vergelijkbaar in `OTGW-Core.ino` of `networkStuff.ino`). Start de listener alleen als `settings.mqtt.bLegacyPort25238Enabled` true is.
- Setting-wijziging tijdens runtime: stop/start de server netjes zonder reboot, of documenteer dat reboot vereist is. Eerste optie heeft voorkeur voor UX.
- Migratie/release notes: vermelden dat default veranderde, en dat HA OTGW Python integratie gebruikers de toggle moeten aanzetten anders verliest hun integratie connectie.

**Default-keuze gerechtvaardigd:** opt-in past bij het principe van least privilege netwerksurface. Bestaande gebruikers die de HA Python integratie gebruiken merken bij upgrade dat hun integratie wegvalt en kunnen via release notes / WebUI banner / Discord helpdesk de toggle activeren. Eénmalige migratie-pijn, langetermijn netwerk-hygiëne.

**Code-lokaties (eerste indicaties, te verifiëren bij implementatie):**
- Server-startup waarschijnlijk in `networkStuff.ino` of `OTGW-Core.ino` rondom `OTGWstream` of `25238` literal
- Settings UI templates waarschijnlijk in `data/index.html` + `data/index.js`
- REST-API setting-handler in `restAPI.ino` (analoog aan `mqttseparatesources`)

**Synergie met TASK-522/523:** TASK-522 fixte de overlappende friendly names die ontstonden bij gebruikers die zowel MQTT als Python integratie hadden. Deze opt-in toggle helpt om die situatie ÜBERHAUPT minder vaak voor te komen door gebruikers expliciet te laten kiezen welke pad zij gebruiken.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Nieuwe bool setting bLegacyPort25238Enabled in OTGW-firmware.h (struct-default false, opt-in by design)
- [ ] #2 JSON read/write voor key LegacyPort25238Enabled in settingStuff.ino, persisted in settings.ini
- [ ] #3 UI-toggle op Settings-pagina van webUI met heldere label en help-tekst die uitlegt dat HA OpenTherm Gateway Python integratie en otmonitor desktop-tool dit nodig hebben
- [ ] #4 TCP-listener op port 25238 wordt alleen gestart wanneer bLegacyPort25238Enabled=true; bij false start de listener niet
- [ ] #5 Setting-wijziging tijdens runtime stopt/start de listener netjes zonder reboot, OF de UI documenteert duidelijk dat reboot vereist is
- [ ] #6 Default false voor zowel nieuwe installs als bestaande devices (struct-default werkt via 'ontbrekende JSON-key houdt struct-default' patroon, zelfde als TASK-523 default-true sub-pattern)
- [ ] #7 python evaluate.py --quick passes en python build.py --firmware succeeds
- [ ] #8 Release-notes voor de versie waarin dit landt vermelden expliciet de gedragswijziging en de migratiestap voor HA Python integratie gebruikers
<!-- AC:END -->
