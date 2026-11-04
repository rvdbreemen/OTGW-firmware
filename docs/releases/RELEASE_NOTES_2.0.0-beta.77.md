# OTGW-firmware v2.0.0-beta.77 Release Notes

**Release date:** 2026-11-04
**Branch:** dev
**Build:** 2.0.0-beta.77 (the githash is in the asset names)
**Compare:** [v2.0.0-beta.62...v2.0.0-beta.77](https://github.com/rvdbreemen/OTGW-firmware/compare/v2.0.0-beta.62...v2.0.0-beta.77). "What changed since beta.62" is new for a beta.62 user. Coming from an older beta: the [beta.62 notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.62.md) hold the changes from beta.1 to beta.62 in full. Coming from 1.x: everything is new to you, read the whole document and the beta.62 notes.

> **Disclaimer: this is a beta of 2.0.0.** It is not a stable release. It has had alpha and beta testing on a small group of units, but not the long field runs a release needs. What was and was not tested for this build:
>
> - beta.77 ran a 1 hour bench soak with MQTT connected, on one OTGW Classic with an S3 Mini (PIC attached). One hour is the agreed minimum for a beta (maintainer decision 2026-11-03): slow problems are for the field testers to find. That is not a 24-hour soak test, and no macOS capture run was done (see Known issues).
> - Each change from beta.63 to beta.76 was host-tested (red on the old code, green on the fix) and bench-tested on its own build before that build was posted; the crash fix (TASK-1605) was bench-tested with the internet blocked for 25 minutes (before: a restart every 5 minutes; after: none).
> - **No real Immergas boiler was tested**, and the OT-Direct change for a thermostat in gateway mode (TASK-1600) has not been seen on a bench with a thermostat attached.
>
> Expect bugs, and keep a way back: save your settings, and keep the flash zip of the 2.0.0 build you run now, so you can flash it back over USB. A 1.x zip is ESP8266 firmware and does not run on an S3 Mini; going back to 1.x means putting the Wemos D1 mini back in. If you need a gateway that just works, stay on the [1.x releases](https://github.com/rvdbreemen/OTGW-firmware/releases) for now.

## What this beta is

A public beta of the 2.0.0 line. 2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported.

The purpose of this beta is field data. We want more people to run it on real heating systems and tell us what happens.

**Supported hardware for this beta:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place.

Read "Before you flash", "BREAKING changes" and "Known issues" first.

---

> **Update recommended for beta.62 users on a network without internet access.** On a LAN where the internet is blocked (the local DNS server still answering), beta.62 can restart every few minutes when the SAT weather forecast or an MQTT broker given by host name is used. beta.77 fixes this (TASK-1605).

## What changed since beta.62

Fifteen builds (beta.63 to beta.77, the last one is the release commit). The most important fix comes first.

### Fixes

- **A gateway without internet access could restart every few minutes; it no longer does** (TASK-1605). With the internet blocked but the local DNS server still answering, a bench unit crashed every 5 minutes. The first hostname lookup after boot (the SAT weather forecast, an MQTT broker given by name, the PIC update check, a webhook given by name) cleared the DNS cache without the network stack's lock, and a pending lookup of the time server then stopped the firmware. Hostnames are now resolved in a way that takes the lock and never clears the cache, and each HTTP request connects to the resolved address (the `Host:` header still carries the name). A failed lookup still waits for the DNS timeouts, about 7 s per configured DNS server. **Do:** nothing. If your unit restarted regularly while the internet was blocked or flaky, this is the fix to try.
- **The SAT weather fetch no longer stalls the main loop, and the PIC refresh stops after a failed update check** (TASK-1604). With the internet blocked, the weather request blocked the main loop for the DNS lookup plus a 5 s connect timeout, at 5, 6, 8, 12 and 20 minutes after boot. It now runs on its own short-lived task with a 2 s connect timeout. The PIC firmware refresh no longer sends a second request after a failed version check, which could stall the loop up to the 30 s watchdog.
- **SAT: all state topics reach Home Assistant again right after its restart** (TASK-1603). About 55 SAT topics (`window_open`, `pressure_drop_rate`, `requested_setpoint` and more) showed `unknown` in HA for up to about 5 minutes after HA came back. They are now sent again on the first pass after HA announces itself online. The topics stay non-retained.
- **SAT: the unchanged `MM=` / `CH=` (and Immergas `TP=`) repair resend goes out once a minute, not every 90 s** (TASK-1602).
- **PIC command queue: a client that repeats a command faster than its delay is no longer starved** (TASK-1571). Before, 120 times `PR=A` every 200 ms produced one send in total. The same command now reaches the PIC about once per second, and `PR=A` and `PR=B` both go out.
- **The PIC-flash progress LEDs light up during a PIC flash on the ESP32-S3 boards** (TASK-1218). They never changed before, because the firmware drove pins that are attached to the LED controller with a call the framework refuses.
- **Forced OT-Direct on an S3 Mini Pro resets the PIC on GPIO 40, not GPIO 12** (TASK-1402). GPIO 12 is the I2C bus of the Pro. Other modes are unchanged.
- **OT-Direct gateway mode: a held `CS=` override also sets the CH enable bit of the thermostat's relayed status** (TASK-1600, ADR-208). SAT's `CH=0` (cold cutoff, PWM OFF phase) is now honoured with a thermostat attached. Without a `CS=` override nothing changes. Not yet seen on a bench with a thermostat.
- **Webhook URLs are checked more strictly** (TASK-1605). A URL with port 0, a port that is not a number or above 65535, or a host that cannot be resolved is refused, with one log line. The webhook connects to exactly the address its local-network check approved.

### New

- **Download the stored core dump over REST** (TASK-1560, ADR-210). `GET /api/v2/device/coredump` streams the ESP32 core dump from flash, so a field tester can send it without a USB cable. It is an admin route (HTTP Basic Auth plus the same-origin check; open while no admin password is set) and answers 404 when no valid dump is stored. The file is named `otgw-coredump-app-<first 9 hex of the app ELF SHA256>.bin`, and the header `X-Coredump-App-Sha256` carries the same prefix: the stored dump is usually from an older build than the one that runs, and the matching `.elf` is the one whose `sha256sum` starts with that prefix. The capture tool fetches the dump at the end of a capture into `coredump-<run>.bin` (skip with `--skip-coredump`).
- **Replay follows the recorded timing** (TASK-1508, ADR-209). A simulation file in the `otframes.log` layout is replayed with the recorded gaps between frames (clamped to 0 to 60 s) instead of every 750 ms. Bare-frame files and the built-in fixture keep 750 ms. `GET /api/v2/simulate` reports `timing` (`timed` or `fixed`). This is a bench and diagnostics feature.
- **PIC boards: the start-up `PM=3/15/48` chain runs at every boot, also with SAT disabled** (TASK-1384). The low-load permission (MsgID 3) is then known when SAT is enabled later.

### SAT (opt-in; only when SAT is on)

These follow decisions of Sergeant D and of the maintainer. They change what SAT reports and, in the first two cases below, what it does.

- **Cold cutoff: no phantom PWM start** (TASK-1515). While the cold cutoff is latched, the PWM schedule is skipped, so it can no longer count a start that then blocks a real one.
- **First minutes without a return temperature: the flame-off hold keeps the previous `CS=`** (TASK-1412.12). Before, it dropped to the cold setpoint (28.2 or 21) on every boiler.
- **Telemetry follows the wire** (TASK-1412.15, entries 266 to 269):
  - A flame that already burns when SAT starts has an unknown first off gap; it is left out of the 4 h average off gap and the duty average.
  - `mod_suppressed` is true while SAT suppresses modulation (the PWM ON phase after the hold). Before, it was always false.
  - The 4 h and 24 h duty is now flame-on seconds divided by the window length, every flame counted (hot water included). SAT running for less than the window still divides by the full window, so the first hours read low.
  - `current_modulation` shows 100 while the last `MM=` sent is `MM=T`.
- **Heating curve rounded to 0.1 C** (TASK-1360). The effect on the requested setpoint is at most 0.05 C.
- **Boiler status `stalled_ignition` and `overshoot_cooling`** (TASK-1356, TASK-1361). `stalled_ignition` forgets a last cycle older than 6 h. `overshoot_cooling` is latched at flame off, as in thermo-nova, and no longer shows in every PWM OFF phase. Display only (`sat/boiler_status`); control does not read it.

**How this was tested.** Host tests on the real code, each failing on the code before the change. Examples: `tests/test_no_hostbyname.py` (old code: the source gate and the weather connect-order cases fail), `tests/test_weather_worker_handoff.py` (old code: all 10 cases fail), `tests/test_replay_timing.py`, `tests/test_otd_status_relay.py` (old code: 4 of 9 fail), `tests/test_sat_pwm_start_limit.py`, `tests/test_sat_no_return_startup.py`, `tests/test_sat_status_display.py` (old code: 2 and 3 failures), `tests/test_pic_pm_chain.py`, `tests/test_pic_rst_pin.py` and `test/host/test_pic_led_ledc.py`. The bench part is described in the disclaimer: one 1 hour soak of beta.76.

---

## SAT is experimental and opt-in

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- While SAT is on, it owns the control setpoint: a `CS=` from MQTT, REST or the serial bridge is refused.
- Some SAT decisions are still open (see Known issues). SAT behaviour can change between builds.
- If you enable SAT, please share a capture log (see "Capture tool"). SAT is only as good as the field data we get.

Without SAT, the gateway behaves as a transparent OpenTherm gateway, as in 1.x.


---

## Before you flash

**1. Save your settings.** Download `settings.ini` from the file explorer, or save the output of `GET /api/v2/settings`:

```
curl http://<ip>/api/v2/settings -o settings-backup.json
```

**2. Choose the route that fits your unit.**

- **A new S3 Mini, or a unit on 1.x or on an old partition layout, needs a USB first flash with `flash_otgw`.** For a unit that holds settings the tool saves them, writes the new layout, keeps your WiFi credentials and restores the settings. Windows: `flash_otgw.bat --migrate --host <ip>`. Linux/macOS: `./flash_otgw.sh --migrate --host <ip>`. See the README and the [WiFi firmware update guide](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/guides/WIFI_FIRMWARE_UPDATE.md).
- **A unit that already runs 2.0.0** (beta.1 or later, or a 2.0.0 alpha with the safeboot layout) **updates over WiFi with the flash zip.** Open `/update`, give it the release zip and press **Update**. The page runs the whole sequence: firmware through the safeboot updater, then the filesystem. Settings are kept.
- Coming from 1.x: this is a different firmware line. Settings and the web UI differ. Read the breaking changes below.

**3. Interrupted filesystem upload.** If the filesystem upload stops part-way, the settings are no longer lost. The gateway wipes the filesystem, writes your settings back from memory and asks you to upload the filesystem again. Until you do, it serves a fallback page with the file explorer upload (`device/info` reports `fsupdateincomplete`).

**4. Do not flash the PIC over WiFi with OTmonitor.** That has not changed.

**5. OTmonitor or the Home Assistant `opentherm_gw` integration?** They connect to the TCP serial bridge on port 25238, which is **off by default (opt-in)**, as in 1.x. Switch on "Legacy TCP port 25238" in the OTGW settings if you use either of them.

---

## Highlights

- **v2 web UI.** The only web UI in 2.0.0. Live value table, help text for every setting (the **i** button), settings rail with card panels, SAT page.
- **Firmware updates over WiFi.** A small safeboot updater next to the firmware. If an upload fails or a new firmware crashes in its first 30 seconds, the unit returns to safeboot and stays reachable. One-action update page for the release zip.
- **Monitor graph with history.** The graph opens with the last hour the gateway kept (24 hours on boards with PSRAM). The control setpoint line shows what the boiler got, also when the thermostat briefly asks something else that the gateway replaces (TASK-1522).
- **Works behind a reverse proxy**, including a Home Assistant sidebar panel (ingress). Guide: [Reverse proxy and HA ingress](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md).
- **Home Assistant.** A "Visit" link to the gateway, SAT power and energy sensors for the Energy dashboard, a cumulative hot water total (`dhw_water_total`), and a "Command Dropped" sensor when the PIC never confirms a command.
- **Return water temperature every minute** from a wired sensor (PIC firmware 6.6 or later, `TS=R`).
- **Crash log endpoint.** `GET /api/v2/device/crashlog` for diagnostics. After an ESP32 panic or watchdog reset it holds a `Core dump:` line with the task, the program counter and a short backtrace (TASK-1479). `reboot_log.txt` names every ESP-IDF reset reason, including 0xb (USB reset by the host) (TASK-1438).
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
## BREAKING changes

### New since beta.62

No MQTT topic name or existing REST route is renamed or removed. These changes alter values or behaviour that automations may read:

- **BREAKING for readers of the SAT duty values: the 4 h and 24 h duty definition changed** (TASK-1412.15, entry 268). The REST fields `duty_ratio` and `24h_duty_ratio` (and the MQTT values that mirror them) are now flame-on seconds divided by the window length. Before: on / (on + off gap) over the cycles. Values drop, mostly in the first hours after SAT starts. **Do:** re-check automations, dashboards and thresholds that read the duty.
- **`mod_suppressed` is now real** (entry 267). It was always false; it turns true during the PWM ON phase suppression. **Do:** check automations that assumed it never changes.
- **`current_modulation` reads 100 while the last `MM=` sent is `MM=T`** (entry 269). It used to read 0 there. This matters for graphs and statistics of that value.
- **Webhook: a URL with port 0, a malformed port or an unresolvable host is refused** (TASK-1605). Such a URL was passed on to the HTTP client before. **Do:** if a webhook stopped firing, check its URL under Settings.
- **New route `GET /api/v2/device/coredump`** (TASK-1560). Additive, not breaking. It is an admin route: with an admin password set, it asks for it.

### Carried over from earlier betas

The items below were announced in earlier notes and still apply if you skipped those builds.

#### From beta.60

- **`sat/ble_temp_valid` is a binary sensor in Home Assistant** (TASK-1462). The old sensor entity is removed (its retained discovery config is cleared) and a binary sensor with a new entity id appears. **Do:** point automations and dashboards at the new entity.

#### From beta.38

- **A form-encoded POST now gets `400 Missing value`** (TASK-1565). That is `Content-Type: application/x-www-form-urlencoded` (curl `-d` without a `-H` header, an HTML form, many quick scripts) or a `text/plain` body that looks like `key=value`. Before, such a POST that followed a JSON POST could read the body of that earlier request and apply it. Now the body is released when its request ends, and a form-encoded call has no body to read. Raw `application/json` and plain `text/plain` bodies work as before. **Do:** check scripts, Node-RED flows and Home Assistant `rest_command` entries that POST to `/api/v2/`: send `Content-Type: application/json` and a JSON body, with curl that is `-H "Content-Type: application/json"` together with `-d`.
- **`MaxRelModLevelSetting/override` changes unit in Home Assistant** (TASK-1389). It now reports the applied `MM=` value in percent (device class temperature and `°C` are gone). The entity changes unit, so its long-term statistics history may reset. **Do:** check automations that read this topic; the value was `0.00` before.
- **`sat/cycle_class` no longer reports `insufficient` for a 1 or 2 second flame** (TASK-1588). Such a flame is not recorded as a cycle, so the cycle count, the last cycle values and the 4 h / 24 h windows stay as they were. **Do:** check automations that react to `insufficient`.

#### From beta.11

- **Immergas owners on Auto must now choose Immergas explicitly** (TASK-1537, beta.16). Up to beta.15, SAT sent the Immergas `TP=11:12` command when the Auto detection recognised MemberID 27. From beta.16 it sends it only when the manufacturer setting is "Immergas". Auto still shows the detected name but no longer sends the command, because MemberID 27 is shared with Sime. **Do:** with an Immergas boiler and SAT on, open the SAT settings and set Manufacturer to Immergas. If you leave it on Auto, SAT works as for any other boiler: no `TP=11:12`. Nothing changes for other boilers.
- **SAT no longer caps MM at 80 on an Immergas** (TASK-1537, beta.15). Only the Immergas `TP=11:12` value carries the 80 limit now, and only with Immergas chosen.

### Since the 1.x line

The items below follow the CHANGELOG; quoted parts are in quotation marks. Check your Home Assistant automations, Node-RED flows and scripts against this list.

#### MQTT and API

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
- **`sat/weather/snowfall` is in cm** (TASK-1464). It was announced in mm while Open-Meteo sends cm; the value itself did not change. Home Assistant may report a unit change for the long-term statistics of this sensor.
- **Heating curve self-tune is on by default** (TASK-1525). On a unit with SAT enabled, the heating curve coefficient can now change by 0.1 at most once a day. Do: switch "Heating curve self-tune" off if you want to keep your coefficient fixed.

#### Settings and SAT features removed

- **PID deadband.** "The setting `satdeadband` (`SATdeadband` in settings.ini), the REST settings field and its `settings.sat.deadband` / SAT status `deadband` read-outs, the MQTT command and state topic `sat/deadband` and the v2 Settings control are removed." The deadband is a fixed 0.1 °C.
- **Force PWM.** "The setting `satforcepwm`, the REST field `force_pwm`, the MQTT command `sat/force_pwm`, the state topic `sat/force_pwm_enable` and the Home Assistant switch `SAT Force PWM` are removed."
- **PWM auto switch.** "The setting `satpwmautoswitch`, the REST field `pwm_auto_switch`, the MQTT command `sat/pwm_auto_switch`, the state topic `sat/pwm_auto_switch_enable` and the Home Assistant switch `SAT PWM Auto Switch` are removed."
- **Humidity comfort adjust.** "The settings `satcomfortadjust`, `satcomforthumidity` and `satcomfortmaxoffset`, the REST/MQTT fields and commands `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `comfort_offset`, `comfort_ref_humidity` and the Home Assistant switch `SAT_Comfort_Adjust` are removed."
- **Hourly heating curve auto-tune** (TASK-1413). The firmware-only auto-tune that rewrote the coefficient and the PID gains every hour is removed. Gone: the settings `satautotune` and `satautotunerate`, the REST fields `auto_tune*`, the MQTT topics `sat/auto_tune*` and the Home Assistant switch `SAT Auto Tune`. The firmware empties the retained discovery and state topics. The daily self-tune above replaces it.
- **Auto-gain setting.** "`satautogains` (`auto_gains_value`) ... and the manual-gain switch (`auto_gains`, `kp_manual`, `ki_manual`, `kd_manual`) ... are removed from the settings, REST, MQTT, the Web UI and the onboarding wizard."
- **OpenWeatherMap** and the `SATweatherapikey` setting. "Open-Meteo is now the only weather source."
- Old `settings.ini` files that hold these keys still load. The keys are ignored and dropped at the next save.

#### Web UI and platform

- **ESP8266 is dropped.** 2.0.0 is ESP32-S3 only.
- **The classic web UI is removed; v2 is the only web UI** (ADR-192). The UI switch and the classic-only `ui_*` settings are gone. The File Explorer is the recovery page.
- **Classic UI features not in v2:** the UI switch button; the heap footer; log persistence across page reloads; resizable table columns; the 8-option graph window list; graph disconnect markers; auto-hiding the log on small screens; the `otgwDebug` console helper; the design showcase page; WiFi network scan; the classic display preferences; weather location auto-fill and the OpenWeatherMap key wizard.
- **Default hostname is now `OTGW2`.** A unit with an existing `settings.ini` keeps its saved hostname. The new default applies to new installs and to any reset that discards `settings.ini`. A fresh install is reachable at `http://otgw2.local/`.
- **New flash layout on 4 MB boards** (ADR-191): safeboot 960 KB, firmware 2240 KB, filesystem 768 KB. "Every unit needs one USB migration."
- **GPIO output and webhook trigger-bit labels** were the wrong way round in the v2 settings page. Stored values are unchanged, so check the select after the update.
- **Relicensed from MIT to GNU GPLv3.**

---

## Known issues

- **No 24-hour soak test and no macOS capture run for this build** (TASK-1544), by maintainer decision, as for beta.1. beta.77 ran 1 hour with MQTT connected on one bench unit, which is not a 24-hour test. The PIC command queue rework (beta.19 to beta.28) in particular has had no long run. Long-run stability is exactly what we need your field data for.
- **Decoding a core dump:** the thread listing of `esp-coredump` may fail inside the tool. The decode still shows the panic reason. Send the downloaded file anyway.
- **The OT-Direct thermostat relay change (TASK-1600)** has not been seen on a bench with a thermostat attached.
- **No Immergas boiler has run this firmware.** The Immergas `TP=11:12` behaviour is proven by host tests and by the commands seen on the bench only. On the beta.17 bench the `MM=` could reach the PIC up to about 1 s before its `TP=11:12`; this is open with Sergeant D, and the queue has changed since (strict first-in, first-out, TASK-1569), so the order was not re-checked on this build. Since beta.48 (TASK-1581) SAT also sends `TP=11:12=80` before the `MM=T` at the maximum PWM duty; that is likewise untested on a real Immergas. If you own an Immergas, your capture is valuable.
- **The 3-hour on/off heat-pump cycle** (TASK-1542) has run on the bench with a simulated heat pump, not on a real heat pump or house. Treat it as experimental.
- **An undecodable crash** was seen once on our bench (TASK-1498): a core dump with the program counter in IRAM, and no matching ELF file to decode it. Not reproduced. If you see a crash, the capture tool grabs the crash log.
- **One crash seen in the field** (alpha.566, TASK-1538): a panic inside a memory allocation, with a backtrace too short to name the caller. Not reproduced; crash reports now keep up to 16 return addresses. If your unit reboots unexpectedly, send the crash log (`GET /api/v2/device/crashlog`) and `reboot_log.txt`.
- **Free memory dips under heavy diagnostic load** (TASK-1528). During long capture runs the lowest free-heap mark fell to between 0.5 and 1.7 KB for a moment, while the free heap stays around 71 KB. No crash was linked to it. Under investigation.
- **Cycle verdicts may differ from beta.38** (TASK-1584, 1585, 1586, 1588): the classifier now follows thermo-nova more closely. The change is covered by host tests against the thermo-nova reference, not by a long field run.
- **SAT is still being validated.** A list of open SAT questions is with Sergeant D, the SAT designer; his answers can change SAT behaviour in later builds. The ignition logic for a boiler without a return temperature (TASK-1371) was bench-tested; it has not run on such a boiler in the field.
- **Heating curve self-tune** (on by default) passed a 4-day run in the bench simulation (TASK-1525): hourly and daily records, and on simulated day 3 the curve was raised one step. It has not yet run for days on a real house. Switch it off if you want a fixed coefficient.
- **Pressure alarm:** a pump-head step of about 0.3 bar after the pump stops can still raise it. It is display only and sends no command.
- **Updating through the Home Assistant panel** completed in 2 of 3 bench runs (one 502 during the upload). Update by IP when you can.
- **Filesystem update while a capture runs** (TASK-1524): on our bench a filesystem update failed once while a long capture was connected, and the unit served the file explorer upload page until a second update. Settings were kept. Stop the capture before you update.

---

## How to report

- Discord: #beta-testing or #alpha-testing, with the capture upload zip.
- GitHub: [open an issue](https://github.com/rvdbreemen/OTGW-firmware/issues) and attach the capture files.

Thank you to everyone who tested the alphas and betas, to gruijter and maarten_41762 for the detailed feedback, and to Sergeant D for the SAT design decisions.
