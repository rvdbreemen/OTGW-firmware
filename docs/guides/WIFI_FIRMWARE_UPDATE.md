# Firmware updates over WiFi on ESP32-S3 boards (4 MB)

From firmware 2.0.0-alpha.425, ESP32-S3 boards with 4 MB flash take firmware updates over WiFi through a small built-in updater, called **safeboot** (ADR-191). Each unit needs **one** USB flash first, to move it to the new flash layout. After that, no USB cable is needed for updates.

## 1. One-time migration over USB

The new layout moves the firmware and the filesystem, so the unit is flashed once over USB. The flash tool saves your settings and puts them back.

1. Download the release zip (`OTGW-firmware-<board>-<version>-flash.zip`) and unpack it.
2. Connect the unit over USB. It must also be on your network, so the tool can save its settings.
3. Run:
   - Windows: `flash_otgw.bat --migrate --host <ip-of-your-unit>`
   - Linux/macOS: `./flash_otgw.sh --migrate --host <ip-of-your-unit>`
   - Add `--password <pw>` if you set an admin password on the unit.

The tool then:

1. downloads `settings.ini` (and `dallas_labels.ini` when present) into a `backup` folder;
2. writes the two `*-migrate-*.bin` images. These skip the area that holds your WiFi credentials, so WiFi is kept;
3. waits for the unit to come back;
4. uploads `settings.ini` again, restarts the unit and checks the file byte for byte.

When it finishes, the unit runs the new layout with your settings. A copy of the settings stays in `backup/<ip>-<date>/`.

`flash_otgw.bat --update` refuses to run on a unit that still has the old layout and tells you to migrate first. That protects the unit: an update written for the new layout does not boot on the old one.

If you cannot reach the unit over the network, `--no-backup` migrates without saving the settings. They then return to defaults.

## 2. Updating over WiFi: one action

From the release that contains the new `/update` page (TASK-1283), one action installs the firmware and the web files.

1. Open `http://<ip-of-unit>/update`. Use the IP address, not `otgw2.local`: the updater has no mDNS name. The page tells you when you opened it by name.
2. Choose the release zip (`OTGW-firmware-<board>-<version>-flash.zip`), or its two files `*.ino.bin` and `*.littlefs.bin`. You can drop them on the page. The page recognises the files by their content and refuses USB images (merged, migrate), the updater itself, a single file and files from different builds.
3. Press **Update** and keep the tab open until it says Done. The page runs these steps, one request at a time:
   1. `POST /safeboot`: the unit restarts into the updater, at the same address.
   2. It uploads the `.ino.bin` to the updater and waits for the unit to answer with the new version.
   3. It waits 35 seconds, so the new firmware is confirmed healthy (ADR-191 marks it valid after 30 seconds) before the next step.
   4. It uploads the `.littlefs.bin` to the new firmware (`POST /update?cmd=100&size=<bytes>`).
   5. It waits for the restart and checks the web files against the firmware (`/api/v2/filesystem/hash-check`) and the version.
4. The whole run takes 2 to 3 minutes. While the updater runs (1 to 2 minutes) the ESP does nothing for the heating: on an OTGW32 (direct OpenTherm) the thermostat-boiler link is interrupted and the boiler runs on its own settings; on a board with the PIC gateway the PIC keeps passing OpenTherm traffic (this PIC behaviour is not measured on the bench).

If you close the tab or lose the connection between the two steps, open `/update` again and choose the same files: the page sees that the firmware is current and installs only the web files. The web files are always sent, also when the unit already reports the same hash: the filesystem is identified by a 7-character githash only, so a matching hash is not proof that the files are there. If the final check fails, or the updater did not receive the whole firmware, **Try again** repeats the step that failed (the web files, or the firmware).

When an admin password is set, the browser asks for it again when the updater starts: the main firmware asks for Digest authentication, the updater for Basic, and the browser keeps them apart. The page waits up to 15 seconds for each answer while the prompt is open. This has not been tried in a browser against real firmware (open item in the TASK-1283 bench plan).

