# Crash-loop rollback to safeboot: feasibility (TASK-1223.7)

Date: 2026-10-15. Branch: `dev`. Context: ADR-191 (safeboot factory partition on
4MB boards), open question "Crash-loop rollback".

Method: static analysis of the installed, pinned toolchain only. Nothing was
built, flashed or installed. Every runtime claim below is derived from source
and binaries, not from a bench run. The bench proof is listed under
"Verification still needed".

## Verdict

**Feasible as option A, the stock bootloader app rollback. No bootloader change
is needed.** The precompiled bootloader and the precompiled `app_update` library
that pioarduino 55.03.35 / Arduino-ESP32 3.3.5 ship for the ESP32-S3 are already
built with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Hybrid compile
(`custom_sdkconfig`) and a separately built ESP-IDF bootloader are not needed.

Rollback still catches almost nothing out of the box. Arduino's
`initArduino()` marks a new image valid before `setup()` runs. The work is in
two applications:

1. **Main firmware:** defer the "valid" mark with `verifyRollbackLater()` and
   mark the image valid after a health gate.
2. **Safeboot updater:** do not re-arm `ota_0` automatically after a rollback.
   The spike updater does that on every start.

Rollback only guards the first boot of a newly installed image, until the image
marks itself valid. A crash loop that starts later, for example after a
settings change, is not caught. An app-level boot counter (option C) can cover
that case as an optional complement. It does not replace option A.

## Evidence

Packages used by the pinned platform (`platformio.ini:106`, pioarduino
55.03.35). The installed platform is
`D:\DevData\platformio\platforms\espressif32@src-17531e5e458e568c91320bf271c45c44`.
Its `platform.json:32-42` points at the Arduino-ESP32 3.3.5 release archives.

- `framework-arduinoespressif32` 3.3.5. `.piopm` URI: `.../arduino-esp32/releases/download/3.3.5/esp32-3.3.5.tar.xz`.
- `framework-arduinoespressif32-libs` `5.5.0+sha.9bb7aa84fe`. `.piopm` URI:
  `.../3.3.5/esp32-3.3.5-libs.tar.xz`. `versions.txt`: esp-idf `release/v5.5 9bb7aa84fe`.
- `framework-espidf` 5.5.1.251215, used here only as readable source for the
  same IDF release line. Its line numbers match the DWARF line table of the
  shipped bootloader exactly (see 1b), so the source matches the binary.

Note: `D:\DevData\platformio\platforms\espressif32` (no suffix) is the Tasmota
platform 2025.11.30. It was not used and must not be used.

### 1. The shipped bootloader has app rollback enabled

Three independent sources agree.

**a. sdkconfig used to build the libs and bootloader**

- `framework-arduinoespressif32-libs/esp32s3/sdkconfig:423`
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
- same file `:424` `# CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK is not set`
- same file `:4086` `CONFIG_APP_ROLLBACK_ENABLE=y` (legacy alias)
- `esp32s3/{qio_qspi,qio_opi,dio_qspi,dio_opi,opi_opi}/include/sdkconfig.h:403`
  `#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1`, the same in all five
  memory variants.
- Bootloader log level is ERROR (`sdkconfig:444`). Informational bootloader
  messages about rollback are therefore not printed on the serial port.

**b. The code is present in the bootloader binary**

The builder converts
`framework-arduinoespressif32-libs/esp32s3/bin/bootloader_${boot_mode}_${f_boot}.elf`
into `bootloader.bin` (`framework-arduinoespressif32/tools/pioarduino-build.py:68-126`).
There is no variant override, because `variants/esp32s3/` contains only
`pins_arduino.h`. For our QIO 80 MHz board that is `bootloader_qio_80m.elf`.

Disassembly of that ELF (`xtensa-esp-elf-objdump -d -l`) shows four calls to
`write_otadata$isra$0` (at 0x403ccc70):

| Call site address | Source line (DWARF) | Meaning |
|---|---|---|
| 0x403cccd7 | `bootloader_utility.c:500-501` | `set_actual_ota_seq` (always present) |
| 0x403cce61, 0x403cce7a | `bootloader_utility.c:401-402` | loop that marks `PENDING_VERIFY` as `ABORTED`, unrolled for both otadata sectors (rollback only) |
| 0x403ccef6 | `bootloader_utility.c:449-450` | `NEW` becomes `PENDING_VERIFY` on first boot (rollback only) |

