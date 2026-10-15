---
id: TASK-1222
title: >-
  Spike: measure whether a safeboot factory-partition updater makes firmware OTA
  possible on 4MB ESP32-S3 boards
status: Done
assignee:
  - '@claude'
created_date: '2026-10-13 16:34'
updated_date: '2026-10-15 03:49'
labels:
  - spike
  - esp32
  - ota
dependencies: []
references:
  - docs/research/2026-10-14-spike-1222-safeboot-ota-4mb.md
priority: medium
ordinal: 342000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Question: can 4MB boards (esp32, esp32-classic, esp32-combo) get WiFi firmware OTA through a small updater app in a second app slot, instead of waiting for an 8MB module (ADR-166)?

Idea (option B, not considered in ADR-166): partition layout app0 (main, ~2.1 MiB) + app1/ota_1 (mini-updater, ~0.5 MiB) + LittleFS (~1.1-1.25 MiB). Main firmware sets boot partition to the updater and reboots; the updater joins WiFi with the shared NVS credentials, serves one upload page, writes ota_0 (never the running partition, so the TASK-959 brick cause is gone), verifies, switches back. With bootloader app rollback, a broken new app falls back to the updater (recovery path).

Facts measured 2026-10-13 (alpha.420): usable flash for apps+FS = 0x3E0000 (4,063,232 B). App images: combo 2,023,296 B, esp32 1,975,760 B, classic 1,919,840 B. Two equal app slots leave 16 KB for LittleFS (option A stays rejected). data/ dir is 1,193,312 B; CSV comment claims ~0.95 MB of web assets in the image.

Unknowns (estimates only, unverified): Arduino-ESP32 v3 WiFi+WebServer sketch ~0.8-1 MB (too big); lean IDF-native updater (esp_wifi + esp_http_server + esp_ota_ops) ~0.6-0.7 MB.

Scope: SPIKE ONLY. Measure, decide, report. No production partition change, no firmware change, no device flash of a new layout. Throwaway code lives outside src/OTGW-firmware (e.g. a scratch dir or spike branch) and is not merged.

Known costs if it goes ahead (for the go/no-go): one-time USB flash for every field unit (partition table + LittleFS offset change, settings reset unless backed up); main app ceiling ~2.1 MiB (~100 KB headroom for combo); rollback needs CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE in a custom bootloader, which cannot be updated OTA; new ADR amending/superseding ADR-166.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Minimal updater app (WiFi STA from NVS creds, one HTTP upload page, esp_ota write + verify + set boot partition) builds for ESP32-S3 and its .bin size is recorded with the build command used
- [x] #2 Actual LittleFS space needed is measured: size of the built littlefs image content (files actually shipped) plus runtime files (settings, logs), with the minimum safe partition size stated
- [x] #3 A concrete candidate 4MB partition layout (offsets + sizes, aligned) is written down and shown to fit current combo app + headroom + updater + measured LittleFS, or shown not to fit
- [x] #4 Spike report in the task notes gives a go/no-go with the numbers, the main-app headroom left, and the migration cost for existing field units
- [x] #5 MycilaSafeBoot (MIT, 630,576 B on esp32-s3-devkitc-1) is built for our S3 targets and evaluated as a drop-in factory updater (size, NVS WiFi-creds reuse, no-HTTPS fit) against writing our own; choice recorded with reasons
- [x] #6 No changes to src/OTGW-firmware or the production partition CSVs; the OTGW32 bench (COM4) may be flashed temporarily with the candidate layout for proof, and is restored to its original layout + settings afterwards (maintainer-approved 2026-10-13)
- [x] #7 On the OTGW32 bench with the candidate layout: main firmware boots and mounts LittleFS; otadata erase hands over to safeboot, which joins WiFi with stored creds; a WiFi upload boots the new image; a truncated upload is rejected; a corrupted ota_0 falls back to safeboot (transcripts)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Scratch workspace outside src/ (.tmp/spike-1222, gitignored): clone MycilaSafeBoot v3.3.9, build it for esp32-s3-devkitc-1 with our pioarduino platform 55.03.35 -> record .bin size (AC1, AC7).
2. Minimal own Arduino updater (WiFi.begin() from NVS creds + WebServer upload + Update to ota_0 + set boot partition) -> record size, compare with Mycila (AC2, AC7).
3. LittleFS need: list exactly what build.py packs into littlefs.bin + read runtime files/free space from bench device (.88.61) -> minimum safe FS size (AC3).
4. Candidate 4MB CSV (expected safeboot factory 0x10000 0xA0000, ota_0 0xB0000, LittleFS ~1.125 MiB, coredump) -> gen_esp32part check + fit table vs combo/esp32/classic sizes (AC4).
5. On-device proof on OTGW32 bench (COM4) [needs AC6 amendment]: back up /api/v2/settings; flash bootloader + candidate table + safeboot + current esp32 app + littlefs at new offsets; check main boots and finds LittleFS; erase otadata (esptool) -> safeboot boots, joins WiFi with stored NVS creds; upload app over WiFi with curl -> boots into ota_0; upload truncated image -> rejected, safeboot stays up; corrupt ota_0 -> bootloader falls back to safeboot. Then restore the original layout + settings with flash_otgw.bat.
6. Report go/no-go in notes with numbers + transcripts (AC5); final summary with evidence per AC.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Prior-art research (2026-10-13, desk research, nothing built yet)

