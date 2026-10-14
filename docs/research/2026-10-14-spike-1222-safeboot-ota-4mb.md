# Spike report: WiFi firmware OTA on 4MB ESP32-S3 boards via a safeboot factory partition

- Task: TASK-1222 (spike), feeds epic TASK-1223
- Date: 2026-10-13 / 2026-10-14
- Firmware under test: 2.0.0-alpha.420 (combo), builds +dddc423 and +db3c020
- Hardware: OTGW32 bench unit, COM4, MAC 10:20:BA:21:B4:F8, IP 192.168.88.61
- Related: ADR-166 (single-app-slot OTA policy), TASK-959 (why app OTA is blocked today)

## 1. Summary

The question was whether 4MB boards (esp32, esp32-classic, esp32-combo) can get
WiFi firmware OTA without waiting for an 8MB module.

**The mechanism works on real hardware.** A small updater in an `app/factory`
partition, next to one large `ota_0` slot, gave every behaviour we need: bootloader
fallback, WiFi reuse, upload over WiFi, rejection of a bad image, and recovery
after an interrupted upload.

**It does not fit today.** With the current plain-text web assets, LittleFS needs
at least 1460 KiB. That leaves at most 532 KiB for the updater, and the smallest
updater we could build on our platform is 924 KiB.

| Asset policy | LittleFS minimum | Room for updater | Verdict |
|---|---|---|---|
| Plain assets (today) | 1460 KiB | 532 KiB max (512K aligned) | **NO-GO** |
| Browser assets gzipped (.js/.css/.html only) | 604 KiB | 960K partition fits | **Conditional GO** |

Gzip conflicts with the maintainer directive recorded at `FSexplorer.ino:83`
("no build-time gzip"). That decision belongs to the maintainer. The alternative
lever, dropping one of the two web UIs, has not been measured.

## 2. Background

The 4MB partition table has one app slot. An app OTA write therefore targets the
running partition and bricks the unit (TASK-959). Today app OTA is blocked in
`_handleUploadStart` and firmware updates are USB-only. Filesystem OTA works.

Usable flash for apps plus filesystem is 0x10000..0x3F0000 = 3968 KiB.
Measured app images (alpha.420):

| Target | App image |
|---|---|
| esp32-combo | 2,023,296 B (1976 KiB) |
| esp32 | 1,975,760 B |
| esp32-classic | 1,919,840 B |

Two equal app slots leave about 16 KB for LittleFS, so classic A/B OTA is out.

## 3. Prior art

| Project | Design | S3 size | Usable for us |
|---|---|---|---|
| Tasmota safeboot | factory 832K + ota_0 2880K + FS 320K; handover by erasing otadata | 794,496 B (incl. TLS, MQTT, web UI) | Pattern yes, code no |
| MycilaSafeBoot (MIT, v3.3.9) | factory 640K + ota_0; borrowed from Tasmota | 630,576 B published | Built and evaluated, see 4.1 |
| warmcat/lws-esp32-factory | factory <1MB + OTA slot, TLS, HTTP/2 | n/a | No: stale since 2018, no S3, needs TLS |

The common design is an updater in the **factory** subtype, not in `ota_1`. The
ESP-IDF bootloader boots the factory app when otadata is blank or when `ota_0`
fails image validation. That gives a recovery path without a custom bootloader.

Sources: arendst/Tasmota (`partitions/`, `support_esp32.ino`,
`xdrv_01_9_webserver.ino`), tasmota/install release assets,
tasmota.github.io/docs/Safeboot, mathieucarbou/MycilaSafeBoot (README, v3.3.9
assets), warmcat/lws-esp32-factory, esp32.com forum threads.

## 4. Measurements

### 4.1 Updater size

All builds: pioarduino platform 55.03.35, Arduino-ESP32 3.3.5, `-Os`.

| Updater | Size | Notes |
|---|---|---|
| Own minimal updater (`.tmp/spike-1222/otgw-safeboot`) | **946,327 B** | `WiFi.begin()` from NVS, `WebServer`, `Update` to ota_0, no mDNS/OTA/TLS |
| MycilaSafeBoot v3.3.9, unmodified | 980,059 B | includes ESPConnect, mDNS, ArduinoOTA, logging |
| MycilaSafeBoot published asset | 630,576 B | needs the Tasmota platform with trimmed libs; **not reproduced** |

