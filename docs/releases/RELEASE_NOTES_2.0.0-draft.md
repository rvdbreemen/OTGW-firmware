# OTGW-firmware v2.0.0 Release Notes (draft)

<!-- DRAFT for Robert's review (TASK-1740 AC4/AC6 preparation, 2026-11-09). Not released. Every section is copied from the v2.0.0-beta.158 notes, docs/BREAKING_CHANGES.md or the named task notes. Review notes like this one are HTML comments: they do not render, and the release-2x lint should find none left before publishing. When the release happens, this file becomes docs/releases/RELEASE_NOTES_2.0.0.md (the outdated April 2026 file of that name was removed in TASK-1753). -->

**Status:** draft, not released. **Branch:** dev. **Compared with:** the 1.x line (latest stable v1.7.9).

## What 2.0.0 is

2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported: going back to 1.x means putting the Wemos D1 mini back in.

**Supported hardware:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place. The same image also runs OT-Direct, the gateway mode that speaks OpenTherm without a PIC.

The web UI is the v2 web UI, the only web UI in 2.0.0. Read "Before you flash", "BREAKING changes" and "Known issues" first.

<!-- Review: the paragraphs above are taken from "What this beta is" in the beta.158 notes, with the beta wording removed. "OT-Direct ... without a PIC" restates the Highlights; check the wording you want for stable. -->

---

## Highlights

- **v2 web UI.** The only web UI in 2.0.0. Live value table, help text for every setting (the **i** button), settings rail with card panels, SAT page. Since beta.111 each page is one gzipped file with an ETag.
- **Firmware updates over WiFi.** A small safeboot updater next to the firmware. If an upload fails or a new firmware crashes in its first 30 seconds, the unit returns to safeboot and stays reachable. One-action update page for the release zip.
- **Monitor graph with history.** The graph opens with the last hour the gateway kept (24 hours on boards with PSRAM). The control setpoint line shows what the boiler got, also when the thermostat briefly asks something else that the gateway replaces (TASK-1522).
- **Works behind a reverse proxy**, including a Home Assistant sidebar panel (ingress). Guide: [Reverse proxy and HA ingress](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md).
- **Home Assistant.** A "Visit" link to the gateway, SAT power and energy sensors for the Energy dashboard, a cumulative hot water total (`dhw_water_total`), and a "Command Dropped" sensor when the PIC never confirms a command.
- **Return water temperature every minute** from a wired sensor (PIC firmware 6.6 or later, `TS=R`).
- **Crash log endpoint.** `GET /api/v2/device/crashlog` for diagnostics. After an ESP32 panic or watchdog reset it holds a `Core dump:` line with the task, the program counter and a short backtrace (TASK-1479). `reboot_log.txt` names every ESP-IDF reset reason, including 0xb (USB reset by the host) (TASK-1438).
- **Capture tool** with raw OpenTherm frame logging (next section), and an opt-in diagnostic recorder on the device (TASK-1665), with Start, Stop and Download in the web UI (TASK-1700).
- **Relicensed to GNU GPLv3** (was MIT).

---

---

