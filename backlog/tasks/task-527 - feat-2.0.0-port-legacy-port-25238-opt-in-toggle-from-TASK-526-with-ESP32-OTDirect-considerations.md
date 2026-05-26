---
id: TASK-527
title: >-
  feat-2.0.0: port legacy port 25238 opt-in toggle from TASK-526 (with ESP32 +
  OTDirect considerations)
status: Done
assignee:
  - '@codex'
created_date: '2026-05-03 10:49'
updated_date: '2026-05-26 16:04'
labels:
  - feature
  - ui
  - settings
  - network
  - feature-2.0.0
  - port-forward
dependencies: []
references:
  - docs/c4/c4-component-network.md
  - docs/c4/c4-component-integration-layer.md
  - other-projects/OT-Thing-OTGW32/
  - other-projects/pyotgw-master/
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/networkStuff.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
**Doel:** port de legacy-port-25238 opt-in toggle uit TASK-526 (dev) naar feature-dev-2.0.0-otgw32-esp32-sat-support, met de extra overwegingen die op feature-2.0.0 spelen.

**Achtergrond:** TASK-526 introduceert op dev een UI-toggle waarmee gebruikers de TCP-server op port 25238 (otmonitor protocol, gelezen door pyotgw en de HA OpenTherm Gateway Python integratie) bewust aan- of uitzetten. Default is `false` (opt-in). Onder feature-2.0.0 moet dezelfde feature landen, maar met aandacht voor branch-specifieke verschillen.

**Verschillen op feature-2.0.0 die mogelijk ander werk vereisen:**

- **OTDirect-pad (no-PIC, ESP32-only).** Op OTGW32-hardware draait de OT-decode in software via `OTDirect.ino`. De port-25238 listener bedient ook deze raw-frame-stream wanneer geen PIC aanwezig is. Verifieer dat het uitschakelen van port 25238 niet ook andere debug-/monitor-paden raakt die alleen op feature-2.0.0 bestaan.
- **ESP32 + Ethernet.** Feature-2.0.0 ondersteunt OTGW32 met optionele Ethernet-interface. De port-25238 listener moet correct stopppen/starten op zowel WiFi als Ethernet network-interfaces. Test beide.
- **BLE / SAT subsystem.** Het SAT subsysteem leest BLE-temperaturen en interageert met OT-bus, niet met port 25238 direct, maar bij integratie-tests met de HA Python component die SAT-states vraagt over port 25238 zijn er afhankelijkheden om te verifiëren.
- **Settings struct location.** Feature-2.0.0 kan een verder geëvolueerde settings-structuur hebben (mogelijk in een eigen `NetworkSettingsSection` ipv `MQTTSettingsSection`). Pas naam en sectie aan op de bestaande conventie van die branch.
- **Web-UI assets verschillen.** De `data/index.html` en `data/index.js` op feature-2.0.0 hebben SAT-specifieke UI-elementen die op dev niet bestaan. Plaats de toggle in een passende sectie (waarschijnlijk Network of Settings → Advanced).

**Aanpak:** wacht tot TASK-526 op dev gemerged is. Cherry-pick de implementation commit en pas waar nodig aan voor:
1. ESP32 / OTGW32 hardware-paden (OTDirect interactie)
2. Ethernet-interface ondersteuning
3. UI-plaatsing in de feature-2.0.0 versie van de Settings-pagina
4. Settings-sectie keuze (`MQTTSettingsSection` vs nieuwe `NetworkSettingsSection`)

**Dependencies:**
- TASK-526 (dev): basis implementatie die forward geport wordt
- Kan niet starten voordat TASK-526 op dev gelanded is

**Test scope op feature-2.0.0:**
- ESP8266 D1 Mini build met PIC: gedrag identiek aan dev
- ESP32-S3 (OTGW32) build zonder PIC: port 25238 listener bedient OTDirect-decode-stream; uitschakelen mag de OT-decoding niet stilleggen
- Ethernet variant: listener stop/start moet werken op de Ethernet-interface
- Met HA OpenTherm Gateway Python integratie verbonden via Ethernet/WiFi: re-enabling de listener herstelt connectiviteit binnen 30s