The 630 KB figure comes from the Tasmota platform. Two attempts to build with it
on Windows failed (toolchain not on PATH, esptoolpy package error). The first
attempt also overwrote the shared PlatformIO packages (Arduino framework 3.3.5
was replaced by 3.3.4). `build.bat --target esp32` restored them, and the
versions were checked afterwards (logged as bug-897).

An IDF-native updater was not built. Even a 600 KB updater does not fit beside
the plain assets (see 4.3), so its size cannot change the verdict.

**Choice: own updater over MycilaSafeBoot.**
1. Mycila reuses WiFi only through its own MycilaESPConnect config. With our NVS
   credentials it would start an access point. Ours reuses them (bench step 3).
2. On our platform Mycila is larger (980,059 B vs 946,327 B).
3. Both are plain HTTP, which matches the no-HTTPS rule.

Kept from Mycila: pointing the boot partition back at `ota_0` on start. This is
safe because `esp_ota_set_boot_partition()` refuses an invalid image.

### 4.2 LittleFS need

- Shipped assets: 43 files, 1,378,195 B raw.
- Minimum image size from a `mklittlefs` bisect (`-b 4096 -p 256`, 4 KiB steps):
  **1460 KiB**.
- Browser-only gzip (.js/.css/.html; 504,911 B content): **604 KiB**. Files the
  firmware reads itself (PIC .hex, .json, simulation logs, .ini) stay raw.
- Runtime files on the bench: settings.ini 5,784 B; reboot_log 2,602 B;
  bootdetect.log 2,195 B; transfer.dat 887 B; sat_*.json under 100 B each.

Correction: an earlier estimate of 528 KiB also gzipped the files the firmware
reads itself. It is replaced by the 604 KiB figure above.

### 4.3 Fit

Plain assets: 3968 - 1460 (FS) - 1976 (combo) = **532 KiB** for the factory
partition. After 64K alignment of `ota_0` that is 512K, with zero app headroom
and no room for runtime files. No updater we measured fits.

Candidate layout with browser-asset gzip:

| Partition | Offset | Size | Contents |
|---|---|---|---|
| nvs, otadata, ... | 0x9000 | unchanged | |
| safeboot (app/factory) | 0x10000 | 0xF0000 (960K) | updater 946,327 B, ~36 KB slack |
| app0 (app/ota_0) | 0x100000 | 0x230000 (2240K) | combo 2,023,296 B, headroom 270,464 B |
| spiffs (LittleFS) | 0x330000 | 0xC0000 (768K) | 604K assets + 164K runtime/COW |
| coredump | 0x3F0000 | 0x10000 | unchanged |

The ~36 KB slack in the factory partition is thin. Ethernet support or
authentication in the updater may push it to 1024K, which takes 64K from the app
or the filesystem.

## 5. Bench proof

### 5.1 Layout used

This was the **proof layout, not the candidate**. The candidate does not fit
under the current asset policy, so the proof layout sized the factory partition
for the measured updater and used a small, empty LittleFS:

| Partition | Offset | Size |
|---|---|---|
| safeboot (factory) | 0x10000 | 0xF0000 |
| app0 (ota_0) | 0x100000 | 0x200000 |
| spiffs | 0x300000 | 0xF0000 |
| coredump | 0x3F0000 | 0x10000 |

Bootloader and NVS were not touched.

### 5.2 Steps and results