**Bootstrap.** The page that drives the update is the one the unit serves before the update. The first install of the release that introduces this page is therefore still the two-step flow ("By hand" below); the one-action flow works from the update after that.

**By hand.** The old forms stay on the same page, under "Advanced". A firmware update by hand: **Restart into the firmware updater**, then upload the `.ino.bin` in the updater (it restarts into the new firmware). A web-files update by hand: upload the `.littlefs.bin` in the second form. A firmware update through safeboot does not touch the filesystem.

### What a filesystem update keeps and loses

The image replaces the whole filesystem partition (768 KB). It is built from the web assets plus a default `settings.ini`, and nothing else. The running firmware puts the runtime files back after the remount (`OTGW-ModUpdateServer-esp32.h`), in two ways.

Kept, written back from RAM (the firmware holds the value, so it is current even when the file was older):

- **Settings.** `settings.ini` is written back right after the upload (`writeSettings(false)`). Measured on alpha.439: 0 of 567 settings changed across a WiFi filesystem update (TASK-1264).
- **SAT learned state** (TASK-1286), through `satPersistAllToFs()`, which calls the regular savers: `sat_energy.json` (energy total) and `sat_energy_estimate.json` (estimated gas energy), which are otherwise saved hourly and on each 0.1 kWh, so the energy counted since the last save is kept too; `sat_pid_state.json` (controller state; the firmware restores it only if it is less than 30 minutes old and the clock is synced, which a normal update meets); `sat/sat_hcr.json` (heating-curve data); `sat/sat_preset.json` (active preset and window pre-target, TASK-1487).
- **Learned OpenTherm profiles** (`ot-boiler.json`, `ot-thermo.json`): `saveOtSupportFilesForced()` writes both from RAM, which holds what was loaded at boot plus everything learned since the last 15-minute save.
- **The DHW water total** (`dhw_water.json`): the restart that follows the upload flushes it to the new filesystem (`flushDHWWaterMeter()` in `doRestart`). This follows from the code and has not been measured on the bench.
- **WiFi credentials**: they live in NVS, which neither update touches.

Kept, copied to the heap before the image is written and written back after the remount (files without a RAM copy; a file over its cap, a total over 8 KB or a low heap only skips that file, the update goes on):

| File | Holds | Cap |
|---|---|---|
| `dallas_labels.ini` (TASK-1266) | Dallas sensor names. For a larger file the page also keeps a copy in the browser and sends it back after the update | 2 KB |
| `sat_markers.json` | heating-curve markers (at most 20) | 2.5 KB |
| `reboot_count.txt` | reboot counter | 16 B |
| `reboot_log.txt` | the last reboot causes (20 lines) | 3 KB |

Worst case on the heap during the upload: 7.7 KB (2 + 2.5 + 0.016 + 3 KB) in at most four blocks, and only when the largest free block is at least 8 KB above the copy being made. The copies are freed when the update ends, fails or is aborted. The log says why a file was skipped: over its cap, over the 8 KB total, or low heap.

The SAT heating-curve file lives in the `/sat` folder, which a fresh image does not contain. The firmware creates the folder when it writes or migrates the file. Before TASK-1286 it did not, so the heating-curve recommendation was lost at every restart of a unit that started from a fresh image.

Lost (start fresh), because they are not in the image and the firmware does not write them back: `bootdetect.log`, `topic_cleanup.bin` (the MQTT topic cleanup state) and files you uploaded in the File Explorer. `tests/webui/update-orchestrator.test.mjs` checks that none of the runtime names is in the shipped image and that the page text matches; `tests/test_fs_update_keep.py` pins the firmware side (keep list, caps, order of the write-back, every SAT saver called). That a unit really keeps the SAT values across a real update is still to be measured on the bench.

Download `settings.ini` first anyway if you want a safety net: the page has a separate **Download a settings backup** link. It is not part of the update.

Writing the filesystem over USB (`flash_otgw --update`) does reset the settings, because no firmware runs during a USB flash to write them back.

When an admin password is set, safeboot asks for it too (user `admin`).

### Cross-version contract

