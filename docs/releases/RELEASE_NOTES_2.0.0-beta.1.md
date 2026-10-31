# OTGW-firmware v2.0.0-beta.1 Release Notes (DRAFT)

<!-- TODO(maintainer): final version name. Proposal: v2.0.0-beta.1. No v2.* tag or GitHub release exists yet. -->
<!-- TODO(maintainer): release date. Target: Sunday 2026-11-01. -->

**Release date:** [TODO: 2026-11-01?]
**Branch:** dev
**Compare:** there is no earlier 2.x release. Everything since the 1.x line is new to you.

## What this beta is

This is the first public beta of the 2.0.0 line. 2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported.

The purpose of this beta is field data. We want more people to run it on real heating systems and tell us what happens.

**Supported hardware for this beta:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place.

This is a beta. Expect rough edges. Read "Before you flash" and "Known issues" first.

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
- **Gas consumption in m3/h** (TASK-1484): two new settings, `satminconsumption` and `satmaxconsumption` (0 to 8), make `sat/consumption` real. Off while both are 0. No Home Assistant entity yet.
- **SAT hands the boiler back when OpenTherm is lost** (TASK-1365). After 30 s without a boiler reply it sends `CS=0`, `CH=1`, `MM=T` once, then waits until the boiler answers again.
- **Smaller changes:** the PID steps on its own 60 s timer (TASK-1368); the PWM hourly start limit counts only PWM's own ON phases, and `cycles_this_hour` now shows only those; PWM switches off at the cold cutoff and when all valves close (TASK-1327); CH is 0 below the cold setpoint (TASK-1316); the overshoot margin default is 2.0 C (was 3.0 C, stored values are kept); the Continuous/PWM buttons on the SAT page are gone (the mode is shown, SAT decides); a SAT preset and the window switch survive a reboot (TASK-1487); a setpoint change on the thermostat becomes the SAT target (setting `satfollowthermostat`, TASK-1259).

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
- **Monitor graph with history.** The graph opens with the last hour the gateway kept (24 hours on boards with PSRAM).
- **Works behind a reverse proxy**, including a Home Assistant sidebar panel (ingress). Guide: `docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md`.
- **Home Assistant.** A "Visit" link to the gateway, SAT power and energy sensors for the Energy dashboard, a cumulative hot water total (`dhw_water_total`), and a "Command Dropped" sensor when the PIC never confirms a command.
- **Return water temperature every minute** from a wired sensor (PIC firmware 6.6 or later, `TS=R`).
- **Crash log endpoint.** `GET /api/v2/device/crashlog` for diagnostics. <!-- TODO(maintainer): the CHANGELOG [Unreleased] does not mention core dumps. Add them here only if they ship in this build. -->
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

**What to post:** the capture folder under `logs/mqtt-diagnostics` (the transcript, `otframes.log` and `otgw_simulation.log`). Post it in Discord #alpha-testing or attach it to a GitHub issue. Check the files before you share them: they contain your device address and your MQTT topics. Leave the broker out if you do not want it in the log.

[macOS: verified by <name> / not verified] <!-- TODO(maintainer): fill in after the tester run. -->

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

### Settings and SAT features removed

- **PID deadband.** "The setting `satdeadband` (`SATdeadband` in settings.ini), the REST settings field and its `settings.sat.deadband` / SAT status `deadband` read-outs, the MQTT command and state topic `sat/deadband` and the v2 Settings control are removed." The deadband is a fixed 0.1 °C.
- **Force PWM.** "The setting `satforcepwm`, the REST field `force_pwm`, the MQTT command `sat/force_pwm`, the state topic `sat/force_pwm_enable` and the Home Assistant switch `SAT Force PWM` are removed."
- **PWM auto switch.** "The setting `satpwmautoswitch`, the REST field `pwm_auto_switch`, the MQTT command `sat/pwm_auto_switch`, the state topic `sat/pwm_auto_switch_enable` and the Home Assistant switch `SAT PWM Auto Switch` are removed."
- **Humidity comfort adjust.** "The settings `satcomfortadjust`, `satcomforthumidity` and `satcomfortmaxoffset`, the REST/MQTT fields and commands `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `comfort_offset`, `comfort_ref_humidity` and the Home Assistant switch `SAT_Comfort_Adjust` are removed."
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

<!-- TODO(maintainer): not verified, taken from the older RELEASE_NOTES_2.0.0.md, not from the CHANGELOG [Unreleased]: (a) the legacy otmonitor TCP bridge on port 25238 being opt-in, (b) the OT-bus presence topics moved to generic topics (ADR-084). Confirm against the build before listing them as breaking. -->

---

## Known issues

- **An undecodable crash** was seen once on our bench (TASK-1498): a core dump with the program counter in IRAM, and no matching ELF file to decode it. It was seen once and is not reproduced. If you see a crash, the capture tool grabs the crash log.
- **Unexplained reboots** were seen on an older alpha (alpha.519, TASK-1438): three reboots in 90 s with reason "Unknown (b)" and no crash log, during a replay run on the bench. The cause is not explained. Send the crash log and `reboot_log.txt` if it happens to you.
- **SAT is still being validated.** Some SAT decisions are open and follow after this beta. SAT behaviour can change between builds. The ignition logic for a boiler without a return temperature (TASK-1371) was bench-tested; it has not run on such a boiler in the field.
- **Pressure alarm:** the SAT pressure drop alarm follows thermo-nova's rule (TASK-1480). A pump-head step of 0.3 bar after the pump stops can still raise it. It is display only and sends no command.
- **macOS capture:** [macOS: verified by <name> / not verified] <!-- TODO(maintainer) -->
- **Updating through the Home Assistant panel** completed in 2 of 3 bench runs (one 502 during the upload). Update by IP when you can.
- The first install of the release that introduces the one-action update page is still the two-step flow, because the unit serves the page before the update.

<!-- TODO(maintainer): add anything the 24 h soak shows, and any item you could not verify. -->

---

## How to report

- Discord: #alpha-testing, with the capture folder.
- GitHub: open an issue and attach the capture files.

Thank you to everyone who tested the alphas, and to gruijter for the detailed feedback.
