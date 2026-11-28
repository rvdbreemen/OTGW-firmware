# OTGW-firmware v2.0.0-beta.NEXT Release Notes (draft skeleton: ONE combined beta after beta.474 = the Saturday set plus the SAT mode-switching cleanup)

Not a release yet. Skeleton for the next public beta after beta.474. Order of Robert, 28 Nov 08:37 (relayed by the coordinator): SAT changes go first, the 14:00 beta is dropped, and the next beta is ONE combined image: the Saturday set plus the SAT mode-switching cleanup, at the first slot after the SAT lane is green, benched and soaked. The remote telemetry and flash table beta is on HOLD with no date (see the HOLD blocks). Lines marked [BENCH-TBD] are filled from the bench of that build; [verify] lines may not go out as they stand. One line per user-visible change; every claim is checked again at the release commit. Not pushed to the public. No Sergeant D entry numbers and no names in public text.

## Heads-up

<!-- HOLD since 2026-11-28 08:32 (coordinator, Robert's order): the heads-up "the beta after this one needs a one-time USB flash" is REMOVED from the live notes, because the next beta is the SAT beta without the table. Bring a heads-up back only when the table beta has a date. The migration text stays below, in the HOLD block. -->

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

<!-- HOLD (telemetry and table beta, no date; Robert 2026-11-28 08:32 and 08:37). Kept for that beta, do not publish in the combined beta:

### Remote telemetry (experimental, off by default, boards with PSRAM only)

- **Remote telemetry** (TASK-3092 and the recorder tasks). If you say yes in a new dialog, your gateway sends a diagnostic recording (OpenTherm messages, SAT decisions, error lines, setting changes, weather and sensor readings) to a server of the firmware maker, so we can see real systems and not only our test bench. It is off until you answer; "No" is final; you can switch it on or off under Settings > System > Remote telemetry and it stops at once. The data is deleted from the server after 30 days. To have it deleted earlier, quote your telemetry ID in a GitHub issue or on the Discord support channel; we delete everything under that ID within 7 days. Switching it off does not delete data that was already sent. Traffic on the OTmonitor port (25238) is not recorded. Read "Remote telemetry" in the manual for the full list of what is sent and what can still be in the data. [BENCH-TBD: what ran on a unit: dialog, upload, switch-off, a flash between two releases]
- **Boards without PSRAM:** the setting is greyed out and nothing is ever sent.
- **Size:** [BENCH-TBD: app image size and free flash of the build, from the build output].

### Update safety (bridge guards)

- **The update page checks the flash layout** [verify: ADR-298 bridge items as they land]: an image for the other layout is refused, a filesystem image of the wrong size is refused before anything is erased, and the device info shows the layout (`partition_layout`). [BENCH-TBD]
-->

### OTGW32 hardware

- **Warning, 3.3 V only:** the DI pad (GPIO 5, the S0 default) and the 1-wire pad (GPIO 4) are 3.3 V inputs and are not 5 V tolerant. Do not connect an S0 meter or sensor that drives them to 5 V or 12 V (reported by the board's hardware owner).

### SAT: mode switching follows thermo-nova, and the extra rules are gone [HOLD until shipped]

<!-- HOLD: goes out only in the beta that carries steps 3a to 3d of the SAT mode-switching task (Robert, 2026-11-28: one beta, all steps, no Sergeant D question). Fill the TASK ids, the build and the numbers from that build; every [BENCH-TBD] is replaced by what was measured, or the line is cut. Public text: no entry numbers, no names. Rule 7 (hot-water pause) is only written as a change after the thermo-nova climate.py is read and cited. -->

- **SAT starts in continuous mode again and no longer has its own way back to PWM.** Before, every heating start began in PWM, SAT then went to continuous after a few minutes, and a flame-out fail-safe could push it back to PWM. Two long field runs showed about one switch every half hour. Now SAT follows the original design: it starts in continuous and goes to PWM only when the boiler flow stays at least 3.0 C above what SAT asks for during 60 seconds. It leaves PWM when the flow stays 3.0 C below the request for 180 seconds or when PWM has had no off time left for 300 seconds. After a restart SAT puts the PWM state back as it was. [TASK ids follow]
- **What changes for you:** fewer switches between continuous and PWM, so a steadier flow temperature. The setting "PWM switch margin" disappears (SAT uses the fixed margin of 3.0 C); a stored value is dropped [verify: what the firmware does with a stored value and with a settings write of the old key]. If you used that setting to tune your boiler, check the heating curve on your first day with this beta. [BENCH-TBD: the switch count of the replay of the two recorded field runs on the finished code; one bench run with the SAT simulator, not a real boiler]
- **SAT mode command ignored:** the manual mode command (auto, continuous, PWM) no longer changes anything; it is ignored and a log line says so, so Home Assistant automations that send it do not break. It will be removed in a later release. The OFF state (SAT off, safety trip, lost OpenTherm link) stays.
- **The PWM on-time resets on every exit** from PWM (before, SAT kept the used on-time and continued it on return).
- **The PWM clock keeps running during a hot-water draw** (before, it paused during the draw and shortly after). [verify: only after the thermo-nova climate.py is read and cited]
- **Heating switches off at once** when it has to (before, SAT kept it on until the flame had burned 180 seconds). [BENCH-TBD: with a real boiler this is not verified]
- **Not changed:** the tolerance band, the maximum modulation rules (0 in PWM, the configured maximum in continuous), the heating-failure warning and the heating-curve self-tune; they read the PWM state and are re-tested [BENCH-TBD].
- **Not verified yet:** behaviour with a real boiler over a full heating day.

### Changes of the Saturday set [verify each against the landed commit before the release commit]

- **Heating-failure warning:** the 15 minute window counts only while SAT has central heating enabled, so a pause of the tolerance band no longer raises a warning; hours inside a pause are no longer flagged or dropped from the self-tune (TASK-3112). [verify: the PWM OFF phase rule against the lane]
- **SAT maximum modulation:** SAT sends the maximum-modulation value `MM=T` only when it hands the boiler back (SAT off, restart) and not inside PWM, where the value stays 0; continuous mode keeps the configured maximum (TASK-3143). Host tests so far; a real unit is still to be benched.
- **Graph history** records the room temperature that SAT actually controls on, not always the thermostat room, so captures no longer look like overheating that is not there (TASK-3007).
- **Web UI on OTGW32 boards:** hides items that exist only with a PIC, shows the LED help for the board you have, and an S0 pin of -1 shows an empty field with a hint (TASK-3121).
- **Settings help in Dutch, German and Greek:** 166 texts each (TASK-2927). [verify: app size against the size gate]
- **Update fix (FS-OTA):** [verify: what the filesystem-update fix changes for the user]
- Tools in the release zip, not in the firmware image: [verify: capture tool items of TASK-2979 and TASK-3009 if they ride]

### Other changes

- [Filled from the lane list of the build commit: one line per user-visible change with its TASK number.]

## Not verified yet, in plain words

- [BENCH-TBD] Telemetry on a real home network; the consent dialog in real browsers; more than one unit.
- [BENCH-TBD] The migration for every route (USB from the old table, the browser update fallback).

## Known issues

(carry over from beta.474 and add what this bench shows)

- The retained state echo of the SAT settings topics (`sat/weather_lat`, `sat/weather_lon`, `sat/zone_count` and the other static `sat/*` settings) is published only once SAT is not held back by its start gates: the first 60 seconds after boot, a lost OpenTherm link, or a missing outside temperature. Setting them over MQTT or REST still works.
- On a unit without extra memory, two web pages open at the same time plus other requests can bring the memory low for a moment; the unit recovered by itself in our test.
- WiFi packet loss and failing filesystem updates while BLE scanning is on (TASK-3126); the web interface can be unreachable for up to about 3 minutes after a severe low-memory event (TASK-3025, TASK-3119, TASK-3123); memory dip after a start on a unit without extra memory (TASK-3024).
- For the 1.x line, not 2.x: the real 1.x `flash_otgw.sh` in an empty folder exits with code 2 before it downloads anything.