In `framework-espidf/components/bootloader_support/src/bootloader_utility.c`,
lines 396-403 and 446-452 sit inside `#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`.
Without that option those call sites would not exist.

**c. The app-side library is built with it too**

`framework-arduinoespressif32-libs/esp32s3/lib/libapp_update.a` contains the
string `Running app has not confirmed state`. That string exists only inside
`#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` in `esp_ota_begin`
(`framework-espidf/components/app_update/esp_ota_ops.c:164-171`).

**d. The bench device already runs this exact bootloader**

The first 20,224 bytes of `.tmp/spike-1222/bench/bench-backup-full.bin` (offset
0x0) are byte-identical to `.tmp/spike-1222/otgw-safeboot/.pio/build/safeboot-s3/bootloader.bin`,
which the pinned platform produced ("Building .pio\build\safeboot-s3\bootloader.bin",
`otgw-sb-pioarduino.log:66`). This proves the result only for the bench device.
Field units that were only updated with `--update` (app plus filesystem) keep
the bootloader of whichever platform pin first flashed them. Older pins were not
checked and must not be assumed to have rollback.

### 2. How the mechanism behaves with a factory partition and one OTA slot

Source: `framework-espidf` v5.5.1, cross-checked against the binary in 1b.

- `esp_ota_set_boot_partition(ota_0)` validates the image, then
  `esp_rewrite_ota_data()` writes the next otadata entry with
  `ota_state = ESP_OTA_IMG_NEW` (`esp_ota_ops.c:113-121, 552-597, 599-640`).
  Arduino `Update.end()` calls it (`libraries/Update/src/Updater.cpp:592`).
- On the next boot the bootloader turns `NEW` into `PENDING_VERIFY` and boots
  `ota_0` (`bootloader_utility.c:446-452`).
- On any later boot, whatever the reset reason, an entry still in
  `PENDING_VERIFY` becomes `ABORTED` (`bootloader_utility.c:396-405`).
- `ABORTED` and `INVALID` entries count as invalid
  (`bootloader_common_loader.c:71-74`). With no valid entry left and a factory
  partition present, the bootloader logs "Defaulting to factory image" and boots
  safeboot (`bootloader_utility.c:407-413`).
- The app can also roll back on purpose with
  `esp_ota_mark_app_invalid_rollback_and_reboot()`. With a single OTA slot this
  works because `esp_ota_check_rollback_is_possible()` accepts a valid factory
  image as the target (`esp_ota_ops.c:848-899`, factory branch at `:871-880`).
- **Never enable `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK`.** It assumes there is no
  factory partition (`bootloader_utility.c:431`) and changes the selection logic.

### 3. What the Arduino core does

`framework-arduinoespressif32/cores/esp32/esp32-hal-misc.c`:

- `:237-246` weak `verifyOta()` returns `true`; weak `verifyRollbackLater()`
  returns `false`.
- `:277-290` in `initArduino()`: unless `verifyRollbackLater()` returns true, a
  running image in `PENDING_VERIFY` is marked valid immediately with
  `esp_ota_mark_app_valid_cancel_rollback()`.
- `cores/esp32/main.cpp:112-113`: `app_main()` calls `initArduino()` and only
  then creates `loopTask`, which runs `setup()` (`main.cpp:67`).

Consequence: by default the image is marked valid before `setup()` runs. Only
a crash before `initArduino()` triggers a rollback. To catch crashes in
`setup()` and in the first minutes of `loop()`, the firmware must override the
hook:

```cpp
extern "C" bool verifyRollbackLater() { return true; }   // C linkage: the weak symbol lives in a .c file
```

The core already uses this pattern: `libraries/RainMaker/src/RMaker.cpp:11`.
No file under `src/` or the repo `.pio/` currently defines
`verifyRollbackLater`, and the firmware has no `esp_ota_mark_*` call today
(grep, 2026-10-15). The firmware then calls
`esp_ota_mark_app_valid_cancel_rollback()` once its health gate passes.

### 4. Interaction with the ADR-191 flow

**a. Handover (main firmware erases otadata).** This works unchanged. Erase the
whole otadata partition (0xE000, 0x2000), both sectors, as the bench did
(`bench/transcript.txt:12`). Erasing only one sector could leave an older
`VALID` entry, and with one OTA slot that entry points at the same `ota_0`, so
a crashing image would be booted as `VALID` and never rolled back.
`esp_ota_set_boot_partition(factory)` also erases the whole partition
(`esp_ota_ops.c:609-615`).

