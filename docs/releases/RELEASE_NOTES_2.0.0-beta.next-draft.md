# OTGW-firmware v2.0.0-beta.NEXT Release Notes (draft skeleton, route B: bridge guards first, telemetry when it lands)

Not a release yet. Skeleton for the beta after beta.474 (Saturday 28 Nov 20:00 slot, candidate A of the coordinator's plan). Lines marked [BENCH-TBD] are filled from the bench of that build; [verify] lines may not go out as they stand. One line per user-visible change; every claim is checked again at the release commit. Not pushed. No Sergeant D entry numbers and no names in public text. The text about remote telemetry may say nothing stronger than the approved dialog (draft 2, consent version 1).

## Heads-up: the beta after this one needs a one-time USB flash

<!-- Route B (Robert, ADR-304 Rejected): the bridge guards come first and the new flash layout in a LATER beta. This beta carries the guards and no new table: no USB flash is needed for THIS beta. Keep the heads-up until the table beta ships; when it does, replace this section with the migration section kept in the HOLD block below. -->

- **This beta needs no USB flash.** It adds checks to the update page (see "Update safety" below) so that a unit refuses an image for another flash layout.
- **The beta after this one changes the flash layout** (the firmware slot grows from 2240 KB to 2304 KB, the filesystem shrinks from 768 KB to 704 KB), and every existing unit will need one USB flash with `flash_otgw --migrate`. We will say so again, with the steps, in the notes of that beta and in #beta-testing. Your settings and WiFi login are kept by that tool. Save your settings now: `curl http://<ip>/api/v2/settings -o settings-backup.json`.

<!-- HOLD (use in the notes of the table beta, migration section FIRST):
### Breaking change: this beta changes the flash layout. Existing units need one USB flash first.

- **Who:** every unit on an ESP32-S3 board (OTGW32, and an OTGW Classic with an S3 Mini or S3 Mini Pro) that was set up before this beta. A new S3 Mini that you flash for the first time over USB already gets the new layout.
- **Why:** the firmware with remote telemetry and encrypted uploads no longer fits the old firmware slot. The slot grows from 2240 KB to 2304 KB and the filesystem shrinks from 768 KB to 704 KB. A WiFi update cannot change the flash layout, only a USB flash can.
- **How:** with the unit connected over USB, run from the unpacked release zip: Windows `flash_otgw.bat --migrate --host <unit>`, Linux and macOS `./flash_otgw.sh --migrate --host <unit>` (add `--password <password>` if the unit has an admin password). The tool saves `settings.ini` and `dallas_labels.ini` over the network first, writes the new layout around the WiFi data, waits for the unit, puts the settings back and checks them. [BENCH-TBD: duration; settings with 0 differences; WiFi kept]
- **What stays:** your settings (`settings.ini`, `dallas_labels.ini`), your WiFi login and the admin password. The runtime files the firmware keeps (SAT heating curve, PID, energy, markers, tune, labels, reboot count and log) are saved and restored too [BENCH-TBD: unit-verified on the bench unit after the commit that added it]. **What goes:** other files on the unit's filesystem, such as recordings. Download them first.
- **If you update through the browser anyway:** the firmware installs, the new web files do not, and the unit warns at boot and on the update page until you migrate; your settings stay. [BENCH-TBD: confirmed on the bench]
- **Going back:** to go back to the previous beta after the one-time USB migration, run `flash_otgw --migrate --host <ip>` from the PREVIOUS release's zip: its migrate pair rewrites the partition table and the partitions and leaves the WiFi and network storage (NVS) alone. Do not flash the old `merged-full.bin`: it erases that storage, so the unit forgets its WiFi and, if on, its remote telemetry enrolment. Never use `--update` for this. Not yet verified on hardware (plain words for the Sunday 08:00 notes; the rollback in this direction is tested after the publish, then update the release body in a follow-up edit and write "tested on" only with the bench-tester's result).
- **Before you start:** save your settings (`curl http://<ip>/api/v2/settings -o settings-backup.json`).
- **Not verified yet:** [BENCH-TBD: the migration on a bench unit, a unit that skipped the bridge release, recovery after a half-finished migration]. Remote telemetry asks for consent only after the migration, because the web files carry the dialog.
-->

## What is new in this beta, in plain words

### Remote telemetry (experimental, off by default, boards with PSRAM only)

- **Remote telemetry** (TASK-3092 and the recorder tasks). If you say yes in a new dialog, your gateway sends a diagnostic recording (OpenTherm messages, SAT decisions, error lines, setting changes, weather and sensor readings) to a server of the firmware maker, so we can see real systems and not only our test bench. It is off until you answer; "No" is final; you can switch it on or off under Settings > System > Remote telemetry and it stops at once. The data is deleted from the server after 30 days. To have it deleted earlier, quote your telemetry ID in a GitHub issue or on the Discord support channel; we delete everything under that ID within 7 days. Switching it off does not delete data that was already sent. Traffic on the OTmonitor port (25238) is not recorded. Read "Remote telemetry" in the manual for the full list of what is sent and what can still be in the data. [BENCH-TBD: what ran on a unit: dialog, upload, switch-off, a flash between two releases]
- **Boards without PSRAM:** the setting is greyed out and nothing is ever sent.
- **Size:** [BENCH-TBD: app image size and free flash of the build, from the build output].

### Update safety (bridge guards)

- **The update page checks the flash layout** [verify: ADR-298 bridge items as they land]: an image for the other layout is refused, a filesystem image of the wrong size is refused before anything is erased, and the device info shows the layout (`partition_layout`). [BENCH-TBD]

### OTGW32 hardware

- **Warning, 3.3 V only:** the DI pad (GPIO 5, the S0 default) and the 1-wire pad (GPIO 4) are 3.3 V inputs and are not 5 V tolerant. Do not connect an S0 meter or sensor that drives them to 5 V or 12 V (reported by the board's hardware owner).

### SAT, web UI and other changes

- [Filled from the lane list of the build commit: one line per user-visible change with its TASK number. Candidate B (the MM change, the heating-failure warning, web) goes into the Sunday beta unless the coordinator moves it.]

## Not verified yet, in plain words

- [BENCH-TBD] Telemetry on a real home network; the consent dialog in real browsers; more than one unit.
- [BENCH-TBD] The migration for every route (USB from the old table, the browser update fallback).

## Known issues

(carry over from beta.474 and add what this bench shows)

- The retained state echo of the SAT settings topics (`sat/weather_lat`, `sat/weather_lon`, `sat/zone_count` and the other static `sat/*` settings) is published only once SAT is not held back by its start gates: the first 60 seconds after boot, a lost OpenTherm link, or a missing outside temperature. Setting them over MQTT or REST still works.
- On a unit without extra memory, two web pages open at the same time plus other requests can bring the memory low for a moment; the unit recovered by itself in our test.
- WiFi packet loss and failing filesystem updates while BLE scanning is on (TASK-3126); the web interface can be unreachable for up to about 3 minutes after a severe low-memory event (TASK-3025, TASK-3119, TASK-3123); memory dip after a start on a unit without extra memory (TASK-3024).
- For the 1.x line, not 2.x: the real 1.x `flash_otgw.sh` in an empty folder exits with code 2 before it downloads anything.
