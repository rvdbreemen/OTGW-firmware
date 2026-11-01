# OTGW-firmware v2.0.0-beta.1 Release Notes

**Release date:** 2026-11-01
**Branch:** dev
**Compare:** there is no earlier 2.x release. Everything since the 1.x line is new to you.

> **Disclaimer: this is the first beta of 2.0.0.** It is not a stable release. It has had alpha testing on a small group of units, but not the long field runs a release needs: the planned 24-hour soak test was not completed for this build (see Known issues). Expect bugs, and keep a way back: save your settings and keep the 1.x release zip at hand. If you need a gateway that just works, stay on the [1.x releases](https://github.com/rvdbreemen/OTGW-firmware/releases) for now.

## What this beta is

This is the first public beta of the 2.0.0 line. 2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported.

The purpose of this beta is field data. We want more people to run it on real heating systems and tell us what happens.

**Supported hardware for this beta:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place.

Read "Before you flash" and "Known issues" first.

---

## SAT is experimental and opt-in

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". It says that SAT is experimental, that it takes over the control setpoint and that we need field data. You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- On a prerelease build the SAT page shows the same notice. You can dismiss it; it comes back in a new browser session.
- While SAT is on, it owns the control setpoint: a `CS=` from MQTT, REST or the serial bridge is refused.
- Some SAT decisions are still open and follow after this beta (see Known issues). SAT behaviour can change between builds.
- If you enable SAT, please share a capture log (see "Capture tool"). SAT is only as good as the field data we get.

Without SAT, the gateway behaves as a transparent OpenTherm gateway, as in 1.x.

Not verified: the same wizard step when SAT is enabled by saving the Settings page (it uses the same code, but only a mock-device test ran). The wizard step itself ran against a mock device, the page notice on a real unit (TASK-1507).

---

## What changed in SAT since the last alpha

These apply only when SAT is on.

- **Max flow setpoint slider** (SAT page, TASK-1373). It uses the boiler's own range, or 20 to 80 C when the boiler reports none. SAT never sends a flow setpoint above it. A value you choose is also sent to the boiler as `SH=`. Until you move the slider, SAT uses 62 C (radiators) or 50 C (underfloor) and sends nothing to the boiler. Home Assistant gets the number `SAT Max Flow Setpoint`.
- **A chosen max flow setpoint is the only cap** (TASK-1513). Without a chosen value the old 65 C ceiling still applies. With a chosen value, SAT caps every CS at it and ignores the 65 C ceiling (within the boiler's range, underfloor at most 50 C). The heat-pump 40 C cap is unchanged.
- **"Central Heating Settings" card with a Max relative modulation slider** (TASK-1514). The card that held the flow slider is renamed. The new slider (0 to 100 %, default 100 = no cap) sets the existing `satmaxmodulation`, which caps every `MM=` SAT sends. Home Assistant gets `SAT Max Relative Modulation`.
- **PWM ignition for a boiler without a return temperature** (TASK-1371). The hold at start-up is flow + 18 C. For the first 20 s after each central heating ignition SAT alternates `CS=` and `PM=25`, then hands over to flow - 1. This applies only to boilers that give no return temperature, and not to boilers that have one. Bench-tested on the Classic + S3 Mini unit (TASK-1504 notes); not run in the field on a boiler without a return temperature.
- **`MI=` sent once per boot, on PIC boards only** (TASK-1371), for such a boiler after 5 minutes of detection, and at once for the Ideal, Intergas and Nefit quirk.
- **CS, MM and CH are sent every control tick and repeated every 5 s** (TASK-1370, ADR-203), as thermo-nova does. The repeat pauses during hot water and a boiler fault.
- **Hot water setpoint** (TASK-1376): `SW=` goes out when the setpoint changes and once after a restart, no longer at the start of every hot-water draw.
- **Outside temperature** (TASK-1501): an MQTT or REST outside temperature younger than 1 hour now beats the boiler value and the weather value for that whole hour. Before, it lost priority after 10 minutes.
- **PWM cycles per hour** is 3 or 4 only, default 3 (TASK-1375). See the breaking changes.
- **Gas consumption in m3/h, split in central heating and hot water** (TASK-1484, TASK-1506): two settings, `satminconsumption` and `satmaxconsumption` (0 to 8), drive the estimate. `sat/consumption` is the central heating value only; the new `sat/consumption_dhw` carries the hot-water value. Home Assistant gets `SAT CH Gas Consumption` and `SAT DHW Gas Consumption`. Off while both settings are 0.
- **Heating curve self-tune, on by default** (TASK-1525, ADR-205). SAT evaluates every clean hour (SAT in control, no hot water, no PWM, flame on, sun low) and logs the result. Once a day, when the last 3 counted days show the curve is too low or too high by more than 1.0 C, it moves the heating curve coefficient by 0.1, clamped to 0.1 to 5.0. Switch it off under Settings, SAT, Curve: "Heating curve self-tune" (`satcurveselftune`) to keep your coefficient fixed. A manual change of the coefficient restarts the evaluation. Each hour and each daily decision is logged (`SATTUNEH` / `SATTUNED` in the SAT log). See Known issues: the multi-day run is still to be done.
- **Fireplace mode** (TASK-1374): a new switch (`satfireplace`, default off; Home Assistant switch `SAT Fireplace Mode`). While it is on, SAT heats on the heating curve only, as when the room temperature is missing, so a stove heating the room with the sensor no longer stops heating for the rest of the house.
- **Hybrid systems use the heat-pump PWM cycle** (TASK-1531). The PWM card on the SAT page shows only the cycle setting of the selected appliance type (TASK-1529); Max modulation is edited only on the Central Heating Settings card (TASK-1530).
- **A heat pump gets the configured max modulation outside PWM too** (TASK-1426).
- **Summer simmer cutoff leaves PWM cleanly**, as the cold cutoff does (TASK-1362).
- **PID values are rounded to 3 decimals**, and a Ki of 0 still clamps the integral (TASK-1423). The PID integral and derivative reset on a real target change, a preset change, the window switch and SAT off/on (TASK-1414); a target change of exactly 0.05 C counts (TASK-1517).
- **`cycles_this_hour` no longer stops at 6** (TASK-1425).
- **Zone diagnostics publish only on change** (TASK-1465), which cuts MQTT traffic.
- **Heating curve coefficient range is 0.1 to 5.0** in the REST metadata and in the setup wizard slider (TASK-1359).
- **SAT simulation** (bench only) now models a normal 24 kW combi boiler with water mass and emitter output (TASK-1532, TASK-1535).
- **SAT hands the boiler back when OpenTherm is lost** (TASK-1365). After 30 s without a boiler reply it sends `CS=0`, `CH=1`, `MM=T` once, then waits until the boiler answers again.
- **Follow thermostat after a reboot** (TASK-1520): with push setpoint on, SAT sends its target to the thermostat at the restart decision (at most 60 s after boot), so a dial change made while SAT waits for a boiler reply and an outside temperature is no longer lost. A change in the first 60 s after boot is still only recorded.
- **SAT log you can follow** (TASK-1521): telnet key 5 is now a quiet SAT log with one `SATTICK` line per control step and `SATBOOT` lines for the restart; key 8 is a separate high-volume trace, off by default. The SAT log in the web UI says why SAT changed mode, pushed a setpoint, cut heating at the cold setpoint or hit the maximum. Field format: `docs/sat/sat-log-format.md`; `scripts/sat_log_to_csv.py` turns a capture into CSV.
- **Earlier smaller changes:** the PID steps on its own 60 s timer (TASK-1368); the PWM hourly start limit counts only PWM's own ON phases, and `cycles_this_hour` now shows only those; PWM switches off at the cold cutoff and when all valves close (TASK-1327); CH is 0 below the cold setpoint (TASK-1316); the overshoot margin default is 2.0 C (was 3.0 C, stored values are kept); the Continuous/PWM buttons on the SAT page are gone (the mode is shown, SAT decides); a SAT preset and the window switch survive a reboot (TASK-1487); a setpoint change on the thermostat becomes the SAT target (setting `satfollowthermostat`, TASK-1259).

---

## Other changes since the last alpha

- **Web UI:** the GPIO output and webhook trigger-bit selects offer all 16 status bits by their OpenTherm name (TASK-1358). The Monitor log with auto-scroll off keeps the lines you are reading in place (TASK-1533). The `/update` page links back to the web UI (TASK-1534).
- **Reverse proxy:** the File Explorer reboot redirects and helper forms use page-relative links, so they also work behind a proxy prefix such as the Home Assistant panel (TASK-1445).
- **Uptime** comes from the 64-bit boot clock and no longer wraps (TASK-1449).
- **Flash tool:** `flash_otgw` also finds a fresh S3 that enumerates as USB id 303A:4001 (TASK-1394), and `--migrate` checks the restored settings key by key, so new keys and number precision no longer report an error (TASK-1388).
- **Home Assistant:** `sat/weather/snowfall` is announced in cm, the unit Open-Meteo sends (TASK-1464). See the breaking changes.

---

## Before you flash

**1. Save your settings.** Download `settings.ini` from the file explorer, or save the output of `GET /api/v2/settings`:

```
curl http://<ip>/api/v2/settings -o settings-backup.json
```

**2. Choose the route that fits your unit.**

- Unit already runs a 2.0.0 alpha with the safeboot layout: update over WiFi. Open `/update`, give it the release zip and press **Update**. The page runs the whole sequence: firmware through the safeboot updater, then the filesystem. Settings are kept.
- Unit on the old partition layout (older alpha or 1.x): it needs **one** USB flash first with `flash_otgw --migrate`. The tool saves your settings, writes the new layout, keeps your WiFi credentials and restores the settings. Windows: `flash_otgw.bat --migrate --host <ip>`. Linux/macOS: `./flash_otgw.sh --migrate --host <ip>`. See the README and `docs/guides/WIFI_FIRMWARE_UPDATE.md`.
- Coming from 1.x: this is a different firmware line. Settings and the web UI differ. Read the breaking changes below.

**3. Interrupted filesystem upload.** If the filesystem upload stops part-way, the settings are no longer lost. The gateway wipes the filesystem, writes your settings back from memory and asks you to upload the filesystem again. Until you do, it serves a fallback page with the file explorer upload (`device/info` reports `fsupdateincomplete`).

**4. Do not flash the PIC over WiFi with OTmonitor.** That has not changed.

---

## Highlights

- **v2 web UI.** The only web UI in 2.0.0. Live value table, help text for every setting (the **i** button), settings rail with card panels, SAT page.
- **Firmware updates over WiFi.** A small safeboot updater next to the firmware. If an upload fails or a new firmware crashes in its first 30 seconds, the unit returns to safeboot and stays reachable. One-action update page for the release zip.
- **Monitor graph with history.** The graph opens with the last hour the gateway kept (24 hours on boards with PSRAM). The control setpoint line now shows what the boiler got, also when the thermostat briefly asks something else that the gateway replaces (TASK-1522).
- **Works behind a reverse proxy**, including a Home Assistant sidebar panel (ingress). Guide: `docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md`.
- **Home Assistant.** A "Visit" link to the gateway, SAT power and energy sensors for the Energy dashboard, a cumulative hot water total (`dhw_water_total`), and a "Command Dropped" sensor when the PIC never confirms a command.
- **Return water temperature every minute** from a wired sensor (PIC firmware 6.6 or later, `TS=R`).
- **Crash log endpoint.** `GET /api/v2/device/crashlog` for diagnostics. After an ESP32 panic or watchdog reset it now holds a `Core dump:` line with the task, the program counter and a short backtrace (TASK-1479). `reboot_log.txt` names every ESP-IDF reset reason, including 0xb (USB reset by the host) (TASK-1438).
- **Capture tool** with raw OpenTherm frame logging (next section).
- **Relicensed to GNU GPLv3** (was MIT).

---

## Capture tool

One Python tool does the diagnostic capture. Two launchers start it:

- Windows: `capture-mqtt-debug.bat`
- Linux and macOS: `capture-mqtt-debug.sh`

If no Python 3.9 or newer is installed, the launcher downloads a portable Python once and checks its SHA-256. You do not need to install anything.

Every capture merges telnet debug, MQTT, browser devtools, the crash log and HTTP probes into one transcript. It also writes two extra files:

- `otframes.log`: raw OpenTherm frames with timing.
- `otgw_simulation.log`: the same frames in the format the replay can read, so we can replay your boiler on the bench.

Example (use your own addresses):

```
capture-mqtt-debug.bat --device 192.168.1.50 --broker 192.168.1.10 --duration 3600
./capture-mqtt-debug.sh --device 192.168.1.50 --broker 192.168.1.10 --duration 3600
```

Without arguments the tool asks for the device address. `--help` lists the options. Unzip the whole flash zip first: the launcher needs `otgw_capture.py` next to it.

**Capturing for days** (TASK-1523): add `--long-run`, or give a `--duration` above 2 hours (3 days: `--duration 259200`). The tool then keeps one folder per hour and zips every finished hour, with CSV files of that hour (SAT records, OpenTherm frames, MQTT). Every disconnect (device reboot, WiFi drop, broker restart) is marked with its start, end and reason. Every 5 minutes it saves the device status. It stops cleanly when the disk gets full. About 6 MB per hour before zipping and well under 1 MB zipped on our bench. Keep the PC awake.

**What to post:** at the end the tool prints `Upload this one file: ...-upload.zip`. That single file holds everything (for a short run: the transcript, `otframes.log`, `otgw_simulation.log` and the CSV files). Post it in Discord #alpha-testing or attach it to a GitHub issue. Check the files before you share them: they contain your device address and your MQTT topics. Leave the broker out if you do not want it in the log.

The Windows launcher is tested on our bench. The Linux/macOS launcher has not yet been run by a macOS tester; if you run it on a Mac, please tell us whether it worked.

---

## BREAKING changes since the 1.x line

The wording below is quoted from the CHANGELOG. Check your Home Assistant automations, Node-RED flows and scripts against this list.

### MQTT and API

- **SAT PWM cycles per hour is 3 or 4 only, default 3** (TASK-1375). Gas boilers and hybrids, radiators and underfloor. A stored 2 becomes 3 and a stored 5 or 6 becomes 4, also when written over REST or MQTT; a settings file is corrected on the next settings write. Heat pumps are unchanged. Do: check the value on the Settings page.
- **`sat/max_setpoint` changed meaning** (TASK-1373). It and `max_setpoint_system` in the SAT status now report the max flow setpoint ceiling, not the fixed 62 / 45 / 40 table. The underfloor default is 50 C (was 45 C). The SAT status gains `max_setpoint_user`. A value above 65 is still cut to 65 unless you chose it on the slider (TASK-1513); a value from 1 to 19 is refused. Do: check automations that read `sat/max_setpoint`.
- **`cycles_this_hour`** (REST, MQTT, SAT page) now counts PWM ON-phase starts only. It reads 0 in continuous mode.
- **SAT MQTT contract cleanup.** "Home Assistant automations or templates that read these topics need an update":
  - `sat/heating_curve_recommendation` is all lower case: `increase` / `decrease` / `hold` / `insufficient` (was `INCREASE` / `DECREASE` / `HOLD` / `insufficient`). The REST/WS field `heating_curve_recommendation` changes with it. `sat/curve_recommendation` is unchanged.
  - `sat/ssi` is removed; read `sat/summer_simmer_index` (same value, one decimal). The firmware empties the retained `sat/ssi` topic on every SAT discovery pass.
  - `sat/pressure_health` is ON on a pressure problem (was: ON = healthy), as thermo-nova and the other `*_health` topics.
  - `sat/device_health` is ON while the boiler status has not been evaluated yet since boot, no longer described as "boiler off". The payloads on the wire do not change.
  - The MQTT command `set/<node>/sat/heating_curve` is renamed `set/<node>/sat/heating_curve_coeff`, without an alias: the old command is ignored. The state topic `sat/heating_curve` stays the computed curve value. The REST setting name follows (TASK-1496): `POST /api/v2/sat/settings/heating_curve_coeff`, and `heating_curve` is no longer accepted there (404, no alias). The REST `detail=full` health JSON follows the MQTT contract too: `flame_health`, `device_health` and `cycle_health` are true = problem with the MQTT meaning (they were true = healthy, with other meanings), and `flame_health` is `null` while the flame status is insufficient data. Update scripts that read them or post the old setting name.
- **Home Assistant availability.** "HA entity availability (`avty_t` = the base `<toptopic>/value/<nodeid>` topic) now reflects only the ESP↔MQTT link (birth/LWT) instead of OpenTherm-bus liveness ... consumers that read the base topic as OT-bus liveness must migrate to the `otgw_connected` sensor."
- **MQTT on-change publishing is now the default.** "changed OpenTherm values publish immediately, unchanged values refresh once per minute." A config that still has `MQTTinterval=0` is migrated once to `60`. Untick "MQTT Publish On-Change" to restore publish-every-message.
- **MQTT `resetgateway` command** now requires payload `"1"` and is rate-limited to one PIC reset per 5 seconds.
- **Base topic publishes thermostat-side intent only.** "slave Write-Ack values no longer flow to the base topic" (ADR-066). The `/boiler` subtopic is gated per MsgID.
- **`settings.sat.iBleInterval`** changed meaning: "Repurposed from 'BLE scan rate' to 'publish/state-update cadence'."
- **Home Assistant PV-boost switch and numbers** now use the topics `sat/pv_boost_*` (they pointed at `sat/boost_*` before). Entity ids are unchanged. Run discovery again (or restart Home Assistant) to pick it up.
- **`GET /api/v2/simulate`** now always reports `available: true` and has a new `source` field (`file` or `builtin`); "the `reason` field and the 409 for an unmounted filesystem are gone."
- **`GET /api/v2/sat/weather/needs-setup`** no longer returns `has_key`, and `GET /api/v2/settings` no longer lists `SATweatherapikey`.
- **While SAT is enabled it owns the control setpoint.** "a `CS=` from MQTT, REST or the serial bridge is refused."
- **`TP=` commands are no longer replaced in the command queue.** Each `TP=` now waits its turn.
- **The TCP serial bridge on port 25238 is off by default.** OTmonitor and the Home Assistant `opentherm_gw` integration connect to it. Do: switch on "Legacy TCP port 25238" in the OTGW settings if you use either of them.
- **`sat/weather/snowfall` is in cm** (TASK-1464). It was announced in mm while Open-Meteo sends cm; the value itself did not change. Home Assistant may report a unit change for the long-term statistics of this sensor.
- **Heating curve self-tune is on by default** (TASK-1525). On a unit with SAT enabled, the heating curve coefficient can now change by 0.1 at most once a day. Do: switch "Heating curve self-tune" off if you want to keep your coefficient fixed.

### Settings and SAT features removed

- **PID deadband.** "The setting `satdeadband` (`SATdeadband` in settings.ini), the REST settings field and its `settings.sat.deadband` / SAT status `deadband` read-outs, the MQTT command and state topic `sat/deadband` and the v2 Settings control are removed." The deadband is a fixed 0.1 °C.
- **Force PWM.** "The setting `satforcepwm`, the REST field `force_pwm`, the MQTT command `sat/force_pwm`, the state topic `sat/force_pwm_enable` and the Home Assistant switch `SAT Force PWM` are removed."
- **PWM auto switch.** "The setting `satpwmautoswitch`, the REST field `pwm_auto_switch`, the MQTT command `sat/pwm_auto_switch`, the state topic `sat/pwm_auto_switch_enable` and the Home Assistant switch `SAT PWM Auto Switch` are removed."
- **Humidity comfort adjust.** "The settings `satcomfortadjust`, `satcomforthumidity` and `satcomfortmaxoffset`, the REST/MQTT fields and commands `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `comfort_offset`, `comfort_ref_humidity` and the Home Assistant switch `SAT_Comfort_Adjust` are removed."
- **Hourly heating curve auto-tune** (TASK-1413). The firmware-only auto-tune that rewrote the coefficient and the PID gains every hour is removed. Gone: the settings `satautotune` and `satautotunerate`, the REST fields `auto_tune*`, the MQTT topics `sat/auto_tune*` and the Home Assistant switch `SAT Auto Tune`. The firmware empties the retained discovery and state topics. The daily self-tune above replaces it.
- **Auto-gain setting.** "`satautogains` (`auto_gains_value`) ... and the manual-gain switch (`auto_gains`, `kp_manual`, `ki_manual`, `kd_manual`) ... are removed from the settings, REST, MQTT, the Web UI and the onboarding wizard."
- **OpenWeatherMap** and the `SATweatherapikey` setting. "Open-Meteo is now the only weather source."
- Old `settings.ini` files that hold these keys still load. The keys are ignored and dropped at the next save.

### Web UI and platform

- **ESP8266 is dropped.** 2.0.0 is ESP32-S3 only.
- **The classic web UI is removed; v2 is the only web UI** (ADR-192). The UI switch and the classic-only `ui_*` settings are gone. The File Explorer is the recovery page.
- **Classic UI features not in v2:** the UI switch button; the heap footer; log persistence across page reloads; resizable table columns; the 8-option graph window list; graph disconnect markers; auto-hiding the log on small screens; the `otgwDebug` console helper; the design showcase page; WiFi network scan; the classic display preferences; weather location auto-fill and the OpenWeatherMap key wizard.
- **Default hostname is now `OTGW2`.** A unit with an existing `settings.ini` keeps its saved hostname. The new default applies to new installs and to any reset that discards `settings.ini`. A fresh install is reachable at `http://otgw2.local/`.
- **New flash layout on 4 MB boards** (ADR-191): safeboot 960 KB, firmware 2240 KB, filesystem 768 KB. "Every unit needs one USB migration."
- **GPIO output and webhook trigger-bit labels** were the wrong way round in the v2 settings page. Stored values are unchanged, so check the select after the update.
- **Relicensed from MIT to GNU GPLv3.**

---

## Known issues

- **An undecodable crash** was seen once on our bench (TASK-1498): a core dump with the program counter in IRAM, and no matching ELF file to decode it. It was seen once and is not reproduced. If you see a crash, the capture tool grabs the crash log.
- **No 24-hour soak test for this build.** The planned 24-hour run was interrupted every time; the longest uninterrupted run was 224 minutes (alpha.577). This beta ships without it, by maintainer decision. On the day of release the build was smoke-tested on two units, one with a PIC: update over WiFi with settings kept, no reboot, web UI, REST and MQTT working. Long-run stability is exactly what we need your field data for.
- **One crash seen in the field** (alpha.566, TASK-1538): a panic inside a memory allocation, with a backtrace too short to name the caller. Not reproduced. If your unit reboots unexpectedly, send the crash log (`GET /api/v2/device/crashlog`) and `reboot_log.txt`.
- **Free memory dips under heavy diagnostic load** (TASK-1528). During long capture runs the lowest free-heap mark fell to between 0.5 and 1.7 KB for a moment, while the free heap stays around 71 KB. No crash was linked to it. Under investigation.
- **Heating curve self-tune has not run for several days yet** (TASK-1525). Its host tests pass and it counted a clean hour on the bench simulation; a multi-day run that shows a coefficient change is still to be done. Switch it off if you want a fixed coefficient.
- **SAT is still being validated.** Some SAT decisions are open and follow after this beta. SAT behaviour can change between builds. The ignition logic for a boiler without a return temperature (TASK-1371) was bench-tested; it has not run on such a boiler in the field.
- **Pressure alarm:** the SAT pressure drop alarm follows thermo-nova's rule (TASK-1480). A pump-head step of 0.3 bar after the pump stops can still raise it. It is display only and sends no command.
- **macOS capture:** not verified by a macOS tester yet.
- **Updating through the Home Assistant panel** completed in 2 of 3 bench runs (one 502 during the upload). Update by IP when you can.
- The first install of the release that introduces the one-action update page is still the two-step flow, because the unit serves the page before the update.
- **Filesystem update while a capture runs** (TASK-1524): on our bench a filesystem update failed once while a long capture was connected, and the unit served the file explorer upload page until a second update. Settings were kept. Stop the capture before you update.

---

## How to report

- Discord: #alpha-testing, with the capture folder.
- GitHub: open an issue and attach the capture files.

Thank you to everyone who tested the alphas, and to gruijter for the detailed feedback.