**b. Safeboot re-arms the bad image (needs a change).** The spike updater calls
`esp_ota_set_boot_partition(ota_0)` on every start
(`.tmp/spike-1222/otgw-safeboot/src/main.cpp:48-51`). After a rollback, no
otadata entry is valid, so that call writes a fresh `NEW` entry. The next reset
boots the crashing image again. It crashes once and lands back in safeboot. On
every power cycle the unit runs the bad image once more and is not reliably
reachable. The production updater must:

- check for a rollback first, for example with
  `esp_ota_get_last_invalid_partition() != NULL`, which finds an `ABORTED` or
  `INVALID` entry for `ota_0` (`esp_ota_ops.c:963-1005`). It must check before
  re-pointing, because re-pointing overwrites otadata sector 0;
- if a rollback happened, stay in safeboot and report it, for example
  `"rolled_back": true` in `/info`. Re-arm only on an explicit user action or a
  new upload;
- otherwise, after a normal handover with blank otadata, re-point as today.
- Optional, to keep the state simple: erase otadata before `Update.begin()`, so
  that `Update.end()` writes exactly one `NEW` entry.

**c. The `PENDING_VERIFY` guard in `esp_ota_begin`** (`esp_ota_ops.c:164-171`)
does not block safeboot. It only fires when the running partition is an OTA
slot, and for factory `esp_ota_get_state_partition()` returns
`ESP_ERR_NOT_SUPPORTED` (`:1013-1015`). Arduino `Update` does not call
`esp_ota_begin` anyway. In safeboot, `initArduino()` makes the same query, gets
`NOT_SUPPORTED`, and does nothing.

**d. Deliberate restarts count as failures.** Any reset before the image is
marked valid aborts it. That includes `platformRestart()` after a settings
save, a LittleFS OTA, or a WiFi re-provision. The firmware must mark itself
valid before any deliberate restart, or keep the health window short.

**e. Mixed versions.** If the bootloader lacks rollback but the app has it, the
state stays `NEW`, nothing is monitored, and nothing breaks. The reverse case
cannot happen with this pin. Add a compile-time guard so that a future platform
bump cannot silently drop the feature:
`#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` followed by `#error`, placed in
the Platform abstraction.

### 5. Coverage of realistic failure modes (option A, with deferred mark-valid)

| Failure mode | Caught? | Why |
|---|---|---|
| Crash in static constructors or early init | Yes | Any reset while the image is `PENDING_VERIFY` |
| Panic in `setup()` | Yes | Same |
| Task or interrupt watchdog loop before mark-valid | Yes | WDT reset, then `ABORTED` |
| Hang without a watchdog reset | Partly | Nothing happens until the next reset or power cycle, which then rolls back |
| Brownout loop before mark-valid | Yes | State is in flash, so it survives any reset type |
| Power cut or a single brownout during the health window | False positive | Lands in safeboot and needs a user re-arm. Keep the window short |
| Crash loop starting after mark-valid (settings, heap exhaustion days later) | **No** | Rollback only covers the first boot of a new image |
| Corrupt or truncated image | Yes, already | Image validation (proven in the spike, `serial-04-truncated.log:18`) |

## Options compared

| | A: stock bootloader rollback | B: hybrid compile (`custom_sdkconfig`) | B': own IDF bootloader | C: app boot counter |
|---|---|---|---|---|
| Bootloader change | None | Rebuilt | Rebuilt separately | None |
| Build-time cost | 0 | Full IDF lib rebuild per sdkconfig change (pioarduino `arduino.py:590-656`, `call_compile_libs` at `:659`) | Separate IDF toolchain and CI job | 0 |
| Flash/RAM cost | Code already linked; one otadata sector write per new image | Same as A | Same as A | NVS writes on every boot |
| Migration impact | Bootloader already in the image; the migration must write it at 0x0 | Same, plus a custom bin | Same, plus a custom bin | None |
| Catches early crashes | Yes | Yes | Yes | Only after its own code runs |
| Catches late-onset loops | No | No | No | Yes |
| Shared-core risk | None | High: hybrid compile writes into the shared `framework-arduinoespressif32-libs` package, and other envs without `custom_sdkconfig` then trigger a reinstall (`arduino.py:571-572, 645-656, 886-913`) | None in PIO, new toolchain | None |