Finding: the pattern is solved and shipped. The usual design is a small updater in an app/FACTORY partition, not ota_1, plus one large ota_0. The bootloader falls back to the factory app when otadata is blank or ota_0 is invalid. That gives a recovery path for free.

### 1. Tasmota "safeboot" (arendst/Tasmota, v12+, in production at scale)
- 4MB table `partitions/esp32_partition_app2880k_fs320k.csv` (read from repo):
  `safeboot app/factory 0x10000 0xD0000 (832K)`, `app0 app/ota_0 0xE0000 0x2D0000 (2880K)`, `spiffs 0x3B0000 0x50000 (320K)`.
- Handover (support_esp32.ino `EspPrepRestartToSafeBoot`): ERASE the first 2 sectors of otadata, then restart. With no valid otadata the bootloader boots the factory app. The way back is `esp_ota_set_boot_partition(esp_ota_get_next_update_partition())`. Single-OTA detection: `esp_ota_get_app_partition_count() == 1`.
- Upload UI (xdrv_01_9_webserver.ino `HandleSwitchBootPartition`, `u4=fct|ota`): when running ota_0 with a single OTA slot, the upload form first restarts into safeboot, then uploads. There is also an OTA-URL pull.
- Built as a separate PlatformIO env with `board_build.app_partition_name = safeboot`, `-DFIRMWARE_SAFEBOOT`, and lib_ignore / custom_component_remove to trim.
- Released binary sizes (tasmota/install firmware/release): `tasmota32s3-safeboot.bin` 794,496 B, esp32 800,432 B, c2 624,848 B. This build includes TLS, MQTT and the Tasmota web UI.
- Docs: safeboot does not save settings and does not support initial WiFi setup.

### 2. MycilaSafeBoot (mathieucarbou, MIT, v3.3.9 2026-09-12, active)
- Same ecosystem as ours: Arduino / pioarduino, by the ESPAsyncWebServer maintainer. Explicitly "borrowed from Tasmota".
- 4MB example: `safeboot app/factory 0x10000 0xA0000 (640K)`, `app ota_0 0xB0000 0x330000`.
- Released ESP32-S3 binaries: `safeboot-esp32-s3-devkitc-1.bin` 630,576 B; `waveshare_esp32s3_eth` 647,392 B (with Ethernet). Partition limit is 655,360 B, described as "a tight fit" on Ethernet boards. Logging off saves ~12 KB; mDNS off saves ~24 KB.
- Delivery: web upload page and ArduinoOTA/espota. Without saved creds it falls back to an AP named "SafeBoot". It reuses WiFi/ETH settings only when the app stored them via MycilaESPConnect, so we would need our own NVS creds hook.
- Switch: `esp_ota_set_boot_partition(factory)` + `esp_restart()`. Also boots when the app is missing.
- No rollback for a valid-but-crashing app is described.

### 3. warmcat/lws-esp32-factory (user-suggested)
- Same concept: a "<1MB" factory app plus a 2.9MB OTA slot. Config in shared NVS, and a GPIO14 button at boot forces factory mode.
- Uses libwebsockets + HTTP/2 + self-signed TLS, so it conflicts with our no-HTTPS rule.
- STALE: last push 2018-11-05, ESP-IDF pinned to Sept 2018, ESP32 only (no S3). Useful as a concept reference, not reusable.

### 4. ESP-IDF forum (esp32.com, community + Espressif staff)
- On 4MB, factory + 2 full OTA slots does not fit. The suggested direction is a small recovery image plus a large slot, with the bootloader falling back.
- Caveat raised: a tiny factory app means `idf.py flash` dev workflows and "factory reset" leave a device that only updates.

