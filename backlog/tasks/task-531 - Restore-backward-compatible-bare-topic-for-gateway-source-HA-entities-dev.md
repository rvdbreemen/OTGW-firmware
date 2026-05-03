---
id: TASK-531
title: Restore backward-compatible bare topic for gateway-source HA entities (dev)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-03 19:30'
updated_date: '2026-05-03 19:34'
labels:
  - mqtt
  - ha-discovery
  - backward-compat
  - 1.5.x
  - bug
dependencies: []
references:
  - 'src/OTGW-firmware/mqtt_configuratie.cpp:2297'
  - 'src/OTGW-firmware/mqtt_configuratie.cpp:1878'
  - 'src/OTGW-firmware/MQTTstuff.ino:1425'
  - 'docs/api/MQTT.md:424'
  - 'docs/api/MQTT.md:473'
  - docs/adr/ADR-040-mqtt-source-specific-topics.md
  - docs/adr/ADR-067-ha-discovery-state-reconciliation-on-ota-upgrade.md
  - >-
    docs/adr/ADR-068-bseparatesources-mutually-exclusive-base-and-source-variants.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Probleem

Commit `2b12834c` (2026-04-17, "refactor: replace legacy MQTT HA discovery with compact streaming API") introduceerde als bijwerking een `_gateway` suffix op de gateway-source variant van source-templated MQTT topics. Dat verbreekt backward compatibility met bestaande Home Assistant setups die op de historische bare entity-namen luisterden:

- **Pre-refactor**: gateway-published values gebruikten de bare/historische entity-naam — `<nodeId>-roomtemperature`, topic `<topic>/roomtemperature`.
- **Post-refactor (huidig, fout)**: gateway krijgt `_gateway` suffix — `<nodeId>-roomtemperature_gateway`, topic `<topic>/roomtemperature/gateway`. Breekt dashboards, automations, MQTT-subscribers die de legacy naam volgden.
- **Vereist**: thermostat en boiler houden hun suffixes; **gateway gebruikt bare names**. Met `bSeparateSources=true` (ADR-068) onderdrukt de firmware de standalone base-entity, en de gateway-variant neemt diens plek in — exacte HA-continuïteit.

## Waarom één regel volstaat

`mqtt_configuratie.cpp:1878` heeft de gate `hasSrc = (ctx.sourceSuffix && ctx.sourceSuffix[0] != '\0')`. Die wordt op drie plekken geconsulteerd:
- `:1897` — `uniq_id` suffix append
- `:1908-1911` — friendly-name suffix append
- `:1926-1929` — `stat_t` segment append

Een lege suffix laat alle drie de plekken automatisch de bare/historische vorm uitvoeren. Geen branching, geen verdere code-wijziging.

## Wat NIET wijzigt

- `kSrcSeg[3] = {"thermostat", "boiler", "gateway"}` op `MQTTstuff.ino:1425` is de wipe-on-OTA helper voor ADR-067. Het bevat topic *segments* (concrete pad-elementen), geen suffixes — per ADR-040 zijn dat verschillende concepten. Ongewijzigd laten zorgt dat ADR-067 nog steeds de pre-fix retained `_gateway` discovery-configs van de broker veegt bij upgrade.
- `src_name_gateway = "Gateway"` en `src_seg_gateway = "gateway"` blijven staan. De `hasSrc` gate consulteert de suffix; lege suffix onderdrukt automatisch ook name en segment in de gepubliceerde JSON.

## Geen nieuwe ADR nodig

ADR-040 (source separation, Accepted/amended), ADR-067 (wipe-on-OTA, Accepted), ADR-068 (bSeparateSources mutual exclusivity, Accepted) beschrijven gedrag in abstracte termen en hardcoderen geen suffix-waardes. Deze fix is implementatie-niveau binnen het bestaande ADR-kader.

## Sister task

Task B op `feature-dev-2.0.0-otgw32-esp32-sat-support` doet dezelfde fix op het hernoemde bestand `MQTTHaDiscovery.cpp:2291`. Onafhankelijk uitvoerbaar.

## Master plan

