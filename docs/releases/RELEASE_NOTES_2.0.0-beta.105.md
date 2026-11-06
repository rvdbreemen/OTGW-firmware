# OTGW-firmware v2.0.0-beta.105 Release Notes

**Release date:** 2026-11-06
**Branch:** dev
**Build:** 2.0.0-beta.105 (the githash is in the asset names)
**Compare:** [v2.0.0-beta.99...v2.0.0-beta.105](https://github.com/rvdbreemen/OTGW-firmware/compare/v2.0.0-beta.99...v2.0.0-beta.105). "What changed since beta.99" is new for a beta.99 user; a beta.95 user also reads "What changed from beta.95 to beta.99", and a beta.77 user also reads "What changed since beta.77". Coming from an older beta: the [beta.77 notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.77.md) hold beta.63 to beta.77 and the [beta.62 notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.62.md) beta.1 to beta.62. Coming from 1.x: everything is new to you, read the whole document and the earlier notes.

> **Disclaimer: this is a beta of 2.0.0.** It is not a stable release. It has had alpha and beta testing on a small group of units, but not the long field runs a release needs. What was and was not tested for this build:
>
> - beta.105 is the beta.104 code plus the version change. beta.104 ran a 1 hour bench soak with MQTT connected on an OT-Direct bench unit (the OTGW Classic with an S3 Mini was running a 24 hour soak of beta.95 at the time). One hour is the agreed minimum for a beta (maintainer decision 2026-11-03): slow problems are for the field testers to find. This build was not soaked on an OTGW Classic, so the PIC path of this exact build has not had its hour; that is not a 24-hour soak test either, and no macOS capture run was done (see Known issues).
> - Each change since beta.77 has host tests on the real code that fail on the old code and pass on the fix, except the Home Assistant names and the flow dots (checked on the bench). Bench-tested on a beta.79, beta.87 or beta.94 build: the new Home Assistant names on the broker, the stylesheet served by the device, the telnet switch (off within 4 s and on again without a reboot), the page load with 3 injected `503` answers, the PIC frame decode and PIC version after the size reduction, the "No setup portal" setting with the saved WiFi away for 5 minutes (TASK-1645), the support map files rewritten in their new format, and the WiFi dead-link recovery (a forced reconnect followed by the `WiFi: connected but no traffic` restart).
> - **Not tested on a bench, or only partly:** the WiFi outage record (TASK-1446, TASK-1444) under a real WiFi drop; the skip-portal boot after a WiFi restart (TASK-1455); the OLED text of the new "No setup portal" setting (TASK-1645); the dead-link recovery's guards, its restart cap and the Ethernet case (TASK-1450); keeping graph history across a device reboot in a browser (TASK-1306); a boot with the telnet console switched off (TASK-1644); the replay fixes on a device (TASK-1649, TASK-1650); the page load in Safari and Firefox (TASK-1643); the new integer parser (TASK-1639) on OT-Direct command parsing and on every message of a real boiler; the support maps relearning from a real boiler (TASK-1391); the new SAT request for the boiler flow temperature on a PIC board (TASK-1653), which needs a thermostat that never reads MsgID 25.
> - **No real Immergas boiler was tested**, and the OT-Direct change for a thermostat in gateway mode (TASK-1600, beta.68) has not been seen on a bench with a thermostat attached.
>
> Expect bugs, and keep a way back: save your settings, and keep the flash zip of the 2.0.0 build you run now, so you can flash it back over USB. A 1.x zip is ESP8266 firmware and does not run on an S3 Mini; going back to 1.x means putting the Wemos D1 mini back in. If you need a gateway that just works, stay on the [1.x releases](https://github.com/rvdbreemen/OTGW-firmware/releases) for now.

## What this beta is

A public beta of the 2.0.0 line. 2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported.

The purpose of this beta is field data. We want more people to run it on real heating systems and tell us what happens.

**Supported hardware for this beta:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place.

Read "Before you flash", "BREAKING changes" and "Known issues" first.

---

## What changed since beta.99

The builds beta.100 to beta.104, plus the release build beta.105. Five changes.

- **Fix: the MQTT broker address survives a failed name lookup** (TASK-1451, a port of the 1.x fix TASK-1250). When the broker name could not be resolved for a moment, the gateway lost the broker until the next reboot. It now keeps the last good address for that name and connects with it, never reuses it for a different name, and closes the old session first when you change the broker name. Bench: a lookup failure fell back to the last good address within one 42 s cycle; a rename closed the old session with no publish to the old broker afterwards.
- **Fix: the MQTT debug log no longer reports a false "TCP disconnected"** (TASK-1664). Right after a reconnect the gateway publishes a burst of messages; when one could not be queued for lack of memory, the log printed "Error: MQTT TCP disconnected" up to 13 times while the connection was fine. It now prints `Error: MQTT packet not queued (connected, maxblk=N)` in that case and keeps the real disconnect reason for real disconnects. Bench: an MQTT off/on cycle logged "user disconnect" on the real disconnect and no false "TCP disconnected" in the reconnect burst. **Not seen on the bench:** the new line itself, because the bench unit had enough memory and nothing was refused; a host test on the real function covers it.
- **Fix: the web UI no longer shows invented values when data is missing** (TASK-1670). The system view showed "ΔT 14.2°" (and, until the first data arrived, sample values such as 62.3° and 1.75 bar) where the gateway had no flow or return temperature. Every live value now shows a dash until it is known. Bench: with no OpenTherm data the system view shows a dash for flow, return, outside, room and ΔT.
- **SAT: a lit burner runs at least 180 s before the cold cutoff stops it** (TASK-1652, Sergeant D's choice). With PWM, a cold cutoff that came early in an ON phase stopped the burner after a few seconds. The cutoff now waits until the burner has run 180 s since ignition; other stop reasons (hot water, a fault, SAT off) still act at once. Bench: the cutoff came 180.04 s after PWM ON, with the narration "waits for the 180 s minimum burn". **Not tested** with a real boiler or on a heat pump.
- **SAT: narration lines are no longer cut off** (TASK-1663). The longest SAT narration lines (up to 102 characters) were cut at 95 characters in telnet and the web log; the buffer now holds 127. **Not seen on the bench:** the longest line itself.

---

## What changed from beta.95 to beta.99

The builds beta.96 to beta.98, plus the release build beta.99. Three changes.

- **Fix: "No setup portal when Wi-Fi is down" now also keeps the beta fallback access point closed** (TASK-1645, GitHub #154, #131). In beta.95 the setting skipped the setup portal, but this beta build still started its fallback access point after 2 failed retries. With `wifinoportal` on, the gateway now stays offline and keeps retrying the saved network every 30 s while that network is unreachable, and no fallback access point appears. Bench: with the saved WiFi away for 5 minutes and the setting on, no OTGW SSID showed up in a scan; with the setting off, the fallback access point appeared after about 2 minutes. Not checked on hardware: the OLED text.
- **SAT asks the PIC for the boiler flow temperature when the thermostat never reads it** (TASK-1653). On a PIC board whose thermostat never reads MsgID 25, SAT had no flow temperature, because the PIC only listens. SAT now sends `PM=25` to the PIC, only when the PIC has a free slot, at most one request at a time and at least 20 s apart. It does this only after 300 s without a thermostat read of 25 and when the flow value is at least 20 s old, so it does not compete with a thermostat that reads 25 itself. If the PIC has no free slot, the request goes unanswered: this is shown as the new MQTT topic `sat/pm25_no_slot` and in new `pm25_*` fields in `GET /api/v2/sat/status`. Host tests fail on the old code and pass on the fix. **Not verified with a thermostat that skips MsgID 25**; the OT-Direct bench unit shows the new fields at 0, as designed, because there is no PIC.
- **SAT derivative gain factor is 0.063 instead of 0.07** (TASK-1656). Decided by Sergeant D: about 10 % less derivative pull during a morning warm-up. This is a deliberate deviation from the SAT reference code, which uses 0.07. On the bench the status shows `kd` 11854.08 at `kp` 22.4 (0.063 x 8400 x 22.4; 0.07 would give 13171.2). **Do:** nothing; SAT users may see a slightly different warm-up.

---

## What changed since beta.77

The builds beta.78 to beta.94, plus the release commit beta.95, which changes no code. beta.80 was prepared as a public beta but never published; its two changes are included here.

Several items come from André Brait (andrebrait): the pull requests #693 and #695, the code size breakdown in GitHub issue #694, and the report on Discord that the web UI looked broken while it loaded. Thank you. The telnet switch and the setup portal setting answer GitHub issue #154 (and #131).

### WiFi and network

- **The gateway recovers from a WiFi link that stays connected but passes no traffic** (TASK-1450, port of the 1.x check, GitHub #682). Before, the gateway only acted when the link dropped. A unit that kept its connection and IP address while the router passed no traffic stayed offline. Now the gateway pings its router 3 times every minute. After 8 failed probes in a row (about 8 minutes) it reconnects WiFi; if traffic still does not pass after 8 more, it restarts with the reason `WiFi: connected but no traffic` in `reboot_log.txt`. It does nothing while MQTT is connected, while REST requests or port 25238 data arrived in the last minute, or while Ethernet is the active link. The check only starts once the router has answered a ping, so a router that blocks ping never triggers it. After 3 such restarts it stops restarting and reconnects at most once every 15 minutes.
- **Telnet and `/api/v2/device/info` show why and when WiFi dropped** (TASK-1446, TASK-1444, GitHub #682). The gateway records the access point and channel it joined, the number of outages since boot, and for the last drop the reason (for example `200 BEACON_TIMEOUT`), the access point, how long it lasted and how long ago it started. Repeated disconnect events during one outage count as one. Drops the firmware causes itself (switching to Ethernet, the dead-link reconnect) are marked and not counted. The telnet banner gets a `Link :` and a `Drop :` line, and `device/info` gets 14 new `wifi_*` keys.
- **After a WiFi restart the gateway no longer waits in the setup portal for an access point that is still away** (TASK-1455, ADR-211). When the WiFi supervisor restarted the gateway and the access point was still away, a release build opened the setup portal for 240 s, and the gateway could not rejoin the returning access point while it ran. On the boot after such a restart the gateway now skips the portal, finishes start-up without WiFi and retries the saved network every 30 s. A power cycle, a first install, the triple reset and every other restart still open the portal as before. **On this beta build** the gain is small: the boot itself skips the beta AP fallback too, but after 2 failed retries (about 1 minute) the gateway still starts that fallback access point, which retries the saved network only every 5 minutes; see Known issues. With the `wifinoportal` setting on, this no longer applies (TASK-1645, see "What changed since beta.95").
- **New setting: no setup portal when the saved WiFi is down at boot** (TASK-1645, GitHub #154, #131). Settings, Network, Wi-Fi: "No setup portal when Wi-Fi is down" (`wifinoportal`, off by default; it applies from the next boot). With it on, a boot that cannot reach the saved network keeps retrying every 30 s instead of opening the setup portal, and the OLED shows "Triple reset to reconfigure WiFi" while offline. Without saved WiFi, or after a triple reset, the portal still opens. Since beta.99 the beta AP fallback stays closed as well with this setting on.

### Telnet

- **New setting: switch the telnet debug console (port 23) off** (TASK-1644, GitHub #154). Settings, System: "Telnet debug console" (`telnetenabled`, on by default). Port 23 has no login and its keys can switch the GPIO output, so switch it off if you do not use it. Off closes the port at once and drops a connected client, without a reboot. When you switch it on again, the gateway keeps trying to open port 23 for up to 3 minutes, because the network stack can hold the old port for about 2 minutes after it closed.
- **Telnet action keys wait until start-up is done** (TASK-1395). A key such as `q` (reload settings) sent while the gateway was still starting ran next to the start-up code, and its output got mixed into the start-up log. Until `Setup finished!`, action keys are now refused with `Key 'q' ignored: setup still running, retry after 'Setup finished!'`. The debug toggles and `h` (help) still work.

### OpenTherm

- **The support maps learn only from frames the boiler and thermostat really sent** (TASK-1391, TASK-1612). The gateway keeps maps of which messages the boiler supports and which the thermostat asks for. Answers and requests the PIC made itself (`A` and `R` frames) were counted as if the boiler or thermostat had sent them, so a message could be marked unsupported by a boiler that never saw it. Now only `B` frames count for the boiler map and only `T` frames for the thermostat map. **Do:** nothing. After the update both maps start empty once and refill from live traffic; the MQTT value topic `otgw-firmware/boiler/unsupported_msgids` is published empty once.
- **About 9 KB more free space in the firmware slot** (TASK-1639, GitHub #694). The firmware now uses an integer-only number parser; the floating point parser it pulled in is gone. On the bench the PIC frames, the PIC version and the boot counter read the same as before.

### Web UI

- **No unstyled first paint** (TASK-1643, reported by André Brait). The stylesheet now loads before the page is drawn, so elements no longer jump while the page loads. If the gateway answers `503` (busy), the page retries the stylesheet up to 8 times, as before.
- **The graph keeps the history from before a device reboot** (TASK-1306). With the Graph page open, the next 10-minute reload after a device reboot dropped everything from before that reboot, because the gateway no longer holds it. It now stays on the page.
- **The flow dots on the Connection map loop without a jump** (TASK-1609, PR #695 by André Brait).
- **Settings help texts are a little shorter** (TASK-1646), to stay within the flash budget for them. Every setting keeps its help text.

### Home Assistant

- **The SAT zone and PV-boost entities get readable names** (TASK-1608, PR #693 by André Brait). The display name was `<hostname>_SAT_Zone_1_<value>` and `<hostname>_SAT_PV_<value>`; it is now `SAT Zone 1 <value>` and `SAT PV <value>`, spaced and without the hostname, like the other entities. Unique ids and MQTT topics are unchanged, so existing entities keep their entity id and history; only the name shown in HA changes. **Do:** nothing, unless a dashboard or automation matches on the old display name.

### Replay (bench and diagnostics)

- **A replayed outside temperature no longer counts as a fresh boiler reading** (TASK-1649). The value is still shown and published; only its age stamp, which SAT uses to judge whether the boiler's outside temperature is current, ignores replayed frames.
- **On OT-Direct, a replayed PIC start-up line no longer makes SAT resend its commands** (TASK-1650). There is no PIC on OT-Direct, so the line can only come from the replay. On a PIC board it still does.

### SAT

Since beta.77 SAT control changed in beta.99 (derivative gain factor, and the PM=25 request on PIC boards; see "What changed from beta.95 to beta.99") and in beta.104 (the 180 s minimum burn before the cold cutoff; see "What changed since beta.99"). One report was checked and needs no change: hot-water flames count in the SAT 4 h and 24 h cycle windows, as in the SAT reference code and as decided by Sergeant D (TASK-1512).

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

### New from beta.95 to beta.99

One new MQTT topic, `sat/pm25_no_slot` (TASK-1653, additive), and new `pm25_*` fields in `GET /api/v2/sat/status`. Nothing is renamed or removed.

- **SAT derivative gain: warm-up behaves slightly differently** (TASK-1656). The factor is 0.063 instead of 0.07, so `kd` is about 10 % lower at the same `kp`. **Do:** SAT users, expect a slightly different warm-up and check automations or dashboards that read `kd`.
- **On a PIC board SAT may send `PM=25` to the PIC** (TASK-1653), only when the thermostat never reads MsgID 25 and the PIC has a free slot. **Do:** if you watch the PIC command stream, expect an occasional `PM=25`.

### New since beta.77

No MQTT topic, REST route or setting is renamed or removed. Additive: two new settings, `telnetenabled` (on by default, TASK-1644) and `wifinoportal` (off by default, TASK-1645), and 14 new `wifi_*` keys in `GET /api/v2/device/info` (TASK-1444). Defaults keep the old behaviour.

- **Home Assistant display names of the SAT zone and PV-boost entities change** (TASK-1608). Display only: unique ids, entity ids of existing entities and topics stay the same. **Do:** check dashboards or automations that match on the old `<hostname>_SAT_Zone_...` or `<hostname>_SAT_PV_...` names.
- **The OpenTherm support maps start empty once after the update** (TASK-1391, TASK-1612). The map files written by older builds are not loaded, and the retained MQTT value topic `otgw-firmware/boiler/unsupported_msgids` is published empty until live traffic fills the maps again. **Do:** check automations that read that list right after the update.
- **A WiFi link that passes no traffic can now restart the gateway** (TASK-1450). Only when MQTT is not connected and no REST or port 25238 traffic arrived in the last minute, after about 16 minutes without a ping answer from the router. The restart is logged as `WiFi: connected but no traffic` in `reboot_log.txt`.

### Carried over from earlier betas

The items below were announced in earlier notes and still apply if you skipped those builds.

#### From beta.77

No MQTT topic name or existing REST route is renamed or removed. These changes alter values or behaviour that automations may read:

- **BREAKING for readers of the SAT duty values: the 4 h and 24 h duty definition changed** (TASK-1412.15, entry 268). The REST fields `duty_ratio` and `24h_duty_ratio` (and the MQTT values that mirror them) are now flame-on seconds divided by the window length. Before: on / (on + off gap) over the cycles. Values drop, mostly in the first hours after SAT starts. **Do:** re-check automations, dashboards and thresholds that read the duty.
- **`mod_suppressed` is now real** (entry 267). It was always false; it turns true during the PWM ON phase suppression. **Do:** check automations that assumed it never changes.
- **`current_modulation` reads 100 while the last `MM=` sent is `MM=T`** (entry 269). It used to read 0 there. This matters for graphs and statistics of that value.
- **Webhook: a URL with port 0, a malformed port or an unresolvable host is refused** (TASK-1605). Such a URL was passed on to the HTTP client before. **Do:** if a webhook stopped firing, check its URL under Settings.
- **New route `GET /api/v2/device/coredump`** (TASK-1560). Additive, not breaking. It is an admin route: with an admin password set, it asks for it.

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

- **No 24-hour soak test and no macOS capture run for this build** (TASK-1544), by maintainer decision, as for beta.1. beta.104 (the code of this release) ran 1 hour with MQTT connected on one OT-Direct bench unit, which is not a 24-hour test of this build and not a run on an OTGW Classic. The PIC command queue rework (beta.19 to beta.28) in particular has had no long run. Long-run stability is exactly what we need your field data for.
- **Free memory slowly drops on an OTGW Classic with an S3 Mini** (TASK-1669). In a 24-hour soak of beta.95 the free heap fell by about 470 bytes per hour, in a straight line, while the largest free block stayed the same. That points at small allocations that are not freed, not at fragmentation; the cause is not found yet. That is about 11 KB per day. If your unit runs for days, please report `freeheap` and `uptime` from `/api/v2/device/info` once a day; a unit that restarts on its own also tells us something, so send the reboot log as well.
- **The skip-portal boot after a WiFi restart does not yet stop the beta AP fallback** (TASK-1455). On the boot after a WiFi restart, the boot itself skips the setup portal and the AP fallback. But this is a beta build: after 2 more failed retries (about 1 minute) it still starts its fallback access point, as all beta builds do when the saved WiFi stays away. Only the "No setup portal when Wi-Fi is down" setting (TASK-1645) keeps it closed. The setting itself has been bench-tested for a 5 minute outage, not for days.
- **The new WiFi features have not run in the field** (TASK-1450, TASK-1455, TASK-1446, TASK-1444). The dead-link recovery was seen once on the bench (forced reconnect, then the restart); its guards, the restart cap, the skip-portal boot and the outage record under a real WiFi drop are covered by host tests only. If your WiFi drops, the `Drop :` line in the telnet banner and the `wifi_drop_*` keys in `device/info` are what we would like to see.
- **A boot with the telnet console switched off** was checked in the code only (TASK-1644); switching it off and on while running was bench-tested.
- **Decoding a core dump:** the thread listing of `esp-coredump` may fail inside the tool. The decode still shows the panic reason. Send the downloaded file anyway.
- **The OT-Direct thermostat relay change (TASK-1600)** has not been seen on a bench with a thermostat attached.
- **The SAT request for the boiler flow temperature (TASK-1653) has not run on a PIC board with a thermostat that skips MsgID 25.** It is covered by host tests and was seen as inactive on the OT-Direct bench unit. If your thermostat never reads MsgID 25 on a PIC board, `sat/pm25_no_slot` tells whether the PIC had a free slot for the request.
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

Thank you to everyone who tested the alphas and betas, to gruijter and maarten_41762 for the detailed feedback, to André Brait for the pull requests, the code size analysis and the page-load report, and to Sergeant D for the SAT design decisions.
