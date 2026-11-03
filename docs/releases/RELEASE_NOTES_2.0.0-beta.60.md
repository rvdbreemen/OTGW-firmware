# OTGW-firmware v2.0.0-beta.60 Release Notes

**Release date:** 2026-11-03
**Branch:** dev
**Build:** 2.0.0-beta.60 (the githash is in the asset names)
**Compare:** [v2.0.0-beta.38...v2.0.0-beta.60](https://github.com/rvdbreemen/OTGW-firmware/compare/v2.0.0-beta.38...v2.0.0-beta.60). "What changed since beta.38" is new for a beta.38 user; "What changed from beta.12 to beta.38" is kept for a beta.11 user, and the sections "What changed in SAT since beta.1" and "Other changes since beta.1" for beta.1 users. Coming from 1.x: everything is new to you, read the whole document.

> **Disclaimer: this is a beta of 2.0.0.** It is not a stable release. It has had alpha and beta testing on a small group of units, but not the long field runs a release needs. What was and was not tested for this build:
>
> - beta.60 ran a 1 hour bench soak with MQTT connected, on one OTGW Classic with an S3 Mini (PIC attached). One hour is the agreed minimum for a beta (maintainer decision 2026-11-03): slow problems are for the field testers to find. That is not a 24-hour soak test, and no macOS capture run was done (see Known issues).
> - The per-task changes from beta.39 to beta.60 were host-tested and built. They were bench-checked only as part of that one soak, not one by one.
> - **No real Immergas boiler was tested.** The Immergas changes (TASK-1537, TASK-1570, TASK-1581) are covered by host tests and by watching the commands on the bench, not by an Immergas boiler.
>
> Expect bugs, and keep a way back: save your settings, and keep the flash zip of the 2.0.0 build you run now, so you can flash it back over USB. A 1.x zip is ESP8266 firmware and does not run on an S3 Mini; going back to 1.x means putting the Wemos D1 mini back in. If you need a gateway that just works, stay on the [1.x releases](https://github.com/rvdbreemen/OTGW-firmware/releases) for now.

## What this beta is

A public beta of the 2.0.0 line. 2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported.

The purpose of this beta is field data. We want more people to run it on real heating systems and tell us what happens.

**Supported hardware for this beta:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place.

Read "Before you flash", "BREAKING changes" and "Known issues" first.

---

## What changed since beta.38

Firmware commits from beta.39 to beta.60, grouped by theme. If you come from an earlier beta, the section "What changed from beta.12 to beta.38" below is kept in full.

### REST API

- **BREAKING: a form-encoded POST now gets `400 Missing value`** (TASK-1565). This is a POST with `Content-Type: application/x-www-form-urlencoded` (curl `-d` without a `-H` header, an HTML form) or a `text/plain` body of the form `key=value`. A raw body sent as `application/json` or plain `text/plain` (for example `comfort`, as the web UI sends it) works as before. Before, a form-encoded POST sent right after a JSON POST could read the body of that earlier request: `POST /api/v2/sat/settings/control_mode` with `pwm` returned the value `18.5` of a previous call. The cause: the async server parses a form-encoded body into parameters and never captures it, and the next request could be allocated at the freed address of the previous one. The captured body is now released when its request ends. **Do:** send the body as JSON with `Content-Type: application/json`, as the API documentation describes. Details under "BREAKING changes".

### SAT behaviour (Sergeant D's decisions; only when SAT is on)

- **SAT checks the age and validity of the boiler flow and return temperature** (TASK-1412.5, entries 217, 218, 241). A flow temperature (MsgID 25) without a valid boiler reply for 60 s (Data-Invalid, Unknown-DataID, no reply, or older) counts as missing: PWM goes idle and SAT sends the continuous setpoint, even while the link is alive. A return temperature (MsgID 28) that is invalid or absent for 5 minutes counts as no return temperature, so the flow + 18 start-up of a boiler without a return applies. The SAT log line `SATTICK` gets `fc=`, `fage=`, `rc=` and `rage=`.
- **A missing flow temperature clears the sustained overshoot and underheat timers** (TASK-1428, entry 218). The 0.0 of a missing flow no longer counts as a very low flow that keeps the 180 s underheat timer running. A running unit already reset PWM on a missing flow, so you will not see a change in behaviour; the fix closes the gap inside the timers.
- **The cycle verdict switches PWM on or off at flame off, as thermo-nova** (TASK-1331, entries 15 and 18). A completed continuous cycle classified OVERSHOOT now enters PWM at once instead of waiting for 60 s of sustained overshoot. A completed PWM cycle classified underheat returns to continuous. The switch goes through the same boiler permission as every other PWM entry (MsgID 3 HB bit 4): a refusal keeps SAT in continuous and shows as `pwm_veto_path` 5. Start limits, heat-pump minimum OFF, hot-water pause and cold cutoff apply as for any PWM entry.
- **PWM ends and goes continuous at the maximum it is clamped to** (TASK-1591, entry 256). The rule compared with your own ceiling, so an installer maximum below it (for example 55 against a default 62) never ended PWM at that maximum. It now compares with the effective maximum.
- **SAT sends `MM=T` at the maximum PWM duty** (TASK-1581, entries 242, 243, 246). At the maximum duty (range 5) SAT releases its own max-modulation override as the step into continuous mode (an Immergas gets `TP=11:12=80` before it). Below the maximum it sends its own `MM=` again. The `SATTICK` log line shows `mm=T`.
- **Switching SAT off gives the boiler its own max CH and DHW setpoints back and clears a pushed room setpoint** (TASK-1541, entry 240). After `CS=0`, `CH=1`, `MM=T` the hand-back sends `SH=` and `SW=` with the values the boiler showed before SAT's first `SH=`/`SW=`, then `TC=0` when SAT pushed a thermostat setpoint. It applies to a user switch-off, a lifted MQTT takeover and a safety trip. An OpenTherm loss, a missing outside temperature and a flash keep SAT enabled and restore nothing.
- **SAT shows whether the boiler accepted its Max flow setpoint (`SH=`)** (TASK-1412.13, entry 252). Write-Ack is accepted; Data-Invalid or Unknown-DataID is rejected. SAT keeps sending `SH=` as before. See "Home Assistant and MQTT" for the new topic.
- **A command the board's command ring refused is no longer counted as delivered** (TASK-1593). With the ring full, SAT's `CS=` or `MM=` was cached as sent and only recovered by the 5 s `CS=` resend or the 60 s `MM=` repair. Now SAT keeps its cache uncommitted and retries on the next tick.
- **Cycle classifier, four changes to follow thermo-nova** (TASK-1584, 1585, 1586, 1588; entries 244, 245, 248, 249). They can change a cycle verdict compared with beta.38:
  - the tail percentiles skip the warm-up of the burn (the tail starts after the smaller of 60 s and 25 % of the burn);
  - the requested setpoint is recorded for every tail second, and the cold check and the PWM underheat test use the tail median and p90 instead of one snapshot at flame off;
  - the tail percentiles interpolate between the two neighbouring values instead of taking the lower one;
  - a flame with fewer than 3 samples is not a cycle and is not recorded, and the flame-on second counts as the first sample. The value `insufficient` of `sat/cycle_class` therefore no longer appears for short flames.
- **SAT log: `SATTICK` shows the return source** (TASK-1590). New field `rsrc=OT|BRD|-` after `rage`: `OT` a fresh MsgID 28 feed, `BRD` only the PIC board sensor (PR=E), `-` none. A boiler that answers MsgID 28 with Unknown-DataID no longer reads as having no return in the log. Logging only, no control change.

### Web UI

- **After a reload the Monitor graph shows the room temperature dropout** (TASK-1578). The 24 h history the gateway keeps now stores no room value while SAT reports `room_temp_lost` or MsgID 24 has been silent for more than 270 s, so the interruption from beta.35 (TASK-1546) is also drawn for history loaded after a page reload.
- **Home: neutral wording for heat pumps and tanks** (TASK-1392). With heating source heat pump, or a boiler that reports a hot-water storage tank, the status line reads "Hot water demand · charging the tank" instead of "Hot water running · tap open". Gas boilers keep the old text. The label "RADIATORS" under the room is now "SPACE HEATING".
- **Monitor > Connection: text stays inside its box** (TASK-1427). The Thermostat and Boiler boxes are wider and the "Broker → HA" and "This browser" labels no longer run past the edge of the diagram.
- **Settings: the REBOOT badge explains itself** (TASK-1398). The orange badge on categories that need a reboot (Network) has a tooltip ("Changes in this category take effect after a reboot") and no longer looks clickable.

### Home Assistant and MQTT

- **New diagnostic topic `sat/sh_rejected`** (TASK-1412.13). A binary sensor (problem) that is ON when the boiler rejected the Max flow setpoint write. `GET /api/v2/sat/status` gets `sh_write` (`unknown`, `accepted`, `rejected`). **Do:** run discovery again (or restart Home Assistant) to see the new entity.
- **`MaxRelModLevelSetting/override` shows the active `MM=` value, as a percentage** (TASK-1389). The topic published `0.00` every minute, also after `MM=100`. It now carries the value the PIC applies (for example `100.00`), and its discovery declares unit `%` instead of device class temperature and `°C`. Details under "BREAKING changes".

### Network

- **WiFi: every connect and reconnect joins the strongest access point of the SSID** (TASK-1397). The default fast scan joined the first access point it found, so a unit could stay at -84 dBm next to a stronger access point of the same SSID. A (re)connect takes about 1 to 2 s longer. There is no roaming while connected.

**How this was tested.** Host tests on the real code, each failing on the code before the change. Examples: `tests/test_sat_flow_return_validity.py`, `tests/test_sat_overshoot_sustain.py` (old code: 3 failures), `tests/test_sat_disable_handback.py` (old code: 11 failures), `tests/test_sat_cycle_classifier.py` and `tests/test_sat_classifier_parity.py` (old code: 1 and 3 failures), `tests/test_sat_command_delivery.py` (old code: 2 failures), `tests/test_wifi_strongest_ap.py` (old code: 3 failures), `tests/test_graph_history.cpp` and the browser tests `tests/webui/graph-room-dropout.test.mjs` (old page: 2 failures in scenario 5), `connection-map-text-fits`, `home-heatpump-wording` and `settings-reboot-badge`. The bench part is described in the disclaimer: one 1 hour soak of beta.60.

---

## What changed from beta.12 to beta.38

Kept from the beta.38 notes for readers who skipped those builds.
### PIC command queue (beta.19 to beta.28)

The queue that carries commands to the PIC was reworked. It is the part of this beta that deserves the most attention in the field.

- **One command per second, oldest first** (TASK-1567, beta.19). The queue sends one command per 1 s tick, the one that has waited longest, so a burst of commands no longer reaches the PIC at once.
- **Strict first-in, first-out, with a lock** (TASK-1569, beta.20). An updated command moves to the back of the queue. `addCommandToQueue()` now reports whether the queue accepted the command.
- **Paced by the PIC echo** (TASK-1571, beta.21). One command is outstanding at a time, with a 50 ms guard and a 1 s timeout. `OE=` is resent, and an echo-latency histogram is kept. A late echo of a command that was replaced in the meantime is attributed to the replacement.
- **`PM=` and `TP=` hold the PIC's priority slot until the boiler answers their data ID** (TASK-1572, beta.22), with a 60 s timeout.
- **Queue state in `/api/v2/device/info`** (TASK-1573, beta.23). New `cmdq_*` fields show the queue state and the echo-latency histogram. Useful in a capture.
- **The command buffer holds the PIC's 15-character maximum** (TASK-1576, beta.24). A longer command is refused, never cut short.
- **SAT counts a command as sent only when the queue accepted it** (TASK-1574, beta.25). A dropped hand-back (the `CS=0`, `CH=1` and `MM=` that SAT sends when it is switched off or restarts) is retried until the queue takes it.
- **An external `CS=` can no longer pass as SAT's own** (TASK-1575, beta.28). The origin is now an argument of the enqueue call instead of a global flag. While SAT owns the control setpoint, a `CS=` from another task is refused.

### SAT behaviour (Sergeant D's decisions; only when SAT is on)

- **MM, the Immergas TP and CH go out on change, and again once a minute** (TASK-1570, beta.18, ADR-207, entry 237). Before, every control tick sent them with unchanged values included. They are also sent again after a command-cache reset (SAT takes control or hands back, the OpenTherm link comes back, boot, a PIC restart seen as the "OpenTherm Gateway" banner) and once the value is 60 s old. `CS=` is unchanged. Effect: far fewer PIC commands on a steady boiler.
- **SAT restores the whole saved PID state at any age, as thermo-nova** (TASK-1415, beta.29, entry 74). The 30-minute limit is gone. The restore still waits for the clock.
- **The 300 s hot-water guard arms on a draw between two control ticks** and counts from the real end of the draw (TASK-1429, beta.30, entries 108 and 131).
- **During a hot-water draw SAT sets MM to the maximum** (TASK-1539, beta.31, entries 220 and 232). Before, an `MM=0` from PWM stayed for the whole draw. The 30 s `CS=` keepalive keeps running.
- **Heat pumps can use a 60 minute PWM period** (TASK-1557, beta.32, entry 228). `sathpcycle` accepts 3600 s (one start per hour) next to 1800 s (default, unchanged), 2400 s and the experimental 10800 s. A hybrid never gets 3600 s.
- **SAT enters PWM only when the boiler allows it** (TASK-1412.4, beta.33, entries 213 to 215). SAT needs a valid MsgID 3 reply with HB bit 4 clear ("master low-off and pump control not allowed") before any PWM entry. Without a reply SAT stays continuous. If the bit turns 1 while PWM runs, PWM ends in that control tick. The SAT status shows `slave_cfg_valid`, `slave_low_off_denied`, `pwm_veto_reason` and `pwm_veto_path`.
- **Curve self-tune:** an hour without a room temperature is not counted (TASK-1559, beta.26, entry 225). For heat pumps the `SATTUNEH` log record gets a `pwm=<ticks>` field (TASK-1551, beta.27, entry 205).
- **PWM start counting was revalidated per source** and the SAT tick log now separates commanded, counted and observed starts (TASK-1412.9, beta.36). This is a test and logging change.

### Web UI

- **One "PWM cycle length" field in minutes for every appliance type** (TASK-1550, beta.34). Gas and Auto choose 20 min (3 per hour, default) or 15 min (4 per hour), heat pumps 30 (default), 40 or 60 min, hybrids 30 or 40 min. Stored settings, REST and `settings.ini` are unchanged.
- **The graph shows a room temperature dropout as an interruption** (TASK-1546, beta.35). While the room temperature is not valid, the room line stops and a dotted pink line runs along the bottom of the plot. The CSV export writes an empty room value for those samples. Before, the graph kept drawing the last reading. The history the gateway keeps on its own is unchanged.

### Home Assistant

- **SAT health, sync, window and fault flags become entities** (TASK-1490, beta.38). New binary sensors `sat/device_health`, `cycle_health`, `pressure_health`, `setpoint_sync`, `modulation_sync` and `ch_sync` (ON = problem), `sat/window_open`, and `sat/fallback_active`, `room_temp_lost`, `ot_link_lost` and `push_refused`. New sensors `sat/requested_setpoint`, `sat/heating_curve_recommendation`, `sat/manufacturer` and `sat/pressure_drop_rate`. New switch `sat_enabled` and select `sat_preset`. It also fixes a missing `break` that made the curve self-tune switch announce the fireplace switch as well. **Do:** run discovery again (or restart Home Assistant) to see the new entities.

### PIC command queue fix (beta.38)

- **With SAT bench simulation on, held-back commands are no longer reported as dropped** (TASK-1580, beta.38). Simulation keeps commands off the OpenTherm bus on purpose. The queue used to retry them and reported the once-a-minute gateway-mode query `PR=M` as dropped every minute, so `command_retry_exceeded` stayed ON in Home Assistant. Each held-back try also delayed the next command by 1 s. A held-back command is now removed without a retry or a drop report. With simulation off nothing changes.

### Logging and diagnostics (beta.12 to beta.17)

- **New: REST API for the telnet debug log flags** (TASK-1561, beta.12 to beta.14, ADR-206). `GET /api/v2/debug/flags` lists the flags by name (otmsg, restapi, mqtt, sensors, sat, otdirect, satble, sattrace, mqttgate, ntp). `POST /api/v2/debug/flags` with `{"name":"sat","value":false}`, or `POST /api/v2/debug/flags/sat/off`, sets one to a value. Setting the same value twice gives the same state, so a script no longer flips a flag by accident the way a telnet toggle key does. From beta.14 the two simulations (`sensorsim`, telnet key `d`, and `otgwsim`, telnet keys `s` and `S`) are in the same list; their side effects run only when the value changes. It is an admin route: with an admin password set, it asks for it.
- **The reboot log says "time not set" before NTP** (TASK-1452, beta.17). A reboot before the clock is synced was logged with a 1970 date. It now reads `time not set - reboot cause: ...`. Tools that look for ` - reboot cause: ` still find it.

### SAT, beta.12 to beta.17

- **BREAKING for Immergas owners on Auto: SAT sends the Immergas command `TP=11:12` only when you choose "Immergas" explicitly** (TASK-1537, beta.16). OpenTherm MemberID 27 is shared by Immergas and Sime, so the Auto detection also sent the Immergas boiler-parameter write to Sime boilers. Auto still shows the detected name, but sends no `TP=11:12`. **Do:** if you have an Immergas boiler and use SAT, set the manufacturer to Immergas under SAT settings (the field now carries a hint). Details under "BREAKING changes".
- **SAT Immergas: `TP=11:12` carries min(MM, 80) right before every `MM=`, and MM itself is no longer capped** (TASK-1537, beta.15; Sergeant D). Before, SAT wrote the flow setpoint into `TP=11:12` after the other commands, and cut MM itself at 80. Now every `MM=` is preceded by `TP=11:12=<min(MM, 80)>`, as thermo-nova does, only when Immergas is chosen explicitly. Since beta.18 these commands go out on change (see above).
- **SAT PWM: the start hold while a hot-water flame still burns also applies with "PWM overshoot protection" off** (TASK-1562, beta.13; Sergeant D). Since beta.11 every OFF phase stops the boiler, with the setting off too, so the hot-water flame goes out and the next ON phase can start. The start now waits for that flame with the setting off, as with it on. In a host test the start moved from 1230 s (beta.12) to 1350 s.

**How this was tested.** Host tests on the real code, each failing on the code before the change. Examples: `tests/test_sat_cmd_cache.py` (the beta.17 code fails 9 tests plus 1 error), `tests/test_sat_release_retry.py` (old code: 3 fail, 1 error), `tests/test_sat_slave_lowload.py` (the old code enters PWM in every case), `tests/test_sat_health_discovery.py` (old code: 18 failures), `tests/test_pic_pm_chain.py` (the old code lets an external `CS=` through), `tests/test_debug_flags_api.py`, `tests/test_sat_immergas_tp.py`, `tests/test_sat_pwm_anchors.py` k3 and `tests/test_reboot_log_time_not_set.py`. Browser tests: `tests/webui/graph-room-dropout.test.mjs` (old page: 6 failures) and `tests/webui/v2-settings-sat-followups.test.mjs` (old page: 5 failures). The bench part is described in the disclaimer: one 1 hour soak of beta.38.

---

## SAT is experimental and opt-in

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- While SAT is on, it owns the control setpoint: a `CS=` from MQTT, REST or the serial bridge is refused.
- Some SAT decisions are still open (see Known issues). SAT behaviour can change between builds.
- If you enable SAT, please share a capture log (see "Capture tool"). SAT is only as good as the field data we get.

Without SAT, the gateway behaves as a transparent OpenTherm gateway, as in 1.x.

---

## What changed in SAT since beta.1

These apply only when SAT is on.

- **Many SAT changes since beta.11** are listed under "What changed since beta.38" and "What changed from beta.12 to beta.38": command sending on change, the PWM permission from the boiler, the hot-water draw, the 60 minute heat pump period, the PID state restore and the Immergas `TP=11:12`.
- **SAT PWM: every OFF phase stops the boiler, also with "PWM overshoot protection" switched off** (TASK-1412.6, beta.11, Sergeant D). In each PWM OFF phase SAT sends control setpoint 10 C and central heating off, whatever that setting is. Before, with the setting off, the OFF phase kept the previous setpoint with central heating on, so the boiler could keep burning through the OFF phase. With it off, the ON phase still gets no ignition hold and no suppression. The graph lane "CH mode" is now called "boiler CH mode": it shows the boiler's own status bit (it stays on during pump overrun), not the central-heating command SAT sends.
- **The cold cutoff no longer flips every minute on mild days** (TASK-1547). With the room on target the requested setpoint can sit right on the cold setpoint (28.2 C radiators, 21 C underfloor). The cutoff switched heating off and on at every 30 s step and lit the boiler for about 60 s each time. It now has a 0.5 C band: heating stops at or below the cold setpoint as before, and starts again only above the cold setpoint + 0.5 C.
- **The pressure alarm no longer resets on every PWM OFF phase** (TASK-1480). The pressure drop rate pauses for 600 s when central heating starts or stops. "Central heating" is now the boiler's own CH-mode status bit (pump running, overrun included), instead of SAT's CH enable, which SAT itself clears in every PWM OFF phase. On a test series shaped like a field capture the false alarm episodes dropped from 7 to 0. The alarm is still display only.
- **Heat pumps: at least 10 minutes on and 10 minutes off** (TASK-1542). A heat pump in SAT PWM now runs at least 600 s per ON phase (was 180 s, the gas value) and stays off at least 600 s after a stop. Gas boilers and hybrids keep 180 s and no minimum OFF time.
- **New, experimental: "On/off heat pump, 3 h cycle"** (TASK-1542). A new appliance type in the SAT settings for on/off heat pumps with a large thermal mass (underfloor heating or a buffer tank): SAT PWM with a 3-hour cycle. It always shows a warning; on radiators you must also confirm a second one ("Not recommended without a buffer tank: the room may overheat."). Feedback from heat-pump owners is very welcome.
- **Every setpoint SAT repeats respects the maximum** (TASK-1536). SAT repeats its last control setpoint every 5 s, and every 30 s as a keepalive on the PIC. Those repeats now use the same maximum as the regular control step: the boiler's own max setpoint and the max flow setpoint you chose, or the default (62 C radiators, 50 C underfloor, 40 C heat pump). The safe fallback setpoint (45 C after 2 hours without a room temperature) now also respects the boiler's own maximum.
- **The 180 s minimum burn after a hot-water draw** protects a burning CH flame only, and a CH flame after a draw always gets it (TASK-1518, TASK-1558). Without a CH flame after the draw the phase ends on its normal ON time; a CH flame after a draw gets the full minimum burn (180 s, heat pump 600 s).

---

## Other changes since beta.1

- **A crash report keeps the whole backtrace** (TASK-1538): up to 16 return addresses instead of about 6.
- **A command you send is no longer lost when the PIC command queue is full** (TASK-1555). The gateway queues commands for the PIC (20 at most). When the queue was full, every new command was dropped, so a burst of reads (`PR=`, `PM=`, `TP=<id>:<entry>`, for example from Home Assistant or a script) could make a setting change such as `HW=`, `CS=`, `TT=` or `PS=` disappear. A write now takes the place of the newest queued read that was not sent to the PIC yet. A read into a full queue is still dropped, and nothing changes while the queue is not full.
- **S0 counter, relay output and Dallas sensors refuse a pin the board itself uses** (TASK-1552). The default pins still came from the 1.x Wemos: S0 on GPIO 12, the relay output on GPIO 16. On an OTGW Classic with an S3 Mini those are the PIC reset and LED1; on an S3 Mini Pro GPIO 12 is the I2C bus of the watchdog and the OLED. The firmware now refuses such a pin, logs one line about it, and leaves that function off. **Do:** if you use S0, the relay output or Dallas sensors, check the pin under Settings and pick a free one. Nothing changes if you do not use them (they are off by default).
- **PM= and TP= commands sent right after each other all reach the PIC** (TASK-1315). Each different PM= or TP= now gets its own queue entry; an exact repeat refreshes the queued one. The queue sends at most one PM= or TP= per second, so a burst does not reach the boiler within a few milliseconds.
- **Texts in the web UI, the REST API and the flash tools** are worded board-neutral (TASK-1552).

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

### New since beta.38

- **A form-encoded POST now gets `400 Missing value`** (TASK-1565). That is `Content-Type: application/x-www-form-urlencoded` (curl `-d` without a `-H` header, an HTML form, many quick scripts) or a `text/plain` body that looks like `key=value`. Before, such a POST that followed a JSON POST could read the body of that earlier request and apply it. Now the body is released when its request ends, and a form-encoded call has no body to read. Raw `application/json` and plain `text/plain` bodies work as before. **Do:** check scripts, Node-RED flows and Home Assistant `rest_command` entries that POST to `/api/v2/`: send `Content-Type: application/json` and a JSON body, with curl that is `-H "Content-Type: application/json"` together with `-d`.
- **`MaxRelModLevelSetting/override` changes unit in Home Assistant** (TASK-1389). It now reports the applied `MM=` value in percent (device class temperature and `°C` are gone). The entity changes unit, so its long-term statistics history may reset. **Do:** check automations that read this topic; the value was `0.00` before.
- **`sat/cycle_class` no longer reports `insufficient` for a 1 or 2 second flame** (TASK-1588). Such a flame is not recorded as a cycle, so the cycle count, the last cycle values and the 4 h / 24 h windows stay as they were. **Do:** check automations that react to `insufficient`.

### New since beta.11

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

- **No 24-hour soak test and no macOS capture run for this build** (TASK-1544), by maintainer decision, as for beta.1. beta.60 ran 1 hour with MQTT connected on one bench unit, which is not a 24-hour test. The PIC command queue rework (beta.19 to beta.28) in particular has had no long run. Long-run stability is exactly what we need your field data for.
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