| # | Test | Result | Evidence |
|---|---|---|---|
| 1 | Full flash backup | `read-flash 0 0x400000`, sha256 `15208f6f4d4dade7...` | `bench-backup-full.sha256` |
| 2 | Flash proof layout | table, updater (946,736 B), combo +dddc423, empty FS; otadata erased; all "Hash of data verified" | esptool output |
| 3 | Blank otadata boots factory | `[safeboot] WiFi KeepOut2 ip 192.168.88.61`: NVS credentials reused, same IP, no reprovisioning | `serial-01-first-boot.log` |
| 4 | Main firmware from relocated slot | after `/reboot`: fwversion `+dddc423`; LittleFS mounted at new offset (totalBytes 983,040); no rebuild | transcript 23:53:31 |
| 5 | Handover to safeboot | otadata erase + reset (esptool, standing in for a future firmware endpoint) → safeboot up | transcript 23:53:49 |
| 6 | Firmware upload over WiFi | `+db3c020` image, 2,036,928 B: HTTP 200 in **11.0 s**, then fwversion `+db3c020` | `serial-03-upload.log`, transcript 23:54:33 |
| 7 | Truncated upload (1,000,000 B) | HTTP 500 "Could Not Activate The Firmware" (`esp_image: Checksum failed`); safeboot stays up | `serial-04-truncated.log` |
| 8 | Broken ota_0 falls back | after `/reboot`: `E (378) boot: OTA app partition slot 0 is not bootable`, then `[safeboot] start` | `serial-04-truncated.log` |
| 9 | Interrupted upload + hard reset | curl killed at 6 s (100 kB/s), RTS reset as power-cut stand-in → back in safeboot, WiFi up | transcript 23:57:57 |
| 10 | Recovery over WiFi | good image: HTTP 200 in 10.4 s → main firmware `+db3c020` boots | `serial-05-recovery.log` |
| 11 | Restore | `write-flash 0x0 bench-backup-full.bin`, hash verified; LittleFS back to 1,572,864 B | transcript 23:59:26 |

Caveat on step 9: `ota_0` was already invalid after step 7. "Interrupt on top
of a valid image" was not shown separately. Steps 7 and 8 do show that any
partial write invalidates `ota_0` and leads to fallback.

Restore detail: the first two restore attempts failed with "Could not open COM4,
the port is busy" (a serial capture still held the port). The third succeeded.
The settings diff after restore was 1 key of 182: `satenabled` true → false. The
restored `settings.ini` holds `"SATenabled": false`; the `true` before the test
came from the MQTT-lost SAT fallback (`SATcontrol.ino:4223`), which sets the
flag in RAM only. The bench MQTT broker had been unreachable for more than
5 minutes before the test.

All evidence is in `.tmp/spike-1222/bench/` (gitignored scratch):
`transcript.txt`, `serial-0*.log`, settings and listfiles JSON before/after.

## 6. Findings for the epic

1. **A failed upload destroys the old firmware.** `Update` erases `ota_0` as it
   writes. With one app slot there is no "keep the old image" rollback. The unit
   stays recoverable over WiFi through safeboot, but it does not gateway until a
   good image is uploaded. This differs from A/B OTA, and the ADR must say so.
2. **No rebuild for the new offset.** The main firmware ran unchanged from
   0x100000. ESP32 app images are mapped through the MMU, and LittleFS is found
   by partition label.
3. **The fallback is free.** After a failed upload, otadata still points at
   app0. The bootloader validates the image on every boot and falls back to the
   factory app by itself. No custom bootloader is needed for this path.
   Crash-loop rollback of a valid but crashing image does need
   `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` (TASK-1223.7).
4. **Migration cost.** Every field unit needs one USB flash (partition table and
   LittleFS offset). Settings are lost unless backed up first. The full-flash
   backup and restore used here is the safe pattern (TASK-1223.8).

## 7. Side observations (not OTA)

- The bench LittleFS on today's 1.5 MiB layout reports used = total
  (1,572,864 B). That is the 95% clamp, so real use is at least 95%. About
  75 KiB is left for settings, logs and LittleFS copy-on-write. This risk exists
  today, independent of OTA, and deserves its own task.
- The bench MQTT broker was down for more than 5 minutes before the test.

## 8. Open decisions

1. **Asset lever:** gzip the browser assets (overrides the no-gzip directive),
   or drop one web UI (unmeasured).
2. **TASK-1222 AC2** (IDF-native updater): drop, since its size cannot change the
   verdict.
3. **TASK-1222 AC8:** reword to "proof layout", or re-run on the final layout
   once the asset lever is chosen.

## 9. Scope check

- `git diff` on `src/` and both partition CSVs: empty. The build wrapper stamped
  `version.h` / `version.hash`; that was reverted.
- The bench is restored byte-exact from the full backup.
- Spike code lives in `.tmp/spike-1222/` and is not merged.
