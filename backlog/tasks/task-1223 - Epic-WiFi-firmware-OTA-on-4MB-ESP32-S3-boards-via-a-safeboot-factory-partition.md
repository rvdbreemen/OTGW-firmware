---
id: TASK-1223
title: >-
  Epic: WiFi firmware OTA on 4MB ESP32-S3 boards via a safeboot factory
  partition
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-13 19:45'
updated_date: '2026-10-15 04:11'
labels:
  - epic
  - esp32
  - ota
milestone: m-2
dependencies:
  - TASK-1222
priority: medium
ordinal: 343000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature tree for app/firmware OTA over WiFi on the 4MB boards (esp32 / esp32-classic / esp32-combo; LOLIN S3 Mini FH4R2, OTGW32 v1). Today ADR-166 makes app OTA USB-only because the 4MB table has a single app slot.

Approach (prior art: Tasmota safeboot, MycilaSafeBoot; see TASK-1222 notes): a small updater app in an app/factory partition (~640K) plus one large ota_0 and a smaller LittleFS. The main firmware hands over by erasing otadata and restarting. The bootloader then boots the factory updater, which writes ota_0 and switches back. An invalid ota_0 also falls back to the updater, which gives a recovery path.

Out of scope: 8MB/N8R8 boards. They get native A/B dual-slot OTA through the existing hasSpareAppOtaSlot() path and need their own OPI/8MB target (separate task).

Gate: TASK-1222 (spike) must end in GO before any child task starts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All child tasks are Done, or explicitly dropped with a reason
- [ ] #2 A 4MB bench unit takes a firmware update over WiFi end to end (main -> safeboot -> new main) with settings and WiFi preserved, shown by a bench transcript
- [ ] #3 An interrupted or corrupt update leaves the unit recoverable over WiFi through safeboot, without USB, shown by a bench transcript
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-14 spike TASK-1222 outcome: mechanism proven on the bench. The 640K factory + 1.125 MiB FS figures in 1223.2/1223.6 are WRONG under current assets (FS needs >=1460 KiB). Viable only with gzipped assets: factory 960K / ota_0 2368K / FS 640K. Subtasks 1223.2 and 1223.6 must be re-specified once the maintainer picks the asset lever (gzip vs dropping a UI). Also: single-slot means a failed upload leaves the unit in safeboot (not gatewaying) until a good image is uploaded.

CORRECTION 2026-10-14 (supersedes the 528 KiB / FS 640K / ota_0 2368K figures): the earlier gzip estimate also compressed .hex/.json/.log/.ini, which the firmware reads itself (PIC hex for flashing, ot-thermo.json, simulation log). With browser-only gzip (.js/.css/.html; 504,911 B content) the mklittlefs minimum is **604 KiB**. Revised conditional-GO layout: safeboot factory 0x10000/0xF0000 (960K, 946,327 B updater, ~36 KB slack) + ota_0 0x100000/0x230000 (2240K, combo headroom 270,464 B) + LittleFS 0x330000/0xC0000 (768K = 604 measured + 164 runtime/COW) + coredump 0x3F0000. Verdict unchanged: NO-GO with plain assets, conditional GO with browser-asset gzip.

Maintainer decisions 2026-10-15:
(1) v2 UI only. The classic UI is removed on the 2.0.0 line. Minified staging (ADR-190) plus dropping classic is the space lever, and gzip stays rejected.
(2) Layout: factory/safeboot 960K at 0x10000, ota_0 2240K at 0x100000, LittleFS 768K at 0x330000, coredump unchanged. A build size gate is required for the updater against 960K.
(3) Safeboot auth works the same as the main firmware. Main mirrors the admin password into NVS; safeboot enforces Basic Auth when it is set and is open when it is not (ADR-056 parity).
(4) Safeboot is WiFi plus a fallback AP only. W5500 Ethernet stays out of the updater; Ethernet-only units recover through the AP.
(5) Crash-loop rollback: investigate whether the pioarduino bootloader can do APP_ROLLBACK, and ship it in the migration image if it can. Otherwise record why not.
(6) The updater is our own minimal Arduino build (spike section 4.1), not MycilaSafeBoot.
<!-- SECTION:NOTES:END -->