### Fit on our 4MB (computed from measured alpha.420 sizes; usable app+FS = 4,063,232 B)
| Factory | LittleFS | Main slot | combo 2,023,296 B headroom |
|---|---|---|---|
| 640K (Mycila) | 1.25 MiB | 2.0 MiB (2,097,152) | 73,856 B (3.5%) too tight |
| 640K (Mycila) | 1.125 MiB | 0x220000 (2,228,224) | 204,928 B |
| 832K (Tasmota) | 1.0 MiB | 0x210000 (2,162,688) | 139,392 B |
Conclusion pending the AC3 LittleFS measurement: 640K factory + ~1.125 MiB FS looks like the sweet spot.

### Impact on this spike
- Prefer the factory subtype over the "ota_1 updater" from the original description. It gives the bootloader fallback without a custom bootloader, though crash-loop rollback still needs CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE.
- Candidate shortcut: build MycilaSafeBoot as-is for the S3 (630 KB proven) instead of writing an IDF-native updater. Add a hook that reads our NVS WiFi creds.
- The Tasmota otadata-erase handover avoids needing the factory partition handle and works even when otadata is corrupt.

Sources: github.com/arendst/Tasmota (partitions/, tasmota/tasmota_support/support_esp32.ino, tasmota/tasmota_xdrv_driver/xdrv_01_9_webserver.ino, platformio_tasmota_env32.ini); github.com/tasmota/install firmware/release; tasmota.github.io/docs/Safeboot/; github.com/mathieucarbou/MycilaSafeBoot (README + release v3.3.9 assets); github.com/warmcat/lws-esp32-factory; esp32.com viewtopic p=89419 / p=91407.

## Spike measurements (2026-10-13, measured this session)

### Updater size (AC1/AC6)
- Own minimal Arduino updater (.tmp/spike-1222/otgw-safeboot, WiFi.begin() from NVS + WebServer + Update, no mDNS/OTA/TLS), pioarduino 55.03.35 / Arduino 3.3.5, -Os: **946,327 B** (pio checkprogsize, log otgw-sb-pioarduino.log). Command: `PLATFORMIO_CORE_DIR=D:/DevData/platformio penv/python -m platformio run -e safeboot-s3` with the pioarduino toolchain bin on PATH.
- MycilaSafeBoot v3.3.9 unmodified (incl. ESPConnect, mDNS, ArduinoOTA, logging), same pioarduino platform: **980,059 B**.
- MycilaSafeBoot published S3 asset (Tasmota platform 2025.11.30, trimmed Arduino libs): 630,576 B. **Secondary source, NOT reproduced**: building with the Tasmota platform failed twice on Windows (toolchain not on PATH / esptoolpy package error). The first attempt also OVERWROTE the shared PlatformIO core packages (framework-arduinoespressif32 3.3.5 -> 3.3.4 tasmota, toolchain stub 15.1). Restored by `build.bat --target esp32` (SUCCESS; framework 3.3.5, toolchain 14.2.0 confirmed).
- Takeaway: the ~630 KB figure depends on the Tasmota platform. On our platform an updater is ~0.95 MB.
- IDF-native updater (AC2): not built. It cannot change the verdict below; see the conclusion.

### LittleFS need (AC3)
- Shipped assets: 43 files, 1,378,195 B raw. mklittlefs bisect (`-b 4096 -p 256`, 4 KiB steps) gives a **minimum image size of 1460 KiB**.
- Bench (OTGW32 .88.61, alpha.420 combo) /api/listfiles reports used=total=1,572,864, free 0. That is the 95.238% clamp, so real usage is >= 95%. TODAY's 1.5 MiB FS has only ~75 KiB for settings.ini, logs and LittleFS COW. (Separate risk, independent of OTA.)
- Runtime files on the bench: settings.ini 5,784; reboot_log 2,602; bootdetect.log 2,195; transfer.dat 887; sat_*.json <100; reboot_count 4.
- Gzipped variant (text assets gzip -9, binaries raw): 423,557 B, **mklittlefs minimum 528 KiB**.