Option B is unnecessary because the option is already on, and it carries
exactly the shared-core pollution risk this project has been bitten by before.
Option B' is unnecessary for the same reason.

### Option C in detail (complement, not replacement)

The idea: the main firmware keeps a boot counter. If it boots N times within X
seconds without reaching "healthy", it erases otadata and restarts into
safeboot.

- **Where the counter lives.** `RTC_NOINIT_ATTR` memory survives software
  reset, panic and watchdog resets, but not power-on reset. Its behaviour on an
  ESP32-S3 brownout reset is **unverified**; no primary source was checked. NVS
  survives every reset type and costs one small write per boot plus one at the
  healthy mark. That is acceptable for NVS wear levelling.
- **The existing counter is too late.** `updateRebootCount()`
  (`src/OTGW-firmware/helperStuff.ino:137`, called at `OTGW-firmware.ino:514`)
  lives on LittleFS and runs well into `setup()`. Crashes before it are not
  counted. A C counter would have to run first in `setup()`, or earlier.
- **Same re-arm problem.** Erasing otadata looks to safeboot like a normal
  handover, so safeboot would re-arm `ota_0`. C needs its own marker, for
  example an NVS key `rollback_reason` that safeboot reads, or it must write an
  `INVALID` state instead of erasing. The simplest form is
  `esp_ota_mark_app_invalid_rollback_and_reboot()`, which marks the entry
  `INVALID`. The safeboot rollback check from 4b then covers both A and C.

## Recommendation

Adopt option A. Cost: no bootloader build, no build-time change, and no extra
migration payload beyond writing the standard `bootloader.bin` at 0x0, which
the migration image must do anyway.

Required work, split across existing subtasks:

1. **Migration (TASK-1223.8):** the one-time USB image must write
   `bootloader.bin` at 0x0 from the pinned platform. Do not rely on the
   bootloader already present in the field. Erase otadata as part of migration.
2. **Main firmware:**
   - define `extern "C" bool verifyRollbackLater() { return true; }`;
   - call `esp_ota_mark_app_valid_cancel_rollback()` after a health gate based
     on the device's own condition, for example the main loop running for 60 s
     with heap above the critical tier. Do not gate on WiFi or MQTT: a unit
     whose router is down would otherwise fall back to safeboot for no fault of
     its own;
   - mark valid before any deliberate restart;
   - add the compile-time `#error` guard (4e).

   Per CLAUDE.md, the hook and the mark-valid call belong behind a
   `platformXxx()` shim in `src/libraries/Platform/src/platform_esp32.h`.
3. **Safeboot updater:** detect a rollback before re-pointing (4b), stay in
   safeboot, and report it. Erase otadata before an upload.
4. **Optional later:** add option C for late-onset loops, using
   `esp_ota_mark_app_invalid_rollback_and_reboot()` so that the safeboot check
   covers it too.

## Risks

- **False positive on power loss** during the health window. The unit lands in
  safeboot and stays there until a user re-arms it. Mitigation: a short window,
  and an explicit "rolled back" message in safeboot. A retry budget (re-arm once
  automatically, counted in NVS) is possible but adds state. Decide this at
  implementation.
- **Late-onset crash loops are not covered** by A (see table).
- **Future platform bumps.** The bootloader on the device never changes over
  WiFi, but the app side follows the pin. The compile-time guard catches a pin
  without rollback.
- **Unexplained bench otadata.** The bench backup shows otadata[0] `seq=1`,
  `state=0xFFFFFFFF` (UNDEFINED), written before safeboot existed. It is
  neither evidence for nor against rollback. Migration erases otadata, so it
  does not matter.
- **Everything above is static analysis.** It has not been run on the bench.

## Verification still needed (for TASK-1223.9 bench proof)

1. A test image whose `setup()` calls `abort()`, uploaded through safeboot.
   Expected: one crash, then the bootloader boots factory; the otadata dump
   shows `state=4` (`ABORTED`); the modified safeboot reports a rollback and
   does not re-arm.
2. A good image with `verifyRollbackLater()` overridden. Expected: the state is
   `1` (`PENDING_VERIFY`) before the health gate and `2` (`VALID`) after it.
   Read it with `esptool read-flash 0xE000 0x2000`; the state is at byte offset
   24 of each sector.
3. Power-cut test during the health window, to confirm the false-positive path
   ends in a usable safeboot.
