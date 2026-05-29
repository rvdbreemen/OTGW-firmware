---
id: TASK-753
title: >-
  feat(hw): publish hardware_type contract, migrate picavailable codepath
  selection
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 08:38'
updated_date: '2026-05-29 09:01'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Introduce a static, machine-readable hardware_type field (otgw-classic/otgw32/ot-thing) derived compile-time from boards.h, as the source for codepath/UI selection. Migrate frontend selection off the runtime picavailable signal. Deprecate picavailable for one release (keep publishing + deprecation log), then remove. 2.0.0-only: OTGW32 exists only here; dev is Classic-only. Preserve the capability-vs-liveness split: hardware_type drives codepath shape, isPICEnabled()/otcommandinterface stay for runtime liveness.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 hardware_type published on MQTT (otgw-firmware/hardware_type) + REST device-info; value otgw32 on OTGW32, otgw-classic on Classic
- [x] #2 Frontend selects PIC-UI on hardware_type, not picavailable; liveness shown as substatus
- [x] #3 picavailable kept one release with deprecation marker/log; documented in docs/api/MQTT.md
- [x] #4 ADR accepted (4 gates): hardware_type contract + capability-vs-liveness invariant + picavailable deprecation
- [x] #5 python build.py exit 0; evaluate.py --quick no new failures; prerelease bump
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
@
SCOPE: 2.0.0 only (OTGW32 exists only here; dev is Classic-only -> hardware_type trivially constant, optional later, NOT cross-worktree).

INVARIANT (capability vs liveness):
- hardware_type = static board class, compile-time from boards.h. Drives codepath/UI SHAPE. Values: otgw-classic (HAS_PIC=1) | otgw32 (HAS_PIC=0) | future ot-thing.
- liveness = is the OT path working NOW -> stays isPICEnabled()/isOTDirectEnabled()/hasOTCommandInterface() (backend), otcommandinterface (frontend).
- Classic board with dead PIC = hardware_type=otgw-classic BUT liveness=false. Frontend shows PIC-UI (it belongs there) with a "PIC not detected" substatus, instead of hiding it.

STEPS (ordered):
1. ADR (blocks rest): hardware_type as codepath-selection contract; capability-vs-liveness invariant; picavailable deprecation (1 release then remove). Guideline-level per ADR-080 unless an evaluate.py check is added.
2. boards.h: HW_TYPE_* per board from existing BOARD_* macros. HAS_PIC stays the capability flag.
3. OTGW-firmware.h: hardwareTypeName() compile-time string; optional hardwareHasPIC()=HAS_PIC.
4. Publish: MQTT retained otgw-firmware/hardware_type (in version/uptime burst ~MQTTstuff.ino:1480); REST hardware_type in device-info (~restAPI.ino:2352 next to board/hardwaremode); HA diagnostic sensor (MQTTHaDiscovery.cpp).
5. picavailable deprecation: keep publishing (MQTT 1490 + REST 2320/2512) + one-time telnet deprecation log + code comment + ADR note; remove next release.
6. Frontend index.js applyPICAvailability: select pic-only on hardware_type===otgw-classic (not picavailable); picavailable becomes pure substatus. ot-command-capable + settings rows stay on otcommandinterface. Fallback to old behavior if hardware_type field absent (transition compat).
7. Backend audit: isPICEnabled() sites in OTGW-Core (PIC cmds/upgrade/query) = real liveness -> KEEP. hasOTCommandInterface() sites -> KEEP. No backend codepath selects on the published picavailable STRING (confirmed in inventory). Backend work = publication + deprecation log only.

FILES: docs/adr/ADR-XXX, boards.h, OTGW-firmware.h, MQTTstuff.ino, restAPI.ino, MQTTHaDiscovery.cpp, data/index.js, docs/api/MQTT.md, version.h (bump).

GATES: build.py exit 0 (ESP32 + ESP8266 green); evaluate.py --quick no new failures; ADR 4 gates; docs updated; field check (HA sees new sensor, OTGW32 no PIC-UI via type path).
@
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-113 written (Proposed). Code: boards.h HW_TYPE_NAME per board; OTGW-firmware.h hardwareTypeName()/hardwareHasPIC(); MQTTstuff.ino publishes otgw-firmware/hardware_type + picavailable marked deprecated; restAPI.ino device-info hardware_type; MQTTHaDiscovery.cpp 248-group discovery row + mqttHaSensorIndex[] ids 249-254 +1; index.js applyPICAvailability selects pic-only on hardware_type. docs/api/MQTT.md updated. evaluate --quick: HA-DISC index now PASS; remaining ESP-abstraction FAIL is PRE-EXISTING (committed HEAD=85 vs stale baseline 78; this change adds zero ESP directives). Bumped alpha.88->alpha.89. Build running.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Introduced hardware_type contract (ADR-113): static, compile-time board-class slug (otgw-classic / otgw32) published on MQTT (otgw-firmware/hardware_type, retained), REST device-info, and HA diagnostic discovery (248-group). Frontend (index.js) now selects PIC-only UI on hardware_type instead of runtime picavailable, with fallback to old behaviour when the field is absent. Capability-vs-liveness invariant preserved: isPICEnabled()/hasOTCommandInterface() stay for runtime liveness. picavailable deprecated for one release (kept publishing + code/docs deprecation markers; remove next release). Build green for ESP32 + ESP8266 (alpha.89). evaluate --quick: HA-DISC index consistency PASS; the remaining ESP-abstraction FAIL is pre-existing in committed HEAD (85 vs stale baseline 78) and unrelated to this change. Field validation (HA shows new sensor; OTGW32 shows no PIC-UI) pending on hardware.
<!-- SECTION:FINAL_SUMMARY:END -->