**Synergie met TASK-524:** beide tasks zijn port-forward-tracking voor dev → feature-2.0.0. Coördineer in dezelfde merge-cyclus om diff-overhead te beperken.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 TASK-526 implementation 1-op-1 geport naar feature-2.0.0 met aanpassing voor ESP32 / OTDirect / Ethernet paden waar relevant
- [x] #2 Setting-naam en JSON-key consistent met dev-versie (bLegacyPort25238Enabled / LegacyPort25238Enabled) zodat een merge dev → feature-2.0.0 zonder semantische conflict werkt
- [x] #3 UI-toggle landed op de feature-2.0.0 versie van Settings-pagina, op een logische plek (Network of Advanced sectie)
- [x] #4 Op OTGW32-hardware (ESP32-S3, geen PIC): port 25238 uitschakelen verstoort de OTDirect OT-decoding niet
- [x] #5 Op Ethernet-variant: listener stopt/start netjes op de Ethernet-interface, niet alleen WiFi
- [x] #6 python build.py succeeds voor zowel esp8266:esp8266:d1_mini als de ESP32-S3 OTGW32 fqbn
- [x] #7 Release-notes voor de feature-2.0.0 versie waarin dit landt verwijzen naar dev's release-notes voor de migratie-guidance, plus eventuele hardware-specifieke notes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Open the feature-2.0.0 worktree and inspect the port-25238 lifecycle, settings structs, REST settings API, and WebUI layout on that branch.
2. Port the TASK-526 setting name and JSON key consistently: bLegacyPort25238Enabled / LegacyPort25238Enabled.
3. Gate the legacy TCP listener behind the setting with runtime stop/start, accounting for ESP32, OTDirect, and Ethernet differences present on the branch.
4. Add/adjust WebUI labels/help and release notes for 2.0.0.
5. Run the required 2.0.0 validation build with firmware and filesystem together; then update ACs, notes, final summary, and prerelease version for this coherent change set.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Ported the legacy TCP port 25238 setting into the 2.0.0 worktree.
- Added runtime start/stop gating for the TCP listener and kept OTDirect decode loop independent from the TCP bridge.
- Added WebUI labels/help, REST settings exposure, default filesystem setting, release notes, and bumped prerelease from alpha to alpha.1.

- Validation: python3 evaluate.py --quick --no-color passed with 59 passed, 0 failed, 2 warnings.
- Validation: python3 build.py --target all --no-color passed for ESP8266 and ESP32-S3, including firmware and LittleFS images for both targets.
- Build produced esp8266 and esp32 .ino.bin/.littlefs.bin artifacts plus merged-full binaries. Build completed with an existing helper warning: flash_otgw.bat could not be copied to build/ because write_text(newline=...) is unsupported on this Python version; distribution zips were still created.
- Hardware-specific ACs were verified by code-path/build review: OTDirect decode loop remains unconditional while only the TCP bridge is gated; SimpleTelnet listener start/stop is shared for WiFi/Ethernet.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the TASK-526 legacy TCP port 25238 opt-in behavior to the feature-2.0.0 worktree. The setting remains compatible with dev via bLegacyPort25238Enabled / LegacyPort25238Enabled, is exposed through REST/WebUI/default settings.ini, and defaults to disabled.

Changes:
- Added runtime start/stop gating for the SimpleTelnet listener on port 25238.
- Gated PIC ser2net input/output and OTDirect TCP bridge I/O behind the setting while leaving loopOTDirect() and OT frame decoding untouched.
- Added WebUI label/help text, known-settings whitelist entry, persisted settings support, and release-note migration guidance for pyotgw/otmonitor/custom TCP clients.
- Bumped the 2.0.0 prerelease from alpha to alpha.1; build.py synchronized version headers/assets.

Validation:
- python3 evaluate.py --quick --no-color: 59 passed, 0 failed, 2 warnings.
- python3 build.py --target all --no-color: passed for ESP8266 D1 Mini and ESP32-S3, building both firmware and LittleFS images for both targets.

Notes:
- No live hardware was connected; OTDirect and Ethernet acceptance were verified by branch code path and successful ESP32 build. The combined build emitted a non-fatal helper warning copying flash_otgw.bat to build/, while artifacts and distribution zips were created successfully.
<!-- SECTION:FINAL_SUMMARY:END -->