Zie `~/.claude/plans/1-het-moet-op-happy-mochi.md` voor de volledige analyse, branch-tabel, en verificatie-stappen.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Edit `src/OTGW-firmware/mqtt_configuratie.cpp:2297`: `src_suffix_gateway` van `"_gateway"` naar `""`
- [x] #2 Update `docs/api/MQTT.md` regels 424 en 473: gateway-variant gebruikt bare entity-naam, geen `_gateway` suffix
- [x] #3 Beslis ADR-040 amendment-note vs nieuwe kleine ADR; documenteer keuze in Implementation Notes
- [ ] #4 Build clean: `python build.py --firmware` exit 0
- [x] #5 Geen regressie: `python evaluate.py --quick` toont geen nieuwe failures
- [ ] #6 Verifieer met `bSeparateSources=true` op een device of via simulatie: drie discovery-configs voor MsgID 24 — bare `<node>-roomtemperature` (gateway) + `<node>-roomtemperature_thermostat` + `<node>-roomtemperature_boiler` met overeenkomstige `stat_t` topics
- [ ] #7 Verifieer ADR-067 wipe-on-OTA cleant pre-fix retained `_gateway` discovery-configs van de broker bij upgrade simulation
- [ ] #8 HA continuïteit smoke check: bestaande automation die `sensor.<host>_room_temperature` gebruikt blijft werken zonder manual remap
- [ ] #9 Commit met beschrijvende titel (geen TASK-ID), push naar origin/dev (standing permission)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

Master plan goedgekeurd via ExitPlanMode op 2026-05-03. Zie `~/.claude/plans/1-het-moet-op-happy-mochi.md` voor volledige analyse.

### Stappen op `dev`

1. Edit `src/OTGW-firmware/mqtt_configuratie.cpp:2297` — `src_suffix_gateway` van `"_gateway"` → `""`. Eén-regel change. De `hasSrc` gate op regel 1878 zorgt automatisch dat `uniq_id`-suffix (1897), friendly-name suffix (1908-1911), en `stat_t` segment (1926-1929) alle drie de bare/historische vorm krijgen voor de gateway-variant.
2. Update `docs/api/MQTT.md`:
   - Regel 424: `_gateway` voorbeeld weg of expliciet "gateway uses bare name (backward compat)"
   - Regel 473: tabel-rij voor `%source_suffix%` bijwerken naar "_thermostat, _boiler (gateway uses empty suffix)"
3. ADR-040 status-block: korte amendment-note toevoegen (geen body-edit, mag niet) verwijzend naar deze fix + commit hash. Alternatief: nieuwe kleine ADR. Beslissing tijdens implementatie.
4. `python build.py --firmware` (background) — verify exit 0
5. `python evaluate.py --quick` — verify geen nieuwe failures
6. Commit met titel "fix(mqtt-ha): gateway-source variant uses bare topic for backward compat" (geen TASK-ID per project memory)
7. Push naar `origin/dev` (standing permission applies)
8. AC's afvinken, final summary schrijven, status → Done

### Verificatie

Build-niveau: AC #4, #5 dekken dit. Hardware/HA-niveau (AC #6, #7, #8) vereisen real-device test of mosquitto_sub simulation; documenteer in final summary welke verificaties op build-niveau gedaan zijn versus deferred-to-hardware.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC #1 done: `src_suffix_gateway` set to empty string at `mqtt_configuratie.cpp:2297`.
AC #2 done: `MQTT.md` regels 424 en 473 bijgewerkt — gateway-variant gedocumenteerd als bare/historische naam (backward compat).
AC #3 decision: **geen ADR amendment**. ADR-040's suffix-lijst op regel 67 is illustratief (voorbeelden van suffix-waardes), niet normatief. Lege string is een geldige suffix-waarde binnen het bestaande ADR-040 framework. Bovendien: project-regels verbieden body-edits op Accepted ADRs, en een nieuwe ADR voor één-regel implementatie-bugfix is overkill.
AC #5 done: `python evaluate.py --quick` toont 31 passed / 2 warnings / 1 pre-existing fail (2 unresolved ADR refs out of 1003 — bestond al, niet door mijn wijziging veroorzaakt). Geen regressie.
<!-- SECTION:NOTES:END -->
