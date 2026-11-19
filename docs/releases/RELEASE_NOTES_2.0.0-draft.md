# OTGW-firmware v2.0.0 Release Notes (draft)

<!-- DRAFT for Robert's review (TASK-1740 AC4/AC6 preparation, 2026-11-09). Not released. Every section is copied from the v2.0.0-beta.158 notes, docs/BREAKING_CHANGES.md or the named task notes. Review notes like this one are HTML comments: they do not render, and the release-2x lint should find none left before publishing. When the release happens, this file becomes docs/releases/RELEASE_NOTES_2.0.0.md (the outdated April 2026 file of that name was removed in TASK-1753). -->

<!-- Refreshed 2026-11-10 against dev at beta.171 (TASK-1740.1): the changes of beta.159 to beta.170 come from the beta.162, beta.167 and beta.170 notes; Known issues were checked against the status of each named task on that date. beta.171 adds only a diagnostic heap trace (TASK-1757) and no user-visible change. What was dropped and why is in the review comments under Known issues. -->

<!-- Refreshed 2026-11-17 against dev at beta.361 (TASK-1740 AC4, comms 92): the SAT section and How to report carry the field-result steps; Known issues rebuilt from the beta.360 list (stale items for TASK-1990, TASK-1874 and TASK-1665 dropped) plus TASK-2005; Highlights and BREAKING still need the full pass at release-candidate compose. Still a draft for Robert's review. -->

**Status:** draft, not released. **Branch:** dev. **Compared with:** the 1.x line (latest stable v1.7.9).

## What 2.0.0 is

2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported: going back to 1.x means putting the Wemos D1 mini back in.

**Supported hardware:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place. The same image also runs OT-Direct, the gateway mode that speaks OpenTherm without a PIC.

The web UI is the v2 web UI, the only web UI in 2.0.0. Read "Before you flash", "BREAKING changes" and "Known issues" first.

<!-- Review: the paragraphs above are taken from "What this beta is" in the beta.158 notes, with the beta wording removed. "OT-Direct ... without a PIC" restates the Highlights; check the wording you want for stable. -->

---

## Highlights

- **v2 web UI.** The only web UI in 2.0.0. Live value table, help text for every setting (the **i** button), settings rail with card panels, SAT page. Since beta.111 each page is one gzipped file with an ETag.
- **Web UI in English, Dutch, German and Greek** (TASK-1764). A flag menu at the top right switches the language at once, without a reload, and remembers your choice. On your first visit the web UI follows your browser language if it is one of the four; otherwise it is English. Only the web UI is translated: firmware texts, MQTT, Home Assistant names, the API and telnet stay English.
- **Firmware updates over WiFi.** A small safeboot updater next to the firmware. If an upload fails or a new firmware crashes in its first 30 seconds, the unit returns to safeboot and stays reachable. One-action update page for the release zip.
- **Monitor graph with history.** The graph opens with the last hour the gateway kept (24 hours on boards with PSRAM). The control setpoint line shows what the boiler got, also when the thermostat briefly asks something else that the gateway replaces (TASK-1522).
- **Works behind a reverse proxy**, including a Home Assistant sidebar panel (ingress). Guide: [Reverse proxy and HA ingress](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md).
- **Home Assistant.** A "Visit" link to the gateway, SAT power and energy sensors for the Energy dashboard, a cumulative hot water total (`dhw_water_total`), and a "Command Dropped" sensor when the PIC never confirms a command.
- **Return water temperature every minute** from a wired sensor (PIC firmware 6.6 or later, `TS=R`).
- **Crash log endpoint.** `GET /api/v2/device/crashlog` for diagnostics. After an ESP32 panic or watchdog reset it holds a `Core dump:` line with the task, the program counter and a short backtrace (TASK-1479). `reboot_log.txt` names every ESP-IDF reset reason, including 0xb (USB reset by the host) (TASK-1438).
- **Capture tool** with raw OpenTherm frame logging (next section), and an opt-in diagnostic recorder on the device (TASK-1665), with Start, Stop and Download in the web UI (TASK-1700).
- **SAT, opt-in and off by default:** every heating start runs in PWM first, a tolerance band around the room target (restart below the target + 0.1 C after a 10 minute pause), no SAT command in the first minute after a reboot, PWM also on boilers that report "master low-off not allowed", and the Home Assistant thermostat card follows SAT while SAT is on (TASK-1942, TASK-1887, TASK-1874, TASK-2000, TASK-1979).
- **Relicensed to GNU GPLv3** (was MIT).

---

## SAT is opt-in and off by default

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- While SAT is on, it owns central heating control: `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT, REST, the boot commands or the serial bridge are refused (TASK-1695; before beta.127 only `CS=` was). `HW=`, `SW=`, `SH=` and every other command still pass.
- Known SAT limits are listed under Known issues.
- **How to report SAT field results:** check the version at `http://<ip>/api/v2/device/info` (`fwversion`); check that `http://<ip>/api/v2/sat/status` shows `"simulation": false`; run the capture tool from the release zip with `--long-run` for at least a night and a morning; note your boiler model, radiators or floor heating and any setting you changed, with the time; stop with `q` and post the `-upload.zip` (see "How to report"). The same steps: https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/sat/field-run-recipe-2.0.0.md. SAT is only as good as the field data we get.

Without SAT, the gateway behaves as a transparent OpenTherm gateway, as in 1.x.

---

## Before you flash

**1. Save your settings.** Download `settings.ini` from the file explorer, or save the output of `GET /api/v2/settings`:

```
curl http://<ip>/api/v2/settings -o settings-backup.json
```

**2. Choose the route that fits your unit.**

- **A new S3 Mini, or a unit on 1.x or on an old partition layout, needs a USB first flash with `flash_otgw`.** For a unit that holds settings the tool saves them, writes the new layout, keeps your WiFi credentials and restores the settings. Windows: `flash_otgw.bat --migrate --host <ip>`. Linux/macOS: `./flash_otgw.sh --migrate --host <ip>`. See the README and the [WiFi firmware update guide](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/guides/WIFI_FIRMWARE_UPDATE.md).
- **A unit that already runs 2.0.0** (beta.1 or later, or a 2.0.0 alpha with the safeboot layout) **updates over WiFi with the flash zip.** Open `/update`, give it the release zip and press **Update**. The page runs the whole sequence: firmware through the safeboot updater, then the filesystem. Settings are kept. Let it finish: this build expects the new filesystem with the gzipped pages. Between the two steps it serves the plain page of the old filesystem.
- Coming from 1.x: this is a different firmware line. Settings and the web UI differ. Read the breaking changes below.