## SAT is experimental and opt-in

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- While SAT is on, it owns central heating control: `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT, REST, the boot commands or the serial bridge are refused (TASK-1695; before beta.127 only `CS=` was). `HW=`, `SW=`, `SH=` and every other command still pass.
- Some SAT decisions are still open (see Known issues). SAT behaviour can change between builds.
- If you enable SAT, please share a capture log (see "Capture tool"). SAT is only as good as the field data we get.

Without SAT, the gateway behaves as a transparent OpenTherm gateway, as in 1.x.

---

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

---

## BREAKING changes

The full log, with the reason and what to do for each item, is [BREAKING_CHANGES.md](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/BREAKING_CHANGES.md). The 2.0.0 entries there:

- Behaviour change: on OT-Direct in gateway mode `TT=` and `TC=` no longer write MsgID 16 and MsgID 100 to the boiler (ADR-223, TASK-1733)
- Changed: while SAT is enabled, external `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` are refused too (TASK-1695)
- Removed: the OT-Direct standalone controller (fixed flow, heating curve, PI room compensation, CH hysteresis) (TASK-1684)
- Behaviour change: a WiFi link without traffic triggers a reconnect, then a restart (TASK-1450)
- Behaviour change: MQTT on-change publishing is the default (ADR-116)

Changes between the 2.0.0 betas are in the release notes of each beta, the latest being the [beta.158 notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.158.md).

### Compared with the 1.x line

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

---

## Known issues

- **OT-Direct gateway mode: a `TT=` or `TC=` override has not been seen working with a real thermostat** (TASK-1718). The gateway answers the thermostat's read of the remote room setpoint (MsgID 9) and of the override flags (MsgID 100) as the PIC does; this is proven by replay tests on the host. Whether a real thermostat shows the override and raises its heat demand is not confirmed in the field yet. Robert waived that check for this release (2026-11-09).
- **OT-Direct gateway mode: the override list no longer shows an active `TT=`/`TC=`** (TASK-1749). Since `TT=` and `TC=` stopped writing MsgID 16 and 100 to the boiler in gateway mode (ADR-223, TASK-1733), Monitor > Connection and `GET /api/v2/otdirect/overrides` do not list an active remote override.
- **OT-Direct: the override flags (MsgID 100) differ from the PIC** (TASK-1750). The PIC answers with the function bits of the active override and applies `OH=`; OT-Direct answers `TT=` with a fixed value and ignores `OH=`.
- **OT-Direct master mode: `TT=0` also clears a room setpoint written over MQTT or with `BS=`** (TASK-1751).
- **OT-Direct: one queued boiler write can still go out after a mode switch** (TASK-1752). Low impact: one frame.
<!-- Review: TASK-1749 to TASK-1752 are To Do on 2026-11-09. Drop each item that is Done at release time. -->
- **With several browser tabs open the gateway sometimes answers "busy" (HTTP 503)** (TASK-1741, TASK-1747). On a board without PSRAM, two visible tabs on Monitor > Values got about 0.5 busy answers per minute on the bench (0.4 % of requests), down from 15.6 per minute before. The pages keep showing their last values, so a busy answer only makes a value one poll older. A tab you do not look at stops polling.
- **The first periodic refresh of a page can come up to 15 s later than in the betas before beta.160** (TASK-1741). Each tab now spreads its polls; a tab shown again after at least 15 s refreshes at once.
- **Rarely, a small file such as the page icon takes about 12 s** (TASK-1675). Proposed text, not yet decided: "Rarely (about 1 in 100 requests on early 2.0.0 betas, not seen since beta.116) the web UI can wait about 12 s for a small file such as the page icon when two browser tabs load at the same time on a busy gateway. Reload the page if a tab stays blank. We could not reproduce it on the current build; if it happens to you, a capture log helps us find it."
<!-- Review: TASK-1675 is In Review. Its notes give this as a DRAFT waiver text for Robert, to be decided after the R1 (boot dip) and R2 (24 h soak) two-tab traces. Keep, change or drop it after that decision. -->
<!-- Review: the beta.158 notes list more known issues (SAT validation status, the Immergas TP=11:12 proof, heat-pump cycle, diagnostic recorder on PSRAM, the OLED fix, update through the HA panel, filesystem update during a capture, and others). Decide per item which still hold for stable before release. -->

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

---

## How to report

- Discord: #beta-testing or #alpha-testing, with the capture upload zip. #beta-testing needs the Beta tester role: ask for it in #english-support or #nederlandse-ondersteuning, and until you have it, report there.
- GitHub: [open an issue](https://github.com/rvdbreemen/OTGW-firmware/issues) and attach the capture files.

Thank you to everyone who tested the alphas and betas, to gruijter and maarten_41762 for the detailed feedback and the field captures, to tjfs for the OLED, OT-Direct, override, pump symbol, pressure, home dial and update page reports and the captures, to Crash-Evans for pull request #699 and the macOS capture runs, to André Brait for the pull requests (#697 and #698 in beta.118), the code size analysis and the page-load report, and to Sergeant D for the SAT design decisions.
<!-- Review: "How to report" is copied from the beta notes and points at #beta-testing and #alpha-testing. For a stable release, decide which channels stable users should use (the support channels, a GitHub issue). -->
