---
id: TASK-1293
title: >-
  Default hostname OTGW2 on the 2.0.0 line so it does not collide with a 1.x
  gateway
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T05:32:00Z'
updated_date: '2026-10-22T05:52:00Z'
labels: []
dependencies: []
ordinal: 418000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
A 2.0.0 gateway and a 1.x gateway both default to hostname OTGW. On one LAN they collide in mDNS (probe conflict renames one, e.g. OTGW-2) and in DHCP/DNS; observed on the bench 2026-10-22 (1.x at 192.168.88.68 and an OTGW32 at .61 both named OTGW, mDNS answers broke). Maintainer (Robert) asked to change the 2.0.0 default to OTGW2 (OTGW2.0 rejected: a dot is not valid inside a hostname label). Only the hostname default changes: MQTT top topic, MQTT client id prefix, unique ids and HA device identifiers stay OTGW-based. Existing installs keep their saved hostname; no migration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A fresh settings file (no settings.ini, or the shipped data/settings.ini) yields hostname OTGW2: struct default, empty-hostname fallback in readSettings, shipped data/settings.ini and safeboot NVS-empty fallback all read OTGW2
- [x] #2 MQTT top topic default, MQTT client id prefix, mqtt unique id and HA device identifiers are unchanged (still OTGW / otgw-<mac>)
- [x] #3 Existing installs keep their saved hostname: no code renames a stored OTGW to OTGW2; verified by reading the settings load path and stated in CHANGELOG
- [x] #4 Every place that hard-codes the default hostname (firmware, safeboot, data assets, build.py README text, docs/manuals/README/guides/api docs, tests and golden files, capture/test scripts) is updated to OTGW2 / otgw2.local or consciously left with a recorded reason
- [x] #5 CHANGELOG [Unreleased] entry (English, no em dashes) states the new default and that it applies to new installs and factory resets only, 1.x keeps OTGW
- [x] #6 build.bat build (esp32-combo firmware + filesystem) reports success with fresh bins, and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Approved by the caller brief (maintainer request relayed 2026-10-22); no separate plan gate.
1. Split the define: _HOSTNAME becomes "OTGW2"; new MQTT_NAME_DEFAULT "OTGW" feeds the MQTT top-topic fallback (settingStuff.ino x2) and the MQTT client-id prefix (MQTTstuff.ino), so MQTT stays unchanged.
2. Other hostname defaults: safeboot NVS-empty fallback, shipped data/settings.ini hostname, updater page safeboot-AP fallback label.
3. Leave with reason: settingStuff "OTGW-%06x" (cleared hostname, already unique per chip), startAPFallback "OTGW-XXYYZZ" (prerelease-only AP SSID, unique per MAC, not a hostname), OLED splash / sModel (product name), Accepted ADRs, archived release notes/reviews/audits/plans, json-golden captures and webui fixtures (recorded device data).
4. Docs: default-hostname sentences, otgw.local -> otgw2.local in 2.0.0 user docs (README, manuals EN/NL, MANUAL.md, guides, api docs), build.py flash-zip README text (EN + NL, AP name).
5. CHANGELOG [Unreleased] entry.
6. bin/bump-prerelease.sh, build.bat (combo firmware + fs + safeboot), evaluate.py --quick, settings-related host/webui tests, byte-search artifacts for OTGW2.
7. Commit by explicit paths; push to origin/dev if gates green and only my commit is ahead.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- _HOSTNAME was shared by three MQTT paths (top-topic fallback x2 in settingStuff.ino, client-id prefix in MQTTstuff.ino). Split: _HOSTNAME = "OTGW2", new MQTT_NAME_DEFAULT = "OTGW" for those three, so MQTT output is unchanged. MQTTstuff.h sTopTopic = "OTGW" untouched.
- Hostname defaults changed: OTGW-firmware.h struct default (via _HOSTNAME), readSettings empty fallback (via _HOSTNAME), data/settings.ini "hostname", safeboot/src/main.cpp NVS-empty fallback (x2), updateServerHtml.h safeboot-AP label fallback. WiFiManager portal SSID is <hostname>-<MAC>, so it follows automatically.
- Existing installs: readSettings parses the saved "hostname" key and only falls back to _HOSTNAME when it is empty; no migration code. OTA filesystem update rewrites settings.ini from RAM (OTGW-ModUpdateServer-esp32.h). A USB fs flash without --migrate replaces settings.ini with the shipped one, which now says OTGW2.
- Left with reason: settingStuff.ino "OTGW-%06x" (user cleared the hostname; already unique per chip, no collision); startAPFallback "OTGW-XXYYZZ" (prerelease-only fallback AP SSID, unique per MAC, not a hostname); OLED splash and Devicetypes sModel "OTGW" (product name); MQTT topics/unique id/HA identifiers; Accepted ADRs (immutable); archived release notes, reviews, audits, plans; scripts/json-golden/* and tests/webui fixtures (recorded device data / explicit inputs); MQTT_STALE_TOPICS_CLEANUP guides (audience is upgraded units that keep OTGW); dev test scripts and capture-settings.example.json (shared by 1.x and 2.0.0 reporters and bench units that keep their saved OTGW name; always overridable via --host / secrets).
- Docs: otgw.local -> otgw2.local across README, MANUAL.md, manuals EN/NL, api README/openapi, WIFI_FIRMWARE_UPDATE, smart-autotune doc, c4 docs, build.py flash-zip README (EN+NL, AP name fixed to OTGW2-<MAC>), bin/provision-wifi-ap.py message. mDNS sections (EN ch06, NL h06, MANUAL) say 1.x keeps OTGW and upgraded units keep their saved name.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Default hostname of the 2.0.0 line is now OTGW2, so a 2.0.0 and a 1.x gateway (which keeps OTGW) no longer collide in mDNS/DHCP on one LAN.

Changes:
- OTGW-firmware.h: _HOSTNAME = "OTGW2"; new MQTT_NAME_DEFAULT = "OTGW" now feeds the MQTT top-topic fallback (settingStuff.ino x2) and the MQTT client-id prefix (MQTTstuff.ino), which used _HOSTNAME before. MQTT topics, client id, unique id (otgw-<mac>) and HA device identifiers are unchanged.
- data/settings.ini ships "hostname": "OTGW2" (MQTTtoptopic stays OTGW); safeboot NVS-empty fallback and the /update page safeboot-AP label fallback are OTGW2. WiFiManager portal SSID follows the hostname (OTGW2-<MAC>).
- Existing installs keep their saved hostname: readSettings only falls back when the key is empty; no migration. OTA filesystem update rewrites settings.ini from RAM. A USB fs flash without restoring settings.ini gets OTGW2 (with all other defaults). HA device name on such a unit becomes "OpenTherm Gateway (OTGW2)".
- Docs: otgw.local -> otgw2.local in README, MANUAL.md, manuals EN/NL, api docs, c4 docs, WIFI_FIRMWARE_UPDATE, smart-autotune doc, build.py flash-zip README (EN+NL, AP name), provision-wifi-ap.py; mDNS sections state 1.x keeps OTGW and upgraded units keep their name. CHANGELOG [Unreleased] entry. Prerelease bumped to alpha.456.
- Left with reason (see notes): OTGW-%06x cleared-hostname fallback, prerelease AP-fallback SSID, product-name literals, ADRs, archives, golden/fixture data, stale-topics guides, dev test scripts.

Evidence (this session):
- AC1: build artifacts byte-searched: littlefs.bin contains '"hostname": "OTGW2"' (1) and no '"hostname": "OTGW"'; ino.bin has "OTGW2
<!-- SECTION:FINAL_SUMMARY:END -->