**3. Interrupted filesystem upload.** If the filesystem upload stops part-way, the settings are no longer lost. The gateway wipes the filesystem, writes your settings back from memory and asks you to upload the filesystem again. Until you do, it serves a fallback page with the file explorer upload (`device/info` reports `fsupdateincomplete`).

**4. Do not flash the PIC over WiFi with OTmonitor.** That has not changed.

**5. OTmonitor or the Home Assistant `opentherm_gw` integration?** They connect to the TCP serial bridge on port 25238, which is **off by default (opt-in)**, as in 1.x. Switch on "Legacy TCP port 25238" in the OTGW settings if you use either of them. While SAT is enabled, the bridge checks every line (TASK-1695, ADR-217): it drops the central heating commands above and any binary data, so a PIC firmware upgrade through OTmonitor needs SAT switched off first. With SAT off the port passes everything unchanged.

---

## BREAKING changes

The full log, with the reason and what to do for each item, is [BREAKING_CHANGES.md](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/BREAKING_CHANGES.md). The 2.0.0 entries there:

- Behaviour change: on OT-Direct in gateway mode `TT=` and `TC=` no longer write MsgID 16 and MsgID 100 to the boiler (ADR-223, TASK-1733)
- Changed: while SAT is enabled, external `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` are refused too (TASK-1695)
- Removed: the OT-Direct standalone controller (fixed flow, heating curve, PI room compensation, CH hysteresis) (TASK-1684)
- Behaviour change: a WiFi link without traffic triggers a reconnect, then a restart (TASK-1450)
- Behaviour change: MQTT on-change publishing is the default (ADR-116)

Changes between the 2.0.0 betas are in the release notes of each beta, the latest being the [beta.170 notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.170.md).
<!-- Review: point this link at the last public beta before the release. -->

A note for beta users: from beta.162 to beta.170 no MQTT topic, setting or API field was renamed or removed (beta.170 notes, BREAKING changes).

### Compared with 1.x (v1.7.9)

Checked against the last 1.x release, v1.7.9. Step by step: Chapter 2 of the manual, "Moving from 1.x to 2.0.0, step by step".