### Fit (AC4); usable app+FS = 3968 KiB (0x10000..0x3F0000), combo app 2,023,296 B (1976 KiB)
- **Current asset policy (no gzip):** 3968 - 1460 (FS) - 1976 (app) = 532 KiB max for the factory partition, rounded down to 512K by 64K alignment for ota_0, with ZERO app headroom and no runtime FS room. Smallest updater: 616 KiB published / 924 KiB measured. **Does not fit -> NO-GO.**
- **With gzip assets (conditional GO):** factory 0xF0000 (960K, holds the measured 946,327 B updater) + ota_0 0x250000 (2368K, combo headroom 401,536 B) + LittleFS 0xA0000 (640K = 528 measured + 112 runtime/COW). Offsets: safeboot 0x10000, app0 0x100000, spiffs 0x350000, coredump 0x3F0000.
- Blocker: FSexplorer.ino:83 records a maintainer directive "no build-time gzip". Gzip is a maintainer decision, not an assumption. The other lever (dropping one of the two UIs, classic index.js 393 KB vs v2) is unmeasured.

## Bench proof (2026-10-13 23:50 - 2026-10-14 00:00), OTGW32 COM4, MAC 10:20:BA:21:B4:F8, .88.61
Transcript: .tmp/spike-1222/bench/transcript.txt + serial-0[1-5]-*.log (gitignored scratch).

**Layout used: the PROOF layout, not the AC4 candidate.** Under the current asset policy the candidate does not fit (see measurements). The proof layout sizes the factory partition for the measured 946 KB pioarduino updater and uses a small/empty LittleFS: safeboot factory 0x10000/0xF0000, ota_0 0x100000/0x200000, spiffs 0x300000/0xF0000, coredump 0x3F0000. Bootloader 0x0 and NVS 0x9000-0xE000 untouched.

Steps and results:
1. Byte-exact backup: `esptool read-flash 0 0x400000`, sha256 15208f6f4d4dade7...; original table parsed OK.
2. Flashed table + updater (946,736 B) + combo alpha.420+dddc423 (2,023,296 B) + empty LittleFS; otadata erased. All "Hash of data verified".
3. **Blank otadata -> factory boot**: safeboot started, `[safeboot] WiFi KeepOut2 ip 192.168.88.61`. It reused the main firmware's NVS WiFi creds with no reprovisioning and kept the same IP. ota_0 valid, so the boot partition was set to app0.
4. **Main firmware runs from the relocated slot**: POST /reboot -> device/info fwversion 2.0.0-alpha.420+dddc423, board Combo. LittleFS mounted at the new offset (listfiles totalBytes 983,040). No rebuild needed for the offset change.
5. **Handover**: otadata erase (via esptool, standing in for the future main-firmware endpoint; the Tasmota EspPrepRestartToSafeBoot pattern) + reset -> safeboot up.
6. **WiFi OTA**: curl upload of a distinguishable image (the original app extracted from the backup, alpha.420+db3c020, 2,036,928 B, esptool image-info hash valid): HTTP 200 in 11.0 s, then device/info fwversion **+db3c020** (changed from +dddc423).
7. **Truncated upload** (1,000,000 B): HTTP 500 "Could Not Activate The Firmware" (`esp_image: Checksum failed`); safeboot stays up.
8. **Broken ota_0 fallback**: the failed upload had already overwritten part of ota_0. POST /reboot -> serial `E (378) boot: OTA app partition slot 0 is not bootable`, then `[safeboot] start`. The bootloader fell back to factory by itself.
9. **Interrupted upload + hard reset** (curl killed at 6 s at 100 kB/s, then esptool RTS reset as a power-cut stand-in): device back in safeboot, WiFi up. Caveat: ota_0 was already invalid from step 7, so "interrupt on top of a valid image" was not separately shown. Steps 7/8 show any partial write invalidates ota_0 and leads to fallback.
10. **Recovery over WiFi**: good image upload HTTP 200 in 10.4 s -> main firmware +db3c020 boots.
11. **Restore**: `write-flash 0x0 bench-backup-full.bin`, hash verified. fwversion +db3c020, LittleFS back to 1,572,864. Settings diff: 1 of 182 keys (satenabled true->false). Explained: before the test SAT ran in the MQTT-lost fallback (SATcontrol.ino:4223 sets settings.sat.bEnabled=true in RAM only). The backed-up settings.ini holds "SATenabled": false, so the restore is byte-exact. Side observation: the bench MQTT broker was unreachable >5 min before the test.

Findings worth carrying into the epic:
- A failed or interrupted upload DESTROYS the previous main firmware, because Update erases ota_0 as it writes. With a single app slot there is no keep-old-image rollback. The device stays recoverable over WiFi through safeboot, but it is down (no OT gateway function) until a good image is uploaded. This is a real behavioural difference from A/B OTA, and the ADR must state it.
- The main firmware needs no rebuild for ota_0 at a different offset (MMU-mapped). LittleFS is found by label.