Release N's page drives release N+1's firmware (and the updater that is installed on the unit). These names are therefore an API. Changing one needs a compatible fallback in the page; the test below fails until the page and the firmware agree again.

- Main firmware: `POST /safeboot` (200, 409); `POST /update?cmd=100&size=<bytes>` (HTTP 200 for success and failure, a failure body starts with `Flash error:`); `GET /api/v2/device/info` (`fwversion`, `hostname`, `ipaddress`, `networkmode`, `board`, `hardware_type`, `safeboot_available`, `app_ota_available`); `GET /api/v2/filesystem/hash-check` (`filesystem_check.match`, `fs_hash`, `fw_hash`); `GET /api/v2/health` (`health.status`).
- Updater (`safeboot/src/main.cpp`): `GET /info` (JSON keys `safeboot`, `app_valid`, `app_rolled_back`, `ap_mode`; any other path answers 302 to an HTML page, so a status code is never proof); `POST /update` (multipart field `firmware`, 200 or 500); the access point `<hostname>-safeboot` at 192.168.4.1.
- Image layout the page recognises: the app starts with `0xE9`, has chip id 9 at offset 12 and the app descriptor magic `0xABCD5432` at offset `0x20`, and is at most 2,293,760 bytes (the app slot); the filesystem image is 786,432 bytes, has the `littlefs` marker at offset 8 or 4104 and contains `version.hash`.

Guard: `node --test tests/webui/update-orchestrator.test.mjs`. The contract test reads the firmware sources; the other tests run the page logic against the real build artifacts and a fake unit.

## 3. When something goes wrong

| Situation | What happens | What to do |
|---|---|---|
| The upload is interrupted (WiFi drop, power cut) | The unit restarts into safeboot; the old firmware is gone | Open `http://<unit>/` and upload the firmware again |
| The upload is incomplete or not a valid image | Safeboot refuses it ("Update failed") and stays up | Upload a valid `.ino.bin` |
| The new firmware crashes during its first 30 seconds | The unit returns to safeboot on its own and shows "The firmware failed to start and was rolled back" | Upload a working firmware |
| The unit is not on WiFi in safeboot (no stored WiFi) | Safeboot opens an access point `<hostname>-safeboot` | Connect to it and open `http://192.168.4.1/` |
| The one-action page does not find the updater within a minute (Ethernet unit, fixed IP address) | The updater always uses WiFi and DHCP, so it may sit at another address | Find the unit in your router, upload the `.ino.bin` there, then open `/update` again and choose the same files to finish the web files. Or switch the unit off and on: it returns to the old firmware |
| The filesystem step fails or the connection drops in the middle | The new firmware is installed; the web files may be half written and the settings only live in memory | Press Try again on the page straight away and do not switch the unit off. Or upload only the `.littlefs.bin` in the second form |

Unlike phones and PCs, the unit has room for only one firmware, so there is no automatic return to the previous version. Safeboot keeps the unit reachable until a good firmware is uploaded. The gateway does not run while safeboot is active.

## 4. Order when you update without migrating

On a unit that is still on the old layout, update the firmware first and the filesystem second. Older firmware looks for the classic `index.html` at `/`. A new filesystem on old firmware therefore shows the File Explorer there; the web UI is still at `/v2.html`.

## 5. Boards with 8 MB flash

The release images use the 4 MB layout on every board, also on a module with 8 MB flash. Such a unit updates through safeboot like any other, and the extra flash stays unused.

Only a unit flashed with a custom build that has two firmware slots (`ota_0` and `ota_1`) takes the `.ino.bin` directly on `/update`, without safeboot. The firmware detects this at runtime. No release image ships that layout. On such a unit the one-action block of the page is switched off and the forms under "Advanced" apply.

## Background

- Layout: safeboot updater at 0x10000 (960K), firmware at 0x100000 (2240K), filesystem at 0x330000 (768K). See `partitions_otgw_esp32.csv`.
- Decision records: ADR-191 (safeboot OTA), ADR-192 (v2-only web UI), ADR-190 (minified filesystem).