- **ESP32-S3 only.** Replace the Wemos D1 mini with a LOLIN S3 Mini or S3 Mini Pro; the OTGW board and the PIC stay. Going back to 1.x means putting the Wemos back.
- **First flash over USB** with `flash_otgw`; after that, updates go over WiFi.
- **GPIO numbers differ.** Set the Dallas, S0 and output pins again after restoring a 1.x `settings.ini`.
- **MQTT unique id.** The default unique id contains the MAC address, and the S3 has another one. Restore the 1.x `settings.ini` (or copy the Unique id) to keep your Home Assistant entities.
- **Default hostname is now `OTGW2`.** A unit with an existing `settings.ini` keeps its saved hostname. The new default applies to new installs and to any reset that discards `settings.ini`. A fresh install is reachable at `http://otgw2.local/`.
- **The v2 web UI replaces the classic web UI** (ADR-192); the File Explorer is the recovery page. See "Classic UI features not in v2" below.
- **SAT is new and off by default.** While it is on, the gateway refuses `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT, REST and the serial bridge. With SAT off the gateway behaves as in 1.x.
- **Relicensed from MIT to GNU GPLv3.**
- Not breaking for a v1.7.9 user, because v1.7.9 already has them: MQTT on-change publishing as the default, the `resetgateway` payload `"1"` with its 5 s limit, the base-topic rules of ADR-066 and the birth/LWT availability.

### For users of a 2.0.0 alpha or beta

The items below changed during the 2.0.0 alphas and betas; most concern SAT, which 1.x does not have. They follow the CHANGELOG; quoted parts are in quotation marks. Check your Home Assistant automations, Node-RED flows and scripts against this list.

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
- **Heating curve self-tune is on by default** (TASK-1525). On a unit with SAT enabled, the heating curve coefficient (the **Heating curve slope** setting) can now change by 0.05 per step. From beta.379 the tuner decides every counted hour over the last 12 counted hours, and a change empties that window, so there are at least 12 counted hours between two steps (TASK-1889, ADR-260; before beta.379: 0.1 per step, at least 3 counted days apart). Do: switch "Heating curve self-tune" off if you want to keep your coefficient fixed.

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

- **The classic web UI is removed; v2 is the only web UI** (ADR-192). The UI switch and the classic-only `ui_*` settings are gone. The File Explorer is the recovery page.
- **Classic UI features not in v2:** the UI switch button; the heap footer; log persistence across page reloads; resizable table columns; the 8-option graph window list; graph disconnect markers; auto-hiding the log on small screens; the `otgwDebug` console helper; the design showcase page; WiFi network scan (the REST endpoint `GET /api/v2/network/scan` stays); the classic display preferences; weather location auto-fill and the OpenWeatherMap key wizard.
- **New flash layout on 4 MB boards** (ADR-191): safeboot 960 KB, firmware 2240 KB, filesystem 768 KB. "Every unit needs one USB migration."
- **GPIO output and webhook trigger-bit labels** were the wrong way round in the v2 settings page. Stored values are unchanged, so check the select after the update.

---

## Changes during the 2.0.0 betas

Most changes since the first 2.0.0 beta are already described above (Highlights and "For users of a 2.0.0 alpha or beta"). This section lists the rest per area, in the state they have in 2.0.0. If you come from an earlier 2.0.0 beta, scan the area you use. The task number and, where the beta notes give it, the first beta that carried the change are in brackets.

### SAT

- With the room outside the 0.1 C band around the target, the PID integral is halved once and then held instead of reset to 0; the same applies to the zone thermostats (TASK-1890, beta.376).
- SAT no longer switches from continuous to PWM while the tolerance band pauses heating (TASK-1887, beta.377).
- PWM ON phases that start during a band pause do not count toward the hourly start limit, and the SAT narration names the value that decided a band stop or restart (TASK-1887, beta.358).
- Fixed: within one burn SAT no longer flips back and forth between continuous and PWM, and those PWM re-entries no longer use up the cycles-per-hour limit. Once a burn has gone from PWM back to continuous, the switch margin no longer moves it to PWM: SAT stays continuous until the flame goes out, and only the safety switch can still take that burn to PWM. Only the first PWM ON of a burn counts toward the hourly limit. The safety switch back to PWM acts at 3.0 C above the request for 30 s instead of 3.5 C (Sergeant D entries 428 to 430, TASK-2913, beta.391).
- SAT switches from continuous to PWM when the flow stays at least 2.0 C above its request for 60 s; the "PWM switch margin" setting (0.5 to 5.0 C) is in the web UI, REST and MQTT, and the 30 s safety switch stays (3.5 C, from beta.391 3.0 C) (TASK-1888, beta.345; TASK-2005, beta.361).
- SAT leaves PWM for continuous mode when the flame is on and the request stays above the flow for 60 s (TASK-1933, beta.274).
- The PWM ON setpoint follows the latest flow temperature, as thermo-nova does (TASK-1990, beta.343, after the 2 minute average of beta.330 and beta.333).
- SAT raises a diagnostic event when the burner does not light or heating fails, and leaves those hours out of the heating curve self-tune; it takes no control action (TASK-1882, beta.341).
- A self-tune hour counts at 90 % clean ticks (108 of 120); ticks left out for hot water or sun only stay out of the hour's mean (TASK-1932, beta.339).
- Fixed a brief hand-back in the middle of a burn: the weather is fetched 10 minutes before the outside temperature from the boiler or Home Assistant expires (TASK-2039, beta.380).
- An outside temperature that Home Assistant sends with `OT=` (boards with a PIC) now counts for SAT (TASK-2040, beta.381).
- SAT learns how fast the house cools only while it controls the boiler; after a long run without a boiler, reset `SATthermalcoeff` (TASK-2010, beta.362).
- A settings re-read no longer drops the active SAT preset, and starting the capture tool no longer triggers that re-read (TASK-2013, beta.369).
- No false pressure-drop alarm after a normal pump stop: readings of the 600 s after a CH mode change are not used, and a real pressure loss is seen about 10 minutes later than before (TASK-1867, beta.332).
- No `CS=10` without a flow temperature during a PWM ON phase; the setpoint is left as it is (TASK-1412.18.1, beta.334).
- SAT state files are written by a background writer, so a slow flash write no longer stalls the control loop (TASK-1989, beta.357).
- Ending a simulation reloads the real PID state and heating curve data, and a simulation no longer overwrites your saved PID and heating curve data (TASK-1952, beta.295; TASK-1900, beta.281).
- A replayed log no longer changes your stored SAT target or the stored PIC board mode (TASK-1901, TASK-1902, beta.268).
- The self-tune help text says that after a change the tuner waits for new counted hours, so steps are further apart than "once a day" (TASK-1792, beta.265).
- Ten SAT settings show the range the gateway really stores, in the web UI and in `GET /api/v2/settings`; very small lower limits such as 0.005 are shown as they are (TASK-1941, beta.264 and beta.285).
- SAT calibration markers are handled in memory and written to flash in the background, not from the web server (TASK-1911, beta.284).

### Home Assistant and MQTT

- The SAT Simulation switch is shown as unavailable while a real boiler answers, because simulation is refused then; needs Home Assistant 2021.2 or later (TASK-2014, beta.374).
- A retained `<label>/override` topic is cleared after the next connect, and removed when the override ends (up to about 11 minutes after the last override frame) (TASK-1991, beta.364; TASK-1985, beta.336).
- The 26 hourly statistics topics go out one per 100 ms, which removes the free-memory dip at the full hour on boards without PSRAM (TASK-2004, beta.355 and beta.356).
- The DHW flow rate sensor is created in Home Assistant again: the unit is "L/min", also in the OT log, web UI and docs (TASK-1949, beta.257; GitHub PR #702 by justink0k).
- MQTT commands keep working after a low-memory connect: a refused command subscription is retried until accepted (TASK-1778, beta.260).
- A planned reboot publishes the retained "offline" status first and then disconnects (TASK-1780, beta.262).

### Web UI and graphs

- The one-time question whether to check GitHub for new versions is a chip "Check for new versions?" in the header; it no longer opens as a dialog over the page you are on. A click on it shows the explanation and the No / Yes buttons. The "New version available" notice now also shows on a phone; before beta.393 it was hidden at a width of 480 px or less (TASK-2918, beta.393).
- The web UI no longer calls SAT experimental; SAT stays opt-in and off by default, and the setup wizard still asks you to confirm before SAT controls the boiler. The heat-pump boiler type is still marked experimental (TASK-2909, beta.390).
- The Monitor graph and its CSV export show SAT's target temperature as its own dashed line while SAT is on, also in the 24 h history (TASK-2018, beta.378).
- The SAT messages "boiler handed back" now say "SAT paused", with the reason, in English, Dutch, German and Greek (TASK-2036, beta.382).
- The SAT weather card uses Home Assistant's sun position (`sat/sun_elevation`) while a value younger than 1 hour is published and then asks for no location; otherwise it asks for latitude and longitude (TASK-2015, beta.371).
- The Monitor graph has a DHW temperature line (MsgID 26) in the graph and its CSV, from the moment the page opens (TASK-1776).
- Boards with PSRAM keep 7 days of graph history; the graph has a 7 days window, `GET /api/v2/graph/history?hours=N` (1 to 168) asks for another window, and a window of more than 1008 slots is thinned (TASK-1963).
- The graph legend and tooltip explain the dashed red and green lines (live connection lost and back), and a real gateway restart gets a solid amber mark (TASK-1914, beta.271).
- The 24 h graph uses the weather outside temperature only while it is younger than 1 hour and then falls back to the boiler's MsgID 27 (TASK-1937, beta.258).
- A page load no longer gets 503 answers because of closing connections: a connection that is closing or still being set up and holds no data does not count towards the limit of 4 (TASK-1947, beta.305).
- Clear for the cooling level sends `CL=-` instead of `CL=0` (TASK-1958).
- The PIC panel shows a pending or clearing setpoint override as text instead of the raw value such as `t20.50` (TASK-1939).
- The settings help is served gzipped with an ETag (a reload answers 304); the app image is about 22 KB smaller (TASK-1955).
- Mission control follows the light and dark theme (TASK-1863, beta.267).
- On a phone the language button and the Light/Dark switch have the same height (TASK-1873, beta.266).

### REST API

- Fixed a crash when two web requests arrived while memory was nearly exhausted: the "busy" answer itself needed memory and stopped the firmware. With the largest free memory block under 4 KB the gateway now closes that connection without an answer; above it the 503 with `Retry-After` is unchanged (TASK-2910, beta.389).
- A control command that SAT refuses through `POST /api/v2/otgw/commands` answers 409 "Command refused: SAT is in control of the boiler"; 503 stays for a full queue (TASK-1970).
- `otgw/messages` and `otgw/label` carry a `source` field (none, boiler, gateway, thermostat, summary, replay) next to the value (TASK-1965).
- `GET /api/v2/sat/weather/needs-setup` gives its own answer instead of the full weather status (TASK-1969).
- Commands from the web page and the REST API run in order on the main loop; a burst too big for the queue gets HTTP 503 instead of a silent OK (TASK-1761, beta.294).
- `GET /api/v2/device/info` no longer adds a stray closing bracket when queried very often (TASK-1910, beta.283).

### OT-Direct (boards without a PIC)

PIC-style commands and answers (PIC firmware 6.8):

- The PR= reports (P, R, O, C, B, L) and the SC=, SR= and FT= replies use the PIC 6.8 wording (TASK-1848, beta.252; TASK-1849, beta.253).
- Command errors read like the PIC's, for example `CH: BV` (TASK-1804, beta.362).
- Commands without a value answer `SE`, and `OT=`, `MM=` and `CL=` without a value clear (TASK-1957, beta.296).
- Commands read their numbers like the PIC: `CS=49.72` sends 49.72 and `RT=25.4` means 25.40; wrong values answer `SE` or `OR`, `TT=`, `TC=` and `SB=` refuse negative values, `SW=` and `SH=` answer `BV` outside the boiler limits, `OT=` of 64 or more clears the outside temperature (TASK-1859, beta.288 to beta.291).
- `MM=-` clears the maximum modulation instead of setting 0 % (TASK-1799, beta.292).
- `GW=` with an invalid value changes nothing and answers `BV` or `SE` (TASK-1946, beta.276).
- `SC=` refuses a time or day the PIC refuses (`OR`, `SE`, `BV`); `SC=12:30` without a day is no longer accepted (TASK-1908).
- `PR=M` understands the OT-Direct mode letters `S`, `L` and `P` (TASK-1951, beta.304).
- `PM=` reads whole tables and strings, Data-IDs 0 to 255 (TASK-1847, beta.297).
- More commands answer as the PIC: short commands and `PR=` variants, the greeting line after `GW=R` to port 25238, `MH=`, `MW=` and `M2=` read-back, `VS=` and `MI=` limits, MsgID 16 always acknowledged, a `TT=` or `TC=` override cleared when the thermostat is lost, `SR=` on a write answers WRITE-ACK, a new boiler fault reads MsgID 5 at once (TASK-1804, beta.324).
- Boiler messages the boiler does not support are handled like the PIC: `MH=`, `MW=`, `M2=` and `HW=P` skip MsgID 99, and an ID that answers Unknown-DataID three times for `SR=` is left out (TASK-1804, beta.367).
- `SR=` rewrites the boiler's answer as the PIC does instead of answering locally (TASK-1804, beta.342).
- A refused `CS=` or `SW=` (Data-Invalid or Unknown-DataID) is dropped like the PIC (TASK-1804, beta.335).
- A command that does not fit the full queue gets no success answer (`MH=`, `MW=`, `M2=`, `RR=`, `RS=`, `PM=`) (TASK-1804, beta.337).
- The thermostat's MsgID 99 is handled like the PIC: a pending `MW=`, `MH=`, `M2=` or `HW=P` rides on it, and a thermostat write sets the stored operating modes (TASK-1906, beta.298).
- `OT=`, `SW=` and `SH=` reach the boiler without a free slot, riding on the thermostat's reads of MsgID 27, 56 and 57 (TASK-1803, beta.299).
- The MsgID 100 override flags and `TT=0` behave like the PIC; in master mode `TT=0` puts a `BS=` or MQTT room setpoint back (TASK-1750, beta.300).
- Master mode sends the room setpoint of `BS=` to the boiler and puts it back after `TT=0` (TASK-1751, beta.354).
- `OH=` is on by default, set once at the first boot on this version (TASK-1961, beta.301).
- "Thermostat disconnected" and "Thermostat connected" are printed like the PIC, also to port 25238 (TASK-1850, beta.259).
- A client that connects to port 25238 right after a reboot no longer gets a stray "Thermostat disconnected" line, which made pyotgw fail to connect about 2 times in 5 (TASK-1999, beta.353).
- Master mode passes a thermostat's writes (any Data-ID except 1, 16 and 24, and 8 and 14 while SAT owns the setpoint) to the boiler and returns the boiler's answer; the thermostat interface is on by default in master mode (TASK-1812, beta.329).
- Removed the setback fields that did nothing; `SB=` and `FS=` stay, as on the PIC (TASK-1950).

Timing and reliability:

- The scheduler keeps the 100 ms master wait time after a conversation (TASK-1954).
- A lost thermostat counts as lost after about 3 s instead of 30 s (gateway mode), and without a thermostat the next status message goes out within about 1 s after a missed boiler answer (TASK-1802, beta.277).
- The boiler line no longer falls silent for 36 minutes, and `/api/v2/device/info` reports `otdrequestactive`, `otdlinestalefixes` and `otdlinetxrestarts` (TASK-1966).
- The OpenTherm library is pinned to a fork that carries the boiler line fix (submitted upstream as Phunkafizer/opentherm_library#1), and the gateway's own workaround is gone (TASK-1980, beta.328).
- The flow temperature (MsgID 25) is read every 10 s again, also during the 60 s slow-poll burst (TASK-1981, beta.325).
- Nothing queued during a simulation reaches the boiler afterwards, and a cleared value no longer follows its old write (TASK-1992, beta.338).
- Loopback mode no longer counts as your real boiler for SAT (manufacturer, heat source, PWM permission) (TASK-1857, TASK-1903, beta.270).
- A gateway/monitor mode switch keeps HW, CS, SR and the other command settings; only the command queue is emptied (TASK-1839, beta.272).
- Saved command settings (HW, the unknown-ID list, the ventilation setpoint) survive a reboot without a thermostat (TASK-1944, beta.275).
- A thermostat in master mode no longer gets stale boiler values, and a mode switch or `GW=R` forgets old boiler data (TASK-1807, beta.278).
- No 16 C flow setpoint when the thermostat disappears; the gateway's own messages keep heating off unless `CS=` is set (TASK-1806, beta.279).
- A boiler "data invalid" answer is never stored as a value (TASK-1925, beta.280).
- MsgIDs 1, 3, 14, 17 and 25 are never switched off after "unknown" answers (TASK-1801, beta.293).

### PIC (OTGW Classic)

- The PIC settings fill in about a minute after a restart instead of four to five (TASK-1971, beta.340).
- The PIC `Error 01` counter counts again (TASK-1804, beta.324).
- During a SAT simulation the `PR=` report commands reach the PIC again, so the PIC settings, the gateway mode and the `PR=E` return-water reading fill (TASK-1943, beta.306).
- New setting "PIC temperature sensor" (default off): with it on, the gateway reads the sensor attached to the PIC every 3 minutes and publishes it to MQTT, Home Assistant and REST, as on 1.x (TASK-1940, beta.254 to beta.256).

### Filesystem, OTA and boot

- A freshly flashed unit starts with the normal defaults (MQTT on, new topic names, setup wizard) because the filesystem image no longer holds a default `settings.ini`; a filesystem update still puts your settings back (TASK-1920).
- An image that fails its first boot goes back to the safeboot updater at once (TASK-1919, beta.327).
- A filesystem update keeps the newest whole lines of the reboot log (TASK-1931, beta.326).
- A setting or SAT state saved while a filesystem update starts is no longer lost (TASK-2007, beta.363 and beta.365).
- The once-a-minute filesystem checks run on the background file writer instead of the main loop, which removes short loop stalls (TASK-1995, beta.370).
- The filesystem health check no longer writes to flash every 5 minutes; an unwritable filesystem is reported as health DEGRADED after the next SAT state-file write (TASK-2020, beta.373).
- A reset during a save no longer empties the thermostat and boiler support files or the boot-detection log (TASK-1916, beta.282).

### BLE (Bluetooth sensors)

- On boards without PSRAM BLE can no longer be switched on (greyed out with "BLE needs PSRAM", refused by REST with 409 and by MQTT, a stored "on" cleared at first start); the LOLIN S3 Mini and S3 Mini Pro keep BLE (TASK-2025, beta.375).

### Capture tool

- The capture records only this gateway's MQTT topics and its Home Assistant discovery configs; `--mqtt-all-topics` records everything (TASK-1988).
- The capture collects the device state, the graph history (24 h, and 7 days where available) as JSON and CSV, and the diagnostic recording the device holds; `--skip-device-state`, `--skip-graph-history` and `--skip-diag-recording` turn parts off (TASK-1964).
- The upload fits one Discord attachment (`.tar.xz` over 24 MB, essentials first) and always holds the summary and the crash log (TASK-1986).
- A core dump is labelled latest or old against the reboot log, and the graph CSV matches the web UI export (TASK-1972).
- On macOS with the built-in Python 3.9 the capture tool writes the graph history CSV again (TASK-2016).
- The device check no longer counts an OT-Direct route that answers "not on this board" as a failure on an OTGW Classic (TASK-2012).

### WiFi

- **The WiFi AP fallback is now part of the stable release** (TASK-2931, Robert's decision, beta.401). When the saved WiFi network cannot be reached, after 2 failed attempts (about a minute) or at boot, the gateway opens its own access point `OTGW-<last 3 bytes of the MAC>` (password `otgw1234`, web UI at `http://192.168.4.1`) and retries your network every 5 minutes. There is no AP fallback with the setting `wifinoportal` (the gateway keeps retrying without restarting) or while memory is critically low (it then restarts after 10 failed attempts). Before beta.401 only beta builds had it. See section 6.1.3 of the manual.
- A gateway whose network is dead while its memory is nearly exhausted now recovers itself: a WiFi check that cannot run for lack of memory counts as dead when the largest free memory block is under 4 KB, so the forced reconnect and then the restart follow (TASK-2026, ADR-258, beta.383). While memory stays that low, a short WiFi reconnect no longer starts that count again (ADR-261, beta.385; log line from beta.388).