## Spike conclusion (AC5)
- **NO-GO under the current asset policy** (plain-text assets, ~1460 KiB LittleFS): no updater fits in 4MB beside the combo app.
- **Conditional GO if web assets are stored gzipped** (FS min 528 KiB measured): factory 960K (own pioarduino updater, 946 KB) + ota_0 2368K (combo headroom ~392 KiB) + LittleFS 640K. The mechanism is proven on hardware (steps 3-10).
- The gzip decision belongs to the maintainer: FSexplorer.ino:83 records a "no build-time gzip" directive. An alternative lever (dropping one of the two web UIs) is unmeasured.
- Migration cost for field units is unchanged from the description: one USB flash (table + LittleFS offset), settings reset unless backed up; a full-flash backup/restore (as done here) is the safe pattern.
- Separate risk found (independent of OTA): today's 1.5 MiB LittleFS is >= 95% full on the bench, so only ~75 KiB is left for settings/logs/COW.

AC6 choice: own minimal updater over MycilaSafeBoot. Reasons: (1) Mycila only reuses WiFi via its own MycilaESPConnect config, so with our WiFiManager/esp_wifi NVS creds it would start an AP (src/main.cpp start_network_manager), while ours reuses them (bench step 3); (2) on our pioarduino platform Mycila is larger (980,059 B vs 946,327 B: ESPConnect + mDNS + ArduinoOTA); (3) both are plain HTTP, consistent with no-HTTPS. Keep from Mycila: set-boot-to-ota_0-on-start (verified safe: esp_ota_set_boot_partition refuses an invalid image) and the MIT design credit. AC7 evidence: git diff on src/ and both partition CSVs is empty; bench restored byte-exact (step 11).

Correction AC7 evidence: build.bat stamped version.h/version.hash (githash db3c020->dddc423, build 7261->7267; prerelease unchanged), a side effect of the build wrapper, not a code change. Reverted with git checkout; git diff on src/ + both CSVs is now empty (0 lines).

CORRECTION 2026-10-14 (supersedes the 528 KiB / FS 640K / ota_0 2368K figures): the earlier gzip estimate also compressed .hex/.json/.log/.ini, which the firmware reads itself (PIC hex for flashing, ot-thermo.json, simulation log). With browser-only gzip (.js/.css/.html; 504,911 B content) the mklittlefs minimum is **604 KiB**. Revised conditional-GO layout: safeboot factory 0x10000/0xF0000 (960K, 946,327 B updater, ~36 KB slack) + ota_0 0x100000/0x230000 (2240K, combo headroom 270,464 B) + LittleFS 0x330000/0xC0000 (768K = 604 measured + 164 runtime/COW) + coredump 0x3F0000. Verdict unchanged: NO-GO with plain assets, conditional GO with browser-asset gzip.

2026-10-15: asset lever decided by the maintainer: v2 UI only plus minified staging (TASK-1224, ADR-190), no gzip. AC2 is dropped: an IDF-native updater cannot change the verdict (even 600 KB does not change the layout math). AC8 is satisfied by the proof-layout bench run of 2026-10-14 (spike report section 5, steps 1-11). The final-layout run is TASK-1223.9. Verdict: GO.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Verdict: GO for safeboot OTA on 4MB, with v2 UI only plus a minified LittleFS.

Measured:
- Own minimal updater 946,327 B, fits a 960K factory partition (MycilaSafeBoot is 980,059 B on our platform and does not reuse our NVS creds).
- Combo app 2,023,296 B in a 2240K ota_0, leaving 270,464 B headroom.
- v2-only minified LittleFS 668 KiB in 768K, leaving about 100 KiB runtime room.

Bench proof, 2026-10-14, on a proof layout with 960K factory and an empty LittleFS:
- Handover via otadata erase works.
- Safeboot reuses the NVS WiFi creds.
- A WiFi upload takes 11 s.
- A truncated upload is rejected.
- A broken ota_0 falls back to safeboot.
- An interrupted upload recovers over WiFi.
- The bench was restored byte-exact.

Caveat: AC7 ran on the proof layout, not the final 960K/2240K/768K layout. The final-layout run is TASK-1223.9.

AC2 (IDF-native updater) was dropped because it cannot change the verdict.

Report: docs/research/2026-10-14-spike-1222-safeboot-ota-4mb.md. Maintainer decisions are recorded on TASK-1223.
<!-- SECTION:FINAL_SUMMARY:END -->

<!-- SECTION:FINAL_SUMMARY:END -->
