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

## 2. Updating the firmware over WiFi

1. Open `http://<unit>/update`.
2. Click **Restart into the firmware updater** and confirm. The gateway stops and the unit restarts into safeboot at the same address, within a few seconds.
3. The page reloads into the updater. Select the **`.ino.bin`** file (not the littlefs or merged image) and upload it.
4. The unit restarts into the new firmware. A 2 MB image takes about 15 seconds on a normal WiFi link.

Settings and WiFi are kept. The filesystem (web UI) is updated separately, on the same `/update` page, without safeboot.

When an admin password is set, safeboot asks for it too (user `admin`).

## 3. When something goes wrong

| Situation | What happens | What to do |
|---|---|---|
| The upload is interrupted (WiFi drop, power cut) | The unit restarts into safeboot; the old firmware is gone | Open `http://<unit>/` and upload the firmware again |
| The upload is incomplete or not a valid image | Safeboot refuses it ("Update failed") and stays up | Upload a valid `.ino.bin` |
| The new firmware crashes during its first 30 seconds | The unit returns to safeboot on its own and shows "The firmware failed to start and was rolled back" | Upload a working firmware |
| The unit is not on WiFi in safeboot (no stored WiFi) | Safeboot opens an access point `<hostname>-safeboot` | Connect to it and open `http://192.168.4.1/` |

Unlike phones and PCs, the unit has room for only one firmware, so there is no automatic return to the previous version. Safeboot keeps the unit reachable until a good firmware is uploaded. The gateway does not run while safeboot is active.

## 4. Order when you update without migrating

On a unit that is still on the old layout, update the firmware first and the filesystem second. Older firmware looks for the classic `index.html` at `/`. A new filesystem on old firmware therefore shows the File Explorer there; the web UI is still at `/v2.html`.

## Background

- Layout: safeboot updater at 0x10000 (960K), firmware at 0x100000 (2240K), filesystem at 0x330000 (768K). See `partitions_otgw_esp32.csv`.
- Decision records: ADR-191 (safeboot OTA), ADR-192 (v2-only web UI), ADR-190 (minified filesystem).