### Diagnostics

- A stopped diagnostic recording stays readable until the next start on boards with PSRAM (TASK-1960, beta.344).
- The gateway reports the full fingerprint of its firmware (`appelfsha256` in `/api/v2/device/info` and next to a core dump) (TASK-1983, beta.346 and beta.347).
- The telnet line `[fs-slow]` names a filesystem step that took 100 ms or more (TASK-1995, beta.362 and beta.368).
- The web connection limit log line splits connections by state and says whether they were closing or still opening (TASK-1947, beta.261 and beta.286).
- `device/info` reports the IPC task stack low points taken after hardware detection (TASK-1870.1, beta.263).

---

## Limitations

These are design limits of 2.0.0, not bugs.

- **ESP32-S3 only.** 2.0.0 runs on the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. The ESP8266 (Wemos D1 mini, NodeMCU) stays on 1.x.
- **No HTTPS.** The web UI, the REST API and the WebSocket use plain HTTP and WS on your local network. Do not expose the gateway to the internet; use a VPN or a reverse proxy if you need remote access.
- **BLE sensors need a board with PSRAM** (TASK-2025, ADR-257). On a board without PSRAM the BLE scanner does not start and the switch is disabled with "BLE needs PSRAM".
- **Firmware SAT and a SAT integration in Home Assistant exclude each other.** While the firmware SAT is on, the gateway refuses `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT and REST, so the integration cannot control (TASK-1695).
- **Port 25238 filters inbound lines while SAT is on** (ADR-217). Text commands that would change central heating control and binary data are dropped, so OTmonitor cannot control the boiler or flash the PIC over that port while SAT is enabled. With SAT off the port is byte-transparent.

## Known issues

- **Most settings help pop-ups are in English in Dutch, German and Greek** (TASK-2927). The rest of the web UI is translated, and every help text falls back to English, so no help is missing. The translation follows in 2.1.0 (Robert's decision, 2026-11-19).

- **Under investigation: once, a bench unit hung after a crash restart** (TASK-2911). It was on WiFi with an address, but did not answer ping, the web UI or telnet for 8 minutes, until a USB reset; the cause is not known. Normal restarts on that unit worked 5 of 5 times. The WiFi self-recovery only starts after the gateway's router answered once since the start, so it does not cover this case. If your gateway is on WiFi but unreachable after a restart, please tell us.

- **The WiFi self-recovery keeps counting a dead link across a short WiFi reconnect while memory is nearly exhausted (TASK-2026).** This is proven in tests on the real code; a test on a device could not create the exact memory state it needs.

- **The continuous to PWM switch rule of beta.361 (flow at least 2 C above the request for 60 s) has not run with a real boiler** (TASK-2005). A replay of a tester's Vitodens capture through the real code shows the old rule switching 3 times and the new rule not at all.

- **The tolerance band of beta.359/360 and the PWM-first start of beta.351 have not run with a real boiler yet.** They rest on host tests on the real SAT code and on bench units without a boiler (TASK-1887, TASK-1942). The MsgID 3 change (TASK-2000) is confirmed on a tester's Viessmann Vitodens (beta.350, real boiler). Field reports with a capture are welcome.
- **`SR=` on OT-Direct (TASK-1804 A6) has not been seen with a thermostat;** it rests on a host test.
- **The Home Assistant card of TASK-1979 has no screenshot from a real Home Assistant on an OT-Direct unit yet.** A tester confirmed it on an OTGW Classic. The MQTT side was checked on the OT-Direct bench unit. With SAT off and no thermostat connected, the card keeps showing SAT's last room temperature until a thermostat reports one.
- **The pressure-drop alarm fix has not run on a unit with a pressure sensor** (TASK-1867). The proof is a replay of gruijter's pressure series through the real code (two false alarms on the old code, none on the fix); the bench unit has no pressure sensor.
- **OT-Direct: going back to a build older than beta.301 and up again sets `OH=` to 1 once more** (TASK-1961, confirmed on the OT-Direct bench unit). The one-time switch to the PIC default runs again after such a round trip; set `OH=0` again if you had chosen that.
- **The thermostat write path of OT-Direct master mode has not been seen with a thermostat** (TASK-1811). The bench unit has no thermostat on the slave side, so the new expiry and the `TSet` of 0 rest on host tests on the real handler and expiry code, and on a bench run that shows the `CS=` timings did not change. If your thermostat in master mode behaves differently than before, please send a capture.
- **OT-Direct: the `CS=` timings are proven in master mode only** (TASK-1834, TASK-1917). The bench ran master mode without a thermostat; the substitution of a real thermostat `TSet` in gateway mode was not exercised there. The timer and release paths are measured (63.53 s, 63.84 s, `CS=5` stays, `CS=0` and `CS=0.001` release); the substitution code is unchanged.
- **A Data-Invalid answer to MsgID 99 on OT-Direct has not been seen** (TASK-1907). The loopback test never reaches that branch; the proof is a host test (old code 2 failures, fix passes). A boiler that answers MsgID 99 with Data-Invalid will show it.
- **A late PIC after a missed boot probe is not bench-tested** (TASK-1790). The ESP cannot hold the PIC silent at boot on the bench, so there is no red reference either; the host test on the real board resolution block is the proof (old code 2 of 5 fail, fix 5 of 5). A bare S3 module with PSRAM and without a PIC is the open residual case. Auto on the OT-Direct bench unit still closes the PIC port, as before.
- **A real power cut in the middle of a settings save has not been tested** (TASK-1913). The settings are written to a temporary file and swapped in. On the bench, 42 of 42 USB resets during a save left a whole settings file, 9 of them certain mid-write hits. That a power cut behaves the same rests on the atomic rename of the filesystem, not on a measurement. The password guard on `/settings.tmp` is checked statically, not with an admin password set.
- **The first Dallas reading fix has not run on a DS18B20** (TASK-1824). No bench unit has one; the host test uses a sensor model (old code publishes 85.0 on boot, fix 4 of 4). The Dallas labels change (TASK-1912) was bench-tested with a simulated sensor.
- **The Dutch, German and Greek texts of the self-tune progress box are not reviewed twice** (TASK-1881). They were translated by hand against the glossary, without the usual two reviews; a review is open. If a text reads oddly, please tell us.
- **The AsyncTCP repair is proven by a host test** (TASK-1746.1). A connection storm rarely makes an allocation fail inside the connection accept, so the 20 restarts under storm on the bench show no regression but do not show that path being hit.
- **A filesystem update can, rarely, panic on the OTGW Classic** (TASK-1774). On our OTGW Classic bench unit a filesystem update panicked in an internal system task on beta.167 on 10 November, and in 1 of 9 filesystem writes on 12 November (beta.217). Both times the settings went back to their defaults and the unit came back in the safeboot updater; uploading the firmware there again brought it back. The likely cause is too small a stack for that system task. Since beta.219 that stack is larger (TASK-1870): on the same unit the free stack during a filesystem write went from 80-88 to 340 bytes, and 11 updates on beta.219 gave no panic. The cause is not proven, so this stays a known issue until more updates have run. Since beta.238 a panic or reset during the filesystem write no longer resets the settings: the next boot formats the filesystem, writes back the settings, the reboot counter and the reboot log, and asks for the filesystem upload again. A power cut during the write is not covered and can still leave the defaults. **Do:** save your settings before every update (see "Before you flash"), and if the unit comes back in safeboot, upload the firmware there and restore your settings if needed.
- **The three-reset WiFi recovery has not been done on a bench** (TASK-1930). The bench can only restart over the web or USB, which never count. The counting of reset-button presses and power-ons rests on a host test on the real code. If the gesture does not open the portal on your unit, or the portal opens without it, please tell us.
- **The 60 s flow follow of SAT continuous mode after an ignition has not run on a boiler** (TASK-1924). On the bench the simulated flow stayed below the request in those 60 s, so the follow never came into play; the proof is a host test on the real control code. The new `CS=` timing was not checked during the 2 s start-up of a boiler without a return temperature either.
- **OT-Direct master mode: the Data-Invalid answer to a thermostat has not been seen** (TASK-1808). No bench unit has a thermostat or a boiler; the proof is a host test (old code fails, fix passes).
- **A reset by the external 0x26 watchdog has not been seen in the new reboot log line** (TASK-1904). No bench unit has the chip; the proof is a host test on the real log code.
- **Watchdog restarts in the field may not be gone** (TASK-1865, TASK-1899). The filesystem reads that a field crash pointed at were moved out of `/api/v2/health`, `/api/v2/simulate` and `device/info`, which the web UI polls, and the internal task stack was enlarged. On the bench, 125 minutes of polling both every 2 s with a filesystem write every minute gave no restart, and a tester's 25.5 hour run on an OTGW Classic gave no watchdog, panic or reboot (light load on these handlers). The original field crash was never reproduced, and two of the three restarts of one tester's unit left no crash report, so their cause is not known. **Do:** close the gateway tabs you do not use (a hidden tab stops asking), and if your unit restarts unexpectedly, send `GET /api/v2/device/crashlog` and `reboot_log.txt`.

- **The new REST and SAT telemetry fields have not been checked against a real boiler** (TASK-1871, TASK-1880, TASK-1885). The bench has no boiler: the `status` of `/api/v2/otgw/messages/<id>` for an acknowledge, Unknown-DataID or Data-Invalid answer, the flow and flame fields of the SAT status, and the PWM phase record of a phase in which the burner never lights rest on host tests and the SAT simulation. If one of them disagrees with what your boiler does, please send a capture.
- **SAT's own requests for the flow and return temperature on a PIC board have not run on a real boiler with a thermostat** (TASK-1653 `PM=25`, TASK-1759 `PM=28`). They are covered by host tests on the real queue and frame code. The PIC sends such a request only in place of a thermostat request the boiler does not support; if your thermostat only asks supported values, SAT sends nothing and `sat/pm25_no_slot` says so. The REST SAT status counts `pm28_sent`, `pm28_answered` and `pm28_unanswered`.
- **OT-Direct: the boot change (TASK-1835) is not proven at frame level.** The bench unit has no boiler and no bus listener, and telnet starts after that part of the boot. On the bench, gateway mode came up about 1 s sooner than master mode in the same build, which matches the one boot status request it no longer sends; that is an indication only. A boot capture with a thermostat in gateway mode is welcome.
- **OT-Direct: the clock answer to a real thermostat (TASK-1840) has not been seen.** The bench checked that `SC=` puts nothing on the bus; the answer to the thermostat's MsgID 20 rests on host tests.
- **Used SAT in an alpha before 30 October 2026? Check the heating curve slope** (TASK-1787). The hourly heating curve auto-tune of the early 2.0.0 test builds (the `SAT Auto Tune` switch, removed by TASK-1413 on 30 October 2026) could lower the heating curve coefficient. 2.0.0 keeps a stored coefficient as it is, so a unit that ran that auto-tune can still carry a value far below the default of 2.0; one tester's unit was at 1.12. The self-tune moves the coefficient only by 0.05 per step, with at least 12 counted hours between steps (from beta.379), so it takes many hours to correct such a value. **Do:** after the upgrade, open Settings > SAT and check **Heating curve slope** (default 2.0).
- **Several open browser tabs still get a few "busy" answers** (TASK-1741, TASK-1747). On a board without PSRAM, two visible tabs on Monitor > Values got about 0.5 HTTP 503 answers per minute on the bench (0.4 % of requests), down from 15.6. The pages keep showing their last values, so a busy answer only makes a value one poll older. **Do:** close the gateway tabs you do not use; a hidden tab stops polling by itself.
- **The first periodic refresh of a page can come up to 15 s after the page loads** (TASK-1741). This is the price of spreading the requests of each tab.
- **The thermostat in OT-Direct gateway mode has not been tested on a bench** (TASK-1707, TASK-1718, TASK-1733). The bench unit has no thermostat. These changes are covered by host tests on the real gateway path. Whether the thermostat shows a `TT=` override and raises its demand, and how the slots behave with a real thermostat and boiler over time, is waiting for tjfs. If your boiler or thermostat misbehaves in gateway mode, please send a capture.
- **The safeboot updater page still reloads after a fixed 20 seconds** (TASK-1703). The normal update page returns to the web UI by itself after a successful update. If the page stays on "Opening the web UI", open the link.
- **The moved S0 and output pins** (TASK-1698) were bench-tested on an OTGW Classic with an S3 Mini: the settings diff, the output switching without a refusal, and a reboot with S0 enabled. S0 pulse counting with a real meter and the S3 Mini Pro pins were not tested.
- **Page loads are about 0.3 s slower than they would be without the send cap** (TASK-1673, ADR-216). The cap on each web connection's send queue halves the data in flight. The page still loads much faster than in beta.105, because it is now one gzipped file. In the bench two-tab test the median tab load is 1.4 to 1.5 s against 1.1 to 1.3 s without the cap. It is the price for the higher free-heap margin.
- **Two tabs loading at once can still push free memory low, but more rarely** (TASK-1679). With the send cap, 1 of 6 bench runs still reached a lowest free heap of 3,252 bytes. The rest of that dip sits outside the send queues, in the web server's own task, and is being measured. No crash or reboot was seen in these tests. Avoid loading the web UI in several tabs at the same moment.
- **The OT-Direct gateway-mode fix (TASK-1701) has not been seen during a heat demand.** The field capture covered 20 minutes in standby: no own status messages from the gateway and no cycling. Whether the boiler follows the thermostat correctly when it asks for heat, and switches off again, is not proven yet. If your boiler still switches between heating and standby, please send a capture.
- **Free memory went lower during a field capture than on our bench** (TASK-1679). In a 20-minute capture on beta.127 (OT-Direct, web UI open in the capture's browser, telnet, WebSocket and polls at once) the lowest free heap was 16,124 bytes, with one REST and one connection answered `503`. The OT-Direct bench unit on beta.129 reached 18,384 bytes; the OTGW Classic on beta.118 24,132. No crash or restart came with it. The new `hd_ack_low_*` and `hd_dip_mqtt_*` keys are there to find where it goes.
- **One unexplained 3-minute connection drop in a field capture.** On beta.127 the telnet, WebSocket and browser connections of a capture dropped at the same moment for 197 s, while the gateway kept running: no restart, no WiFi drop recorded, the main loop never stalled for more than 0.8 s. The cause (the PC's network, the access point, or the gateway's network stack) is not known. If you see the web UI and telnet stop answering for minutes, a capture of it helps.
- **The port 25238 line filter (TASK-1695) has been tested on the host only.** Our bench unit has no PIC. If OTmonitor or the Home Assistant `opentherm_gw` integration misbehaves while SAT is on, `ext_bridge_dropped` in `/api/v2/sat/status` and the telnet log show what the bridge dropped.
- **A rare `/favicon.ico` request got no answer in early bench tests** (TASK-1675): 4 of 400 requests, before the send cap. Since the send cap and the favicon served from flash (TASK-1696) there were no failures in 2,300 requests, but the cause was never observed, so it is not proven gone. If a page hangs while loading, a capture of it helps us.
- **The OLED fix (TASK-1678) is confirmed on an OT-Direct unit by a tester, not on an OTGW Classic with an S3 module.** That path is proven by host tests only; our bench unit has no OLED. If your OLED stays dark during the WiFi setup, please tell us.
- **The "No setup portal when Wi-Fi is down" setting has been bench-tested for a 5 minute outage, not for days** (TASK-1645, TASK-1455). The checks that a power cycle, a first install and a triple reset still open the setup portal were not done on a bench. If the portal does not open when it should, please tell us.
- **The new WiFi features have not run in the field** (TASK-1450, TASK-1455, TASK-1446, TASK-1444). The dead-link recovery was seen once on the bench (forced reconnect, then the restart); its guards, the restart cap, the skip-portal boot and the outage record under a real WiFi drop are covered by host tests only. If your WiFi drops, the `Drop :` line in the telnet banner and the `wifi_drop_*` keys in `device/info` are what we would like to see.
- **A boot with the telnet console switched off** was checked in the code only (TASK-1644); switching it off and on while running was bench-tested.
- **Decoding a core dump:** the thread listing of `esp-coredump` may fail inside the tool. The decode still shows the panic reason. Send the downloaded file anyway.
- **The OT-Direct thermostat relay change (TASK-1600)** has not been seen on a bench with a thermostat attached; in the field it ran on one unit with a thermostat for 20 minutes (beta.127, standby).
- **No Immergas boiler has run this firmware.** The Immergas `TP=11:12` behaviour is proven by host tests and by the commands seen on the bench only. On the beta.17 bench the `MM=` could reach the PIC up to about 1 s before its `TP=11:12`; this is open with Sergeant D, and the queue has changed since (strict first-in, first-out, TASK-1569), so the order was not re-checked on this build. Since beta.48 (TASK-1581) SAT also sends `TP=11:12=80` before the `MM=T` at the maximum PWM duty; that is likewise untested on a real Immergas. If you own an Immergas, your capture is valuable.
- **The 3-hour on/off heat-pump cycle** (TASK-1542) has run on the bench with a simulated heat pump, not on a real heat pump or house. Treat it as experimental.
- **An undecodable crash** was seen once on our bench (TASK-1498): a core dump with the program counter in IRAM, and no matching ELF file to decode it. Not reproduced. If you see a crash, the capture tool grabs the crash log.
- **One crash seen in the field** (alpha.566, TASK-1538): a panic inside a memory allocation, with a backtrace too short to name the caller. Not reproduced; crash reports now keep up to 16 return addresses. If your unit reboots unexpectedly, send the crash log (`GET /api/v2/device/crashlog`) and `reboot_log.txt`.
- **Free memory dips under heavy diagnostic load** (TASK-1528). During long capture runs the lowest free-heap mark fell to between 0.5 and 1.7 KB for a moment, while the free heap stays around 71 KB. No crash was linked to it, and the dip was not pinned to one allocation. The `hd_dip_*` keys in `device/info` show what held the memory at that moment.
- **Cycle verdicts may differ from the early 2.0.0 test builds** (TASK-1584, 1585, 1586, 1588): the classifier now follows thermo-nova more closely. The change is covered by host tests against the thermo-nova reference, not by a long field run.
- **SAT's flame-out protection (TASK-1762) has not run on a real boiler.** If SAT switches to pulse mode more often than you expect, look for the SAT log line "Control mode: PWM (fail-safe: ...)", and please send a capture.
- **SAT behaviour is fixed for 2.0.0.** Open SAT questions with Sergeant D, the SAT designer, are for 2.1.0 and later.
- **The ignition logic for a boiler without a return temperature has not run on such a boiler in the field** (TASK-1371). It was bench-tested.
- **Heating curve self-tune** (on by default) passed a 4-day run in the bench simulation (TASK-1525): hourly and daily records, and on simulated day 3 the curve was raised one step (the day-based timing of that build; from beta.379 the tuner decides per counted hour, covered by host tests and a closed-loop convergence test, TASK-1889). It has not yet run for days on a real house. Switch it off if you want a fixed coefficient. The phase split of beta.176 (TASK-1796) and the changes of beta.189 and beta.190 (TASK-1815, TASK-1816) are covered by host tests and short bench runs in the SAT simulation; the two-phase decision over several real days is not tested yet.
- **Updating through the Home Assistant panel** completed in 2 of 3 bench runs (one 502 during the upload). Update by IP when you can.
- **Filesystem update while a capture runs** (TASK-1524): on our bench a filesystem update failed once while a long capture was connected, and the unit served the file explorer upload page until a second update. Settings were kept. Stop the capture before you update.

---

## Capture tool

One Python tool does the diagnostic capture. Two launchers start it:

- Windows: `capture-mqtt-debug.bat`
- Linux and macOS: `capture-mqtt-debug.sh`

If no Python 3.9 or newer is installed, the launcher downloads a portable Python once and checks its SHA-256. You do not need to install anything. On Windows the download now uses Windows' own `curl.exe`, `certutil` and `tar.exe` instead of PowerShell (TASK-1711), after a tester's Windows Defender quarantined the old launcher as a Trojan. If your antivirus still blocks the launcher, please do not whitelist it: tell us the threat name (Windows Security, Protection history), and install Python 3.9 or newer yourself in the meantime.

Every capture merges telnet debug, MQTT, browser devtools, the crash log and HTTP probes into one transcript. It also writes two extra files:

- `otframes.log`: raw OpenTherm frames with timing.
- `otgw_simulation.log`: the same frames in the format the replay can read, so we can replay your boiler on the bench.

Example (use your own address):

```
capture-mqtt-debug.bat --device 192.168.1.50 --duration 3600
./capture-mqtt-debug.sh --device 192.168.1.50 --duration 3600
```

Without `--device` the tool looks for gateways on your network (mDNS, TASK-1690): one gateway is used at once (after a second check that no other answers), several give a numbered list to pick from, and none brings the old question for the address. `--discover` only lists them. It reads the MQTT broker, port and user name from the gateway (TASK-1677) and prints `MQTT broker read from the gateway: ...`. It now also reads the MQTT password from the gateway's own `settings.ini` (the REST API only gives its length), and never prints it or writes it into the capture. When the gateway has an admin password it cannot read that file, and asks for the MQTT password when the broker needs one. Give `--broker` only when you want another broker: if it differs from the gateway's, the tool prints a WARNING and uses yours. With an admin password the tool also cannot read the broker, and asks for it. `--help` lists the options. Unzip the whole flash zip first: the launcher needs `otgw_capture.py` next to it.

**Capturing for days** (TASK-1523): add `--long-run`, or give a `--duration` above 2 hours (3 days: `--duration 259200`). The tool then keeps one folder per hour and zips every finished hour, with CSV files of that hour (SAT records, OpenTherm frames, MQTT). Every disconnect (device reboot, WiFi drop, broker restart) is marked with its start, end and reason. Every 5 minutes it saves the device status. It stops cleanly when the disk gets full. About 6 MB per hour before zipping and well under 1 MB zipped on our bench. Keep the PC awake.

**What to post:** at the end the tool prints `Upload this one file: ...-upload.zip`. That single file holds everything (for a short run: the transcript, `otframes.log`, `otgw_simulation.log` and the CSV files). Post it in Discord or attach it to a GitHub issue (see "How to report"). Check the files before you share them: they contain your device address, your broker address and your MQTT topics. To leave MQTT out of the capture, give `--broker ""`.

The Windows launcher is tested on our bench, also without Python installed. A tester ran the macOS launcher on a Mac (macOS 27.3) for a 2-minute and a 5-minute capture, one with MQTT, without tool errors. On a Mac without Edge or Chrome, the browser part now uses Safari (TASK-1708, pull request #699 by Crash-Evans). Switch on Safari > Settings > Advanced > "Show features for web developers", then Develop > Developer Settings > "Allow remote automation", and run `safaridriver --enable` once. Safari opens a separate window for the capture: leave it alone. This was tested on the contributor's Mac only.

---

## How to report

- GitHub: [open an issue](https://github.com/rvdbreemen/OTGW-firmware/issues) and attach the capture files.
- Discord: #english-support or #nederlandse-ondersteuning, with the capture upload zip. Beta testers keep using #beta-testing.
- SAT field results: the steps in "SAT is opt-in and off by default" above, also at https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/sat/field-run-recipe-2.0.0.md.

Thank you to everyone who tested the alphas and betas, to gruijter and maarten_41762 for the detailed feedback and the field captures, to tjfs for the OLED, OT-Direct, override, pump symbol, pressure, home dial and update page reports and the captures, to Crash-Evans for pull request #699 and the macOS capture runs, to André Brait for the pull requests (#697 and #698 in beta.118), the code size analysis and the page-load report, and to Sergeant D for the SAT design decisions.
<!-- Decided (Robert, 2026-11-18, TASK-2034): SAT is stable from 2.0.0, still opt-in; stable users report through a GitHub issue or the support channels, #beta-testing only for beta testers. -->
