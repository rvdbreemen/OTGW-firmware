---
id: TASK-532
title: Port-forward gateway-source bare-topic fix from dev (feature-2.0.0)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-03 19:41'
updated_date: '2026-05-03 19:42'
labels:
  - mqtt
  - ha-discovery
  - backward-compat
  - 2.0.0
  - bug
  - port-forward
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTHaDiscovery.cpp:2291'
  - 'src/OTGW-firmware/MQTTstuff.ino:1425'
  - 'docs/api/MQTT.md:424'
  - 'docs/api/MQTT.md:473'
  - docs/adr/ADR-040-mqtt-source-specific-topics.md
  - docs/adr/ADR-067-ha-discovery-state-reconciliation-on-ota-upgrade.md
  - >-
    docs/adr/ADR-068-bseparatesources-mutually-exclusive-base-and-source-variants.md
  - 'https://github.com/rvdbreemen/OTGW-firmware/commit/7c7b64fe'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Probleem

Sister task van **TASK-531** op `dev`. Dezelfde bug bestaat op deze branch in `MQTTHaDiscovery.cpp:2291` (het bestand werd hernoemd van `mqtt_configuratie.cpp` naar `MQTTHaDiscovery.cpp` als onderdeel van de streaming-discovery split op deze branch).

Commit `2b12834c` (2026-04-17) introduceerde een `_gateway` suffix op de gateway-source variant van source-templated MQTT topics. Dat verbreekt backward compatibility met bestaande Home Assistant setups die op de historische bare entity-namen luisterden:

- **Pre-refactor**: `<nodeId>-roomtemperature` op topic `<topic>/roomtemperature`.
- **Post-refactor (huidig, fout)**: `<nodeId>-roomtemperature_gateway` op `<topic>/roomtemperature/gateway`.
- **Vereist**: thermostat en boiler houden hun suffixes; **gateway gebruikt bare names** zodat met `bSeparateSources=true` (ADR-068) de gateway-variant exact de plek van de historische base-entity inneemt.

## Verschil met dev (TASK-531)

Het bestand is hernoemd. Op dev: `mqtt_configuratie.cpp:2297`. Op feature-2.0.0: `MQTTHaDiscovery.cpp:2291`. Inhoud identiek: `static const char src_suffix_gateway[] PROGMEM = "_gateway";`.

De downstream `hasSrc` gate logica is identiek aan dev en gates `uniq_id`, friendly-name en `stat_t` segment append-sites. Een lege suffix produceert automatisch de bare/historische vorm.

## Sister-task referentie

Zie **TASK-531** op `dev` voor de volledige analyse, ADR-040/067/068 context, en de code-level deductieve verificatie van de resulterende topic-shape voor MsgID 24.

## Master plan

`~/.claude/plans/1-het-moet-op-happy-mochi.md`
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Edit `src/OTGW-firmware/MQTTHaDiscovery.cpp:2291`: `src_suffix_gateway` van `"_gateway"` naar `""`
- [x] #2 Update `docs/api/MQTT.md` regels 424 en 473 (zelfde wijzigingen als TASK-531 op dev): gateway-variant gebruikt bare entity-naam
- [ ] #3 Build clean op ESP8266 én ESP32-S3: `python build.py --firmware` exit 0
- [x] #4 Geen regressie: `python evaluate.py --quick` toont geen nieuwe failures
- [x] #5 Code-level deductieve verificatie: drie discovery-configs voor MsgID 24 — bare gateway + suffixed thermostat + suffixed boiler. Topic shapes en uniq_ids matchen TASK-531 verificatie tabel
- [ ] #6 Verifieer ADR-067 wipe-on-OTA cleant pre-fix retained `_gateway` discovery-configs op upgrade simulation (deferred to hardware test indien nodig)
- [ ] #7 HA continuïteit smoke check op ESP32-S3 device (deferred to hardware test indien nodig)
- [ ] #8 Commit met beschrijvende titel (geen TASK-ID), vraag toestemming voor push naar `origin/feature-dev-2.0.0-otgw32-esp32-sat-support` (feature-branch push vereist per-instance bevestiging)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

Direct port-forward van TASK-531's fix. Master plan goedgekeurd via ExitPlanMode op 2026-05-03; deze branch krijgt dezelfde één-regel change op de hernoemde file `MQTTHaDiscovery.cpp` (line 2291 ipv 2297).

### Stappen op `feature-dev-2.0.0-otgw32-esp32-sat-support`

1. Edit `src/OTGW-firmware/MQTTHaDiscovery.cpp:2291` — `src_suffix_gateway` van `"_gateway"` → `""`. Logica identiek aan dev: `hasSrc` gate produceert automatisch bare/historische vorm voor de gateway-variant.
2. Update `docs/api/MQTT.md` regels 424 en 473 — zelfde inhoud als TASK-531 op dev (gateway uses bare entity name, backward compat note).
3. `python build.py --firmware` (background) — verify exit 0 voor ESP8266 én ESP32-S3 build targets.
4. `python evaluate.py --quick` — verify geen nieuwe failures.
5. Commit met titel "fix(mqtt-ha): gateway-source variant uses bare topic for backward compat (port-forward to feature-2.0.0)" — beschrijvende, geen task-ID.
6. **Vraag toestemming voor push** naar `origin/feature-dev-2.0.0-otgw32-esp32-sat-support` — feature-branch push vereist per-instance bevestiging per project-regels.
7. ACs afvinken, final summary schrijven, status → Done.

### Verificatie

Code-level deductieve verificatie (AC #5) is identiek aan TASK-531 — zelfde `hasSrc` gate logica, zelfde verwachte topic-shapes. Hardware-niveau ACs (#6, #7) deferred tijdens normale beta-test cyclus zoals bij TASK-531.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC #1 done: `src_suffix_gateway` empty string at `MQTTHaDiscovery.cpp:2291`.
AC #2 done: `MQTT.md` regels 983 en 1147 bijgewerkt op feature (andere regelnummers dan dev wegens bredere doc-structuur op deze branch). Beide vermelden gateway-as-bare met backward-compat rationale.
AC #4 done: `python evaluate.py --quick` toont 59 passed / 2 warnings / 0 failed / 97.1% health — geen regressie.
AC #5 done: code-level deductieve verificatie. `expandAndStreamSensorSources()` op feature branch heeft dezelfde `hasSrc` gate logica als dev (`MQTTHaDiscovery.cpp:1872` of equivalent). Drie discovery-configs voor MsgID 24 produceren bare gateway + suffixed thermostat + suffixed boiler, identiek aan TASK-531 verificatie tabel.
<!-- SECTION:NOTES:END -->
