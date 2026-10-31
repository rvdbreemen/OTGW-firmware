# Changelog

All notable changes to OTGW-firmware are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed

- **`sat/cycles_this_hour` is announced to Home Assistant with `state_class` `measurement`** (TASK-1463). It counts PWM starts in the rolling hour and falls as starts leave it, so `total_increasing` made HA read every drop as a meter reset. Topic, entity name and unique_id are unchanged.
- **SAT resets its PID integral and raw derivative where thermo-nova does** (TASK-1414, Sergeant D entry 73), main and zone PIDs: on a target that really changes (REST, MQTT, the thermostat), a preset change or clear, the window switch and restore, the SAT off to on switch, the `reset_integral` command, and for one zone when its setpoint changes. Only the integral and the raw derivative are zeroed; the update timers and the start-up state stay, so SAT still sends a computed setpoint on the first tick. Before, a target or preset change left the integral in place, a window change and `reset_integral` cleared the integral only, and the zone integrals were never reset by them.
- **SAT leaves PWM for continuous in the same tick the underheat or saturation exit fires** (TASK-1468, Sergeant D entry 134, Q47b A). The control tick now runs the PWM schedule, then the two exits, then the control setpoint, as thermo-nova does (`heating_control.py:211-228`). Before, the exits ran after the setpoint, so the exit tick still sent a PWM ON-phase CS and the continuous CS came one control tick (default 30 s) later; CH was already 1 on that tick. Exit timing is unchanged (180 s underheat, 300 s saturation); only the CS of the exit tick changes.
- **SAT PWM: a hot-water draw during the wait for the first flame, and during the 180 s minimum burn, are handled as Sergeant D decided** (TASK-1502, entry 132). A draw that interrupts the ON phase before its first central-heating flame no longer counts that wait as used ON time: the ON time and the 180 s minimum burn start at the first central-heating flame after the draw, or the ON time starts 180 s after the draw ends when no flame comes (no new boiler-temperature measurement). A short draw inside the 180 s minimum burn no longer drops the rest of it: the remaining part (draw time excluded) still applies after the draw.
- **SAT page: "Central Heating Settings" card with a Max relative modulation slider** (TASK-1514, Sergeant D entry 153). The card that held the Max flow setpoint slider is renamed and gets a second slider, 0 to 100 %, for the existing `satmaxmodulation` setting (default 100 = no cap), which already caps every `MM=` SAT sends. Home Assistant discovers it as the number entity `SAT Max Relative Modulation`, set with `set/<node>/sat/max_modulation`; its state is `sat/max_modulation`. Control behaviour is unchanged.
- **A max flow setpoint the user chose is the only cap** (TASK-1513, Sergeant D entry 152). Once the slider holds a value, `SH=` carries it and SAT caps every CS at it, without the global 65 °C SAT ceiling (within the boiler's MsgID 49 range, underfloor at most 50). Without a chosen value the defaults stay 62 radiators / 50 underfloor and the 65 ceiling still applies. For a heat-pump source the default cap without a chosen value is 40 °C on CS (still no `SH=`); a chosen value replaces it, exactly as for a boiler: `SH=` carries it and it is the CS cap (Sergeant D entry 155; before, 40 °C also cut a chosen value).
- **SAT ignites a boiler without a return temperature from the flow temperature** (TASK-1371, Sergeant D entries 45, 135, 141, 142, 149, 151). When the boiler answers MsgID 28 with Unknown-DataID and no board return sensor (`R`) has given a reading in the last 5 minutes, the PWM start-up hold is flow + 18 (the flame-off offset, clamped between the cold setpoint and the maximum setpoint) instead of a return-based hold that fell back to the floor. For the first 20 s after every central heating ignition, the relight after a hot-water draw included, SAT alternates `CS=` (even seconds) and `PM=25` (odd seconds), then hands over at once to flow - 1 suppression. The flood length follows the suppression-delay setting (default 20 s). All of this applies only to a boiler without a return temperature, in the PWM ON phase with overshoot protection on. "Without a return temperature" needs evidence from the current boot: a boiler frame that said Unknown-DataID for MsgID 28 and, on a PIC with a return sensor poll, at least one poll result with no valid reading in the last 5 minutes, so a restored file verdict alone does not count while the sensor is still being read. `PM=25` waits while the boot `PM=3/15/48` chain is pending. `MI=500` is now sent once per boot, on PIC boards only, for such a boiler (after 5 minutes of continuous detection) and at once for the Ideal, Intergas and Nefit quirk; it was sent at boot on OT-Direct boards too and almost never saw the manufacturer. Boilers with a return temperature are unchanged.
- **SAT sends the hot-water setpoint when it changes** (TASK-1376, Sergeant D entries 33, 53, 150). With SAT on and DHW control on, `SW=<setpoint>` goes out at once when the setpoint changes and once after a restart, no longer at the start of every hot-water draw.
- **SAT sends CS, MM and CH every control tick and repeats them every 5 s** (TASK-1370, Sergeant D entry 44, ADR-203), as thermo-nova, instead of only on change or every 5 min. The repeat pauses during hot water and a boiler fault.

### Changed (breaking)

- **SAT no longer rewrites the heating curve coefficient by itself** (TASK-1413, Sergeant D entry 72 Q22). The firmware-only hourly auto-tune is removed: thermo-nova has no such step, and it silently changed Kp, Ki and Kd. BREAKING: the settings `satautotune` and `satautotunerate`, the REST fields `auto_tune`, `auto_tune_active`, `auto_tune_cycles`, `auto_tune_score` and `auto_tune_rate` (`sat`, `sat/status`, `device/debug`), the MQTT command topics `set/<node>/sat/auto_tune` and `sat/auto_tune_rate`, the state topics `sat/auto_tune`, `sat/auto_tune_enable`, `sat/auto_tune_rate`, `sat/auto_tune_score` and `sat/auto_tune_active`, and the Home Assistant switch `SAT Auto Tune` are gone. The firmware empties the retained discovery config and state topics on every SAT discovery pass. An old `settings.ini` with the two keys loads without error and keeps `SATcoefficient`. The coefficient changes only through the settings, REST, MQTT and the wizard; `satautogains` and the curve recommendation stay.
- **SAT PWM cycles per hour is 3 or 4 only, default 3** (TASK-1375, Sergeant D entries 30, 52, 99). Gas boilers and hybrids, radiators and underfloor alike; the Settings page shows a choice. This narrows an accepted range: a stored 2 becomes 3, a stored 5 or 6 becomes 4 (a settings file with such a value is corrected on the next deferred write), and the same snap applies to REST and MQTT writes. Heat pumps are unchanged (heat-pump PWM cycle, no hourly limit).
- **SAT MQTT contract cleanup** (TASK-1485, Robert's decisions 2-5 in TASK-1466). Home Assistant automations or templates that read these topics need an update:
  - `sat/heating_curve_recommendation` is all lower case: `increase` / `decrease` / `hold` / `insufficient` (was `INCREASE` / `DECREASE` / `HOLD` / `insufficient`). The REST/WS field `heating_curve_recommendation` changes with it. `sat/curve_recommendation` is unchanged.
  - `sat/ssi` is removed; read `sat/summer_simmer_index` (same value, one decimal). The firmware empties the retained `sat/ssi` topic on every SAT discovery pass.
  - `sat/pressure_health` is ON on a pressure problem (was: ON = healthy), as thermo-nova and the other `*_health` topics.
  - `sat/device_health` is ON while the boiler status has not been evaluated yet since boot (thermo-nova `INSUFFICIENT_DATA`), no longer described as "boiler off". The evaluator never reports `off` after its first run, so the payloads on the wire do not change.
  - The MQTT command `set/<node>/sat/heating_curve` is renamed `set/<node>/sat/heating_curve_coeff`, without an alias: the old command is ignored. The state topic `sat/heating_curve` stays the computed curve value. The REST setting name `sat/settings/heating_curve` is unchanged.

### Added

- **Max flow setpoint slider** (TASK-1373, Sergeant D entries 48, 51, 54, 67, 100, 110, 148). The SAT page has a new slider, **Max flow setpoint**, with the boiler's own range (MsgID 49) or 20 to 80 °C when the boiler reports none. SAT never sends a flow setpoint above it, the ignition hold of PWM included. A value you choose is also sent to the boiler as `SH=` (MsgID 57) when it changes and once after a restart. Until you move the slider SAT uses 62 °C (radiators) or 50 °C (underfloor) and sends nothing to the boiler, so the installer's setting stays. New setting `satmaxsetpoint` (0 = not chosen), REST `POST /api/v2/sat/settings/max_setpoint`, MQTT command `set/<node>/sat/max_setpoint` and a Home Assistant number entity `SAT Max Flow Setpoint`. Contract change: `sat/max_setpoint` (and `max_setpoint_system` in the SAT status) now reports this ceiling instead of the fixed 62 / 45 / 40 table (underfloor default 50, was 45); the SAT status gains `max_setpoint_user`. A value above 65 is still cut to 65 by the old global ceiling (the value sent to the boiler as `SH=` is cut the same way, so they agree); a value from 1 to 19 is refused as a typo.
- **SAT gas consumption in m³/h** (TASK-1484). Two new SAT settings, `satminconsumption` and `satmaxconsumption` (gas use at minimum and maximum modulation, 0 to 8 m³/h, under SAT > Technical > Boiler), make `sat/consumption` real: minimum + modulation/100 × (maximum − minimum) while the boiler reports CH mode and the flame is on, else 0 (so a hot-water draw reads 0). This follows thermo-nova's Current Consumption sensor with the CH-mode bit its OpenTherm Gateway (MQTT) coordinator reads; in simulation the simulated flame stands in for CH mode. The topic is published only when both settings are above 0 (default 0 = off). No Home Assistant entity yet.
- **A command the PIC never confirms is now reported on MQTT and in Home Assistant** (TASK-1439, port of 1.x TASK-1431). After 5 tries the gateway drops a command, and until now only the debug log and the web log said so. The dropped command is now published on `otgw-firmware/command_dropped` (for example `TT=22.0`, not retained), `otgw-firmware/command_retry_exceeded` turns `ON` (retained) until the next confirmed command, and `/api/v2/device/info` counts the drops in `cmd_dropped`. Home Assistant gets a "Last Dropped Command" sensor and a "Command Dropped" binary sensor on the Gateway device. The gateway's own `PR=` readouts and the SAT `PM=` messages are reported when they fail, but do not clear the flag. OT-Direct boards (OTGW32) answer each command at once and never drop one. Same topics and payloads as 1.7.8-beta.8.
- **`TP=` commands are no longer replaced in the command queue** (TASK-1439). A new command replaces a waiting one with the same two-letter code, so only the last `TT=` is sent. For `TP=` that lost all but the last one; each `TP=` now waits its turn behind the others.

- **The web UI works behind a reverse proxy, including a Home Assistant sidebar panel** (TASK-1377.5). Every address the v2 UI, FSexplorer and the update page use is now relative to the page, so the gateway works under a path such as Home Assistant ingress (`/api/ingress/otgw/`, with the open-source hass_ingress integration) or `https://example.lan/otgw/` on nginx. The live OpenTherm log comes along: it is the `/ws` WebSocket on the same port, opened on the proxy's path (`wss://` on an HTTPS page). The single-flight API gate (ADR-165) now recognises the API under a proxy path too, and the update page warns instead of refusing when it is opened through a proxy. Bench (alpha.505 vs alpha.507, HA 2026.10.4 over HTTP and HTTPS, nginx): before, 13 requests per page went to the proxy root and nothing loaded; after, none do, settings, upload and the live log work, at most 2 API calls stay in flight. Firmware updates through the HA panel completed in 2 of 3 runs (one 502 during the upload, recovered by updating by IP), so update by IP when you can. Guide: `docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md`.

- **Home Assistant shows a "Visit" link to the gateway's web UI** (TASK-1377.1). The MQTT discovery device block carries `configuration_url` = `http://<gateway IP>/` (Ethernet address when wired). A full discovery republish compared before and after: 402 configs, no other field changed.

- **Return water temperature refreshes every minute from a wired sensor** (TASK-1314). On a PIC gateway with firmware 6.6 or later and the wired temperature sensor set to return water (`TS=R`), the firmware asks the gateway for the sensor value every minute (`PR=E`, a serial command, no OpenTherm traffic) and uses it as the return water temperature. Tret now refreshes about every 60 s instead of whenever the thermostat asks (about 5 minutes on a Honeywell Round). When the gateway has no reading, the last value stays. Boards without the sensor send nothing extra.
- **The Monitor graph opens with the history the gateway kept** (TASK-1296): the last hour on every board, the last 24 hours on boards with PSRAM. The graph and its CSV export used to hold only what the browser had collected since the page was opened, so the 1 h, 4 h and 24 h windows were mostly empty after a refresh. The gateway now keeps its own history of flow, return, control setpoint, thermostat setpoint, room, outside, modulation, flame, DHW and CH in a ring and serves it from the new `GET /api/v2/graph/history`. Every board has a static ring of 126 slots of 30 s (1.8 KB of internal RAM, 1 hour served). A board with PSRAM instead allocates one 10 KB PSRAM block at boot (726 slots of 2 minutes, 24 hours served) and leaves the 1.8 KB array unused; internal RAM is not used for the big ring. The v2 UI loads it when the Graph tab opens (again after 10 minutes) and merges it by time into its own 5-second samples, so holes from a sleeping laptop or a dropped WebSocket are filled too. A graph line now breaks where more than 2.5 minutes of data are missing, instead of drawing across the hole. The history lives in RAM, so it starts empty after a reboot and fills at one slot per 30 s (or 2 minutes with PSRAM); a pause for a firmware flash leaves empty slots, not a stretched timeline. Dallas sensors stay live-only: their lines start at page load. Older firmware answers 404 and the graph keeps working as before.
- **Help text for every setting in the v2 web UI** (TASK-1281). An **i** button after each setting label shows a one- or two-sentence explanation: hover or keyboard focus shows it, a click or tap pins it, **Esc** closes it. The texts are in the new `settings-help.json` next to `v2.js` (181 settings, loaded on first use). The explanation of the SAT setting `Use MQTT/REST as indoor temp source` moved from an inline block into its popup.
- **Home Assistant discovers the SAT power and energy topics** (TASK-1276). Three new sensors on the SAT device: `SAT Power` (`sat/power`, kW, `power` / `measurement`), `SAT Energy Total` (`sat/energy_total`, kWh, `energy` / `total_increasing`) and `SAT Estimated Gas Input` (`sat/energy_estimated_kwh`, kWh, `energy` / `total_increasing`), so the two energy sensors can feed the Home Assistant Energy dashboard. The manuals already listed them, but discovery never announced them. The estimated gas input stays `unknown` until `satboilerratedkw` is above 0, because the topic is only published then.
- **One-action firmware update over WiFi** (TASK-1283). On ESP32-S3 boards with 4 MB flash the `/update` page takes the release zip (or the `.ino.bin` and `.littlefs.bin` files) and a single press on **Update** runs the whole sequence in the browser: hand over to the safeboot updater, upload the firmware, wait for the new firmware, wait 35 s, upload the filesystem, verify version and filesystem hash. The page recognises the files by content and refuses USB images, the updater itself, a lone file and mismatched pairs; it reads the zip in the browser (`DecompressionStream`, no library) and falls back to "choose the two .bin files" where that is missing. It detects a rolled-back firmware, a connection that drops after a complete upload, a `Flash error` from the filesystem step, and resumes with the filesystem step when only that is missing. It warns about Ethernet units, a board type that does not match the files and files from different builds. The page states before the update that settings and sensor names are kept and what starts fresh (files uploaded in the File Explorer, the boot detection log, the MQTT topic cleanup state). The default-checked "Download backups" box and its mid-update prompt are gone; **Download a settings backup** is a separate link. The old forms stay under "Advanced". Safeboot and the OTA endpoints are unchanged (ADR-191). The first install of the release that introduces this page is still the two-step flow, because the page is served by the unit before the update. The names the page depends on are listed in `docs/guides/WIFI_FIRMWARE_UPDATE.md` and guarded by `tests/webui/update-orchestrator.test.mjs`. Thanks to gruijter for the feedback.
- **SAT MQTT state stays correct while SAT is disabled or tripped** (TASK-1284). `sat/enabled`, `sat/active`, `sat/mode`, `sat/safety_tripped`, `sat/control_mode`, `sat/preset` and `sat/fallback_active` are retained and are published when SAT is disabled, when a safety trip disables it and when it is re-enabled. A broker no longer keeps `sat/active` `true` and `sat/mode` `continuous` after SAT is off. New retained state topics: `sat/enabled`, `sat/control_mode` (the requested `auto`/`continuous`/`pwm`; `sat/mode` stays the actual mode), `sat/preset`, `sat/fallback_active`, `sat/external_temp_enable`. `sat/safety_tripped` is now retained. New command `sat/external_temp` (setting `SATexternaltemp`, REST `sat/settings/external_temp`) and Home Assistant switch `SAT_External_Temp`. Thanks to gruijter.
- **SAT target follows the thermostat setpoint** (TASK-1259). New setting `satfollowthermostat` (REST `sat/follow_thermostat`, MQTT `sat/follow_thermostat`, Home Assistant switch `SAT_Follow_Thermostat`, v2 UI). A room setpoint change on the thermostat (MsgID 16 Write-Data, edge-triggered, 0.1 C or more, no minimum time) becomes the SAT target and clears an active preset without restoring the earlier temperature. With push-setpoint on, a new setpoint within 0.25 C of the last TC= SAT pushed is the SAT echo and is ignored, a change needs two consecutive frames, and with push on it is adopted only when the previous thermostat value was within 0.25 C of the last TC=. Units with no stored value get the push-setpoint value; a stored value wins. CH enable is unchanged: SAT keeps control while enabled.
- **SAT `DHW capacity` setting** (TASK-1258). `sat/power` uses it while the boiler reports DHW active (0 = boiler capacity). The estimated gas input is now `sat/power` / efficiency; `Boiler rated input` only enables and caps it. Both values change magnitude during DHW; existing energy totals are not reset.
- **Firmware updates over WiFi on ESP32-S3 boards with 4 MB flash** (TASK-1223, ADR-191). A small updater ("safeboot") now lives next to the firmware. On `/update`, "Restart into the firmware updater" restarts the unit into it, at the same address with the stored WiFi credentials. Upload the `.ino.bin` there; the unit then restarts into the new firmware (about 15 s for 2 MB). If an upload is incomplete or interrupted, or a new firmware crashes during its first 30 seconds, the unit returns to safeboot and stays reachable over WiFi. The bootloader's app rollback handles the crash case. Safeboot asks for the admin password when one is set.
- **One-time USB migration** (`flash_otgw.bat` / `flash_otgw.sh --migrate --host <ip>`). It saves `settings.ini`, writes the new flash layout while keeping the WiFi credentials, then restores and checks the settings. See `docs/guides/WIFI_FIRMWARE_UPDATE.md`.
- v2 web UI (TASK-1223.14 to .18), ported from the classic UI:
  - a live value table with every OpenTherm, 1-Wire and S0 value, which keeps updating in PS=1 mode;
  - Dallas sensor label editing;
  - an OT-Direct status panel;
  - all 15 PIC gateway settings;
  - every device-info field on the Debug page.

- **Cumulative DHW water total for the Home Assistant Energy dashboard** (TASK-1123, TASK-1189, TASK-1190, ADR-176, ADR-181, ADR-182; GH #675). The gateway integrates MsgID 19 (DHW flow rate) into `dhw_water_total`, an auto-discovered sensor with `device_class: water`, unit `L` and `state_class: total_increasing`: the same topic and fields as the 1.x line, under faux discovery id 241. Only boiler Read-Ack frames count. The summary line a PIC sends in PS=1 mode does not (ADR-181): the PIC builds it from stored values without their age, so a stale flow would keep counting; a PIC gateway in PS=1 mode adds no water, as on the 1.x line. Answers the gateway builds itself, including OT-Direct master mode replaying cached boiler values to a thermostat, do not count, and neither does the OT-Direct PS=1 summary echo. An interval longer than 60 s between two samples adds nothing, and so does a reading no MsgID 19 frame can carry (not a number, infinite, above 128 L/min). The entity is announced just in time, as on the 1.x line: the first 60 s publish after a MsgID 19 frame has decoded on the current boot queues its discovery config and sends its value (ADR-182), so a gateway without MsgID 19 traffic shows no water entity. `POST /api/v2/otgw/reset_water_total`, or the MQTT command `set/<node_id>/otgw/reset_water_total` (a copy that arrives as a retained message after a reconnect is ignored), sets the total to 0 in RAM and in its file together and publishes 0 while MQTT can publish; during a firmware, filesystem or PIC upgrade, or while LittleFS is unavailable, the reset waits until the file can be written. Unlike the 1.x line the total survives a reboot: it is kept in `/dhw_water.json`, written when 10 L are unsaved or 15 minutes after the last write, and on every orderly restart; a power cut loses under 10 L plus up to one minute of flow, and while the total is below 100 L Home Assistant can take that drop for a meter reset and count the restored total again. The number is an estimate from sparse samples, not a metrologically valid water meter (`docs/api/MQTT.md`, DHW Water Total).

- **OTDirect TT=/TC= PIC-style remote-override semantics** (TASK-466). TT (temporary) and TC (constant) commands now write both MsgID 16 (TrSet) and MsgID 100 (RemoteOverrideFunction) flag bits, distinguishing program-priority from manual-priority overrides. TT carries an auto-clear honour-state machine that releases the override once the thermostat program resumes; TC persists indefinitely. Behaviour matches the legacy PIC firmware. User-visible: TT and TC are no longer functionally identical. Fixture rows added in `tests/otdirect_pic_parity_fixture.md`.
- **BLE temperature-sensor support on ESP32-S3 OTGW32** (TASK-487 NimBLE port + TASK-494 continuous-scan + TASK-488 HA discovery). Replaces the classic Bluedroid stack with NimBLE-Arduino 2.x (~400 KB flash freed). Async continuous scan matches the OT-Thing reference: sensors are discovered within seconds of boot. Per-MAC MQTT topics under `<TopTopic>/sat/ble/<mac>/{temp,rh,bat,rssi}` plus retained Home Assistant auto-discovery configs. Supports ATC/pvvx custom firmware (UUID `0x181A`) and BTHome v2 (UUID `0xFCD2`). ADR-092 covers the dependency adoption.
- **OTDirect command-queue coalesce-by-MsgID** (TASK-494). Multi-channel fan-in (MQTT/REST/WebUI/serial) now collapses repeated WRITE_DATA commands targeting the same MsgID into the latest value, matching PIC's "latest WRITE_DATA per MsgID wins" semantics on the OT bus. High-water mark counter exposed via telnet debug for diagnostic visibility. ADR-068 amended.

- **SAT hands the boiler back when OpenTherm is lost, and resumes by itself** (TASK-1365; Sergeant D entries 39, 47, 79). After 30 s without a boiler reply SAT sends `CS=0`, `CH=1`, `MM=T` once, then nothing until the boiler answers again; `sat/ot_link_lost` shows the state, and a loss right after boot no longer wipes the saved PID state.

### Changed
- **SAT pressure drop rate follows thermo-nova** (TASK-1480). `sat/pressure_drop_rate` and the drop-rate part of `sat/pressure_alarm` keep pressure samples for up to one hour (was about 10 minutes), as thermo-nova's pressure health sensor does (`binary_sensor.py:357-365`). The 600 s pause and the sample reset now follow the master CH enable bit (the CH request sent to the boiler) instead of SAT on/off (`binary_sensor.py:335-345`, serial coordinator), and the pause also holds across the `millis()` wrap. SAT clears CH enable in every PWM OFF phase, so in PWM every phase change pauses the drop rate and restarts the window; the full hour is reached only while CH enable stays unchanged. This is parity, not a fix for the alarm flood of TASK-1474: replaying that capture gives 9 alarm episodes (was 11, the field saw 10), the same as thermo-nova with this signal. A pump-head step of 0.3 bar after the pump stops still raises the alarm, as it does in thermo-nova; whether it should, and whether SAT means the boiler's CH-mode bit instead (thermo-nova's MQTT coordinator, 5 episodes on that capture), are open questions. Display only: the alarm sends no command. `sat/pressure_health` keeps its polarity.
- **SAT outside temperature: the external value wins for its whole valid hour** (TASK-1501, Sergeant D entry 139). An MQTT/REST outside temperature younger than 1 hour now beats the boiler MsgID 27 and the weather value; before, it lost priority after 10 minutes. Validity is unchanged (1 hour).
- **SAT zones follow thermo-nova's area aggregation** (TASK-1440; Sergeant D entry 116, ADR-199). The requested setpoint is now the higher of the main PID output and the zones output for every zone count, and a zone more than 0.3 C over its target caps it at the cold setpoint + 10 - 4 per degree over (ECO follows the main PID only). The setting `satzoneheadroom` now means the most the zones may ask above their 75th-percentile output (no effect with 4 zones or fewer); a setup without zone data is unchanged.
- **SAT PWM: the hourly start limit counts only PWM's own ON phases, and the first phase after PWM is entered is ON** (TASK-1401, TASK-1369; Sergeant D entries 43 and 59). Hot-water ignitions and relights no longer count against cycles per hour, so a shower no longer blocks heating. When SAT switches into PWM the first phase starts ON without waiting an off time; a burner that is already burning keeps burning for its ON time instead of being cut on the next tick. `cycles_this_hour` (REST, MQTT, SAT page) now shows PWM ON-phase starts only: it reads 0 in continuous mode and can drop as starts leave the rolling hour (its Home Assistant `state_class` is `measurement`, TASK-1463).
- **SAT switches PWM off on the cold cutoff and when all valves are closed** (TASK-1327; Sergeant D, 2026-10-23, as thermo-nova heating_control.py:229-234). When the requested setpoint drops to the cold setpoint or below (28.2 radiators, 21 underfloor), or all valves close, SAT resets the PWM phase, the overshoot/underheat/saturation guards and the remembered setpoint (10), sends the maximum relative modulation, and leaves PWM mode. Before, it stayed in PWM and could resume with an old setpoint such as 45.
- **SAT follows thermo-nova for the solar offset, the CH bit and the integral freeze** (TASK-1323). The solar setpoint offset now lowers the requested setpoint before the PWM or continuous step, so the PWM ignition hold is no longer reduced by it. CH is 0 with CS=10 when the requested setpoint is at or below the cold setpoint (28.2 radiators, 21 underfloor) or while the PWM OFF phase drives the CS, otherwise 1, so a held CS below the cold setpoint no longer clears CH. The PID integral freezes during solar gain only when the freeze setting is on, and then skips its reset as well.
- **Settings is back to the left rail with card panels** (TASK-1326). The collapsible-group layout of TASK-1312 is reverted for Settings only: the rail of categories (a scrolling row on a narrow screen) and the selected category's cards are back, one category at a time, with the search as before. Links into Settings (BLE Assign, the sensor roster) and a refused IP save select the right category and scroll to it, also when the settings load slowly. Monitor tab memory and the SAT page changes stay.
- **SAT page: the Continuous/PWM buttons are gone** (TASK-1322). The Burner cycle mode card on the Control view now only shows the active mode, because SAT decides when each mode is in charge. The Force PWM setting was removed again in TASK-1332 (see Removed); the `POST /api/v2/sat/mode` endpoint is unchanged for API users.
- **SAT chart renamed 'SAT control loop', with a link to the boiler history** (TASK-1311). The chart on the SAT page stays because it shows what the Monitor graph does not (SAT target and PID output); a line under it explains that and opens Monitor > Graph for the full boiler history.
- **SAT page: end-user controls first** (TASK-1310). The Thermostat view now shows the dial, presets, status and the temperature history; Hot water sits below it in a group you fold open (remembered in the browser), and Burner cycle mode moved to the Control view. The view buttons stay a level selector. The controls send exactly the same commands as before.
- **SAT turns CH off below the cold setpoint** (TASK-1316). SAT sent `CH=1` for any setpoint above 10 degrees. Rule agreed with Sergeant D: PWM ON phase keeps `CH=1` whatever CS is sent; PWM OFF sends `CS=10` and `CH=0`; continuous mode sends `CH=0` below the per-system cold setpoint (28.2 radiators, 21.0 underfloor), else `CH=1`; CH is decided before the solar offset is subtracted. The flame-off hold of a cold start never goes below the cold setpoint, so CH stays on while the boiler ignites.
- **Settings is one page of collapsible groups; Monitor remembers its tab** (TASK-1312). The left rail on the Settings page is gone: every category (System, Network, MQTT, ...) is a group you fold open and closed, with its field count, all closed by default and remembered in the browser. Search opens every group with a match and clearing it restores your own choice. Links into Settings (BLE Assign, the sensor roster) open and scroll to that group for that visit only. Monitor now remembers the tab you picked, like Advanced. The navigation concept for the whole UI is in docs/design/v2-navigation-concept.md; the SAT page follows in TASK-1310/1311.

- **Setting help texts and the replay fixture now live in the firmware image, not on the filesystem** (TASK-1294). `/settings-help.json` is served from flash (same URL, `Cache-Control: no-cache`, ETag from the firmware build, 304 on a match), and the OpenTherm replay (`POST /api/v2/simulate/start`) uses a fixture built into the firmware when LittleFS holds no `/otgw_simulation.log`; an uploaded file still takes precedence. `GET /api/v2/simulate` now always reports `available: true` and has a new `source` field (`file` or `builtin`); the `reason` field and the 409 for an unmounted filesystem are gone. This frees about 20 KiB of LittleFS (the help JSON took 5 blocks) on the 4 MB boards.
- **Default hostname is now `OTGW2`** (TASK-1293). The 1.x firmware keeps `OTGW`, so a 2.0.0 gateway and a 1.x gateway on one network no longer claim the same name in mDNS (`otgw.local`) and DHCP; with equal names one of them was renamed by the mDNS conflict check and `.local` lookups broke. A new 2.0.0 install is reachable as `http://otgw2.local/`, and its WiFi setup network is `OTGW2-<MAC>`. A unit that already has a `settings.ini` keeps the hostname saved there, usually `OTGW`: nothing is renamed on upgrade, and an over-the-air filesystem update keeps the settings. The new default applies to new installs and to any reset that discards `settings.ini` (a factory reset, or a USB filesystem flash without restoring the saved settings). On such a unit the Home Assistant device name becomes `OpenTherm Gateway (OTGW2)`. The MQTT top topic default (`OTGW`), the MQTT client id, the unique id (`otgw-<MAC>`) and the Home Assistant device identifiers are unchanged.
- **New flash layout on 4 MB ESP32-S3 boards** (ADR-191): safeboot 960 KB, firmware 2240 KB, filesystem 768 KB. **Every unit needs one USB migration** (see above). `flash_otgw --update` refuses a unit that has not been migrated yet.
- **The classic web UI is removed; v2 is the only web UI** (ADR-192). The UI switch and the classic-only `ui_*` settings are gone; older settings files still load. The File Explorer has its own small stylesheet and is shown at `/` when the web UI file is missing, so a broken UI can be repaired over WiFi.
- **Web files are minified at build time** (ADR-190), still as plain text (no gzip). The web UI files now need 620 KiB of filesystem space instead of 1460 KiB.

- **Project relicensed from MIT to GNU GPLv3.** Applies to the project's own code (Robert van den Breemen, sole copyright holder), including the root LICENSE and every source file that previously carried an MIT header/footer. Third-party vendored libraries (`src/libraries/OpenTherm` — (c) Ihor Melnyk, `src/libraries/OTGWSerial` — (c) Schelte Bron) and the LGPL-2.1-licensed portions of `FSexplorer.ino` (c) Jens Fleischer keep their original licenses and copyright notices unchanged.
- **MQTT on-change publishing is now the default** (TASK-791, ADR-116). New setting `MQTTonChangePublishing` defaults to `true`, and the publish interval defaults to `60` seconds: changed OpenTherm values publish immediately, unchanged values refresh once per minute. On upgrade, a config that still has `MQTTinterval=0` is migrated once to `60` (persisted via the deferred settings write, no boot-time rewrite). Untick "MQTT Publish On-Change" (or set `MQTTonChangePublishing=false`) to restore legacy publish-every-message behaviour. The TASK-400 status-bit heartbeat is independent and unchanged.
- **`settings.sat.iBleInterval` semantics** (TASK-494). Repurposed from "BLE scan rate" to "publish/state-update cadence". The BLE radio scans continuously on ESP32 since this release; existing user configs continue to load and round-trip cleanly. WebUI tooltip updated.
- **MQTT `resetgateway` command** now requires payload `"1"` (matching the HA-discovery `payload_press` value already shipped) and is rate-limited to one PIC reset per 5 seconds. Non-matching payloads are logged and ignored; rapid retries inside the cooldown window are silently dropped with a log line. Closes the unauthenticated-LAN reset-storm path raised by the dev review (TASK-668, port of dev TASK-661).

- **The SAT PID steps on its own 60 s timer, as thermo-nova** (TASK-1368, TASK-1270; Sergeant D entries 42, 111, 115). Before, every 30 s control tick stepped the PID, so the integral grew twice as fast; the first tick after enabling SAT now sends a computed setpoint instead of `CS=10`.

### Removed

- **SAT: the PID deadband is a fixed 0.1 °C** (TASK-1417; Sergeant D, 2026-10-27, entry 76, Q26: "fixed 0.1"). thermo-nova uses the constant `DEADBAND = 0.1` (`const.py:16`). The setting `satdeadband` (`SATdeadband` in settings.ini), the REST settings field and its `settings.sat.deadband` / SAT status `deadband` read-outs, the MQTT command and state topic `sat/deadband` and the v2 Settings control are removed. The widening of the deadband by 0.5 °C per hour while the room temperature is estimated (MQTT takeover) is gone too. A `SATdeadband` value left in settings.ini is ignored and dropped at the next save; the retained `sat/deadband` topic is cleared at the next discovery publish. The curve recommendation threshold is now a fixed 0.2 °C (thermo-nova `_ERROR_THRESHOLD = DEADBAND * 2`, `sensor.py:301`).
- **SAT: the Force PWM option is gone** (TASK-1332; Sergeant D, 2026-10-23). thermo-nova stores the option but never reads it, so SAT chooses PWM or continuous itself. The setting `satforcepwm`, the REST field `force_pwm`, the MQTT command `sat/force_pwm`, the state topic `sat/force_pwm_enable` and the Home Assistant switch `SAT Force PWM` are removed. On the first discovery pass after the update the device clears the old retained Home Assistant discovery config and state topic, so the entity disappears from Home Assistant. A settings file that still holds `SATforcepwm` loads without error; the key is ignored and dropped at the next save.
- **SAT: the PWM auto switch setting is gone** (TASK-1341; Sergeant D, 2026-10-24). thermo-nova has no such setting, so the three runtime switches always run: continuous to PWM after 60 s of overshoot, PWM to continuous after sustained underheat, and PWM to continuous on saturation. The firmware used to skip all three when the setting was off; it was on by default. The setting `satpwmautoswitch`, the REST field `pwm_auto_switch`, the MQTT command `sat/pwm_auto_switch`, the state topic `sat/pwm_auto_switch_enable` and the Home Assistant switch `SAT PWM Auto Switch` are removed. On the first discovery pass after the update the device clears the old retained Home Assistant discovery config and state topic. A settings file that still holds `SATpwmautoswitch` loads without error; the key is ignored and dropped at the next save.
- **SAT: the humidity comfort adjust is gone** (TASK-1282; Sergeant D, 2026-10-28, entry 114). thermo-nova has no such feature; humidity is only used through Thermal comfort (Summer Simmer Index). The settings `satcomfortadjust`, `satcomforthumidity` and `satcomfortmaxoffset`, the REST/MQTT fields and commands `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `comfort_offset`, `comfort_ref_humidity` and the Home Assistant switch `SAT_Comfort_Adjust` are removed. Home Assistant drops the old switch after the next discovery pass. An old `settings.ini` that still holds the keys loads without error.
- **SAT: the unused auto-gain setting is gone** (TASK-1357). `satautogains` (`auto_gains_value`) was stored and published but the PID never read it, and the manual-gain switch (`auto_gains`, `kp_manual`, `ki_manual`, `kd_manual`) had no way to be turned on. Both are removed from the settings, REST, MQTT, the Web UI and the onboarding wizard. The PID always uses the automatic gains from the heating curve, unchanged. An old `settings.ini` that still holds `SATautogains` loads without error.

These classic web UI features are not in v2 (details in `docs/history/2026-10-15-retired-classic-web-ui-features.md`):
- the UI switch button;
- the heap footer;
- log persistence across page reloads;
- resizable table columns;
- the 8-option graph window list;
- graph disconnect markers;
- auto-hiding the log on small screens;
- the `otgwDebug` console helper;
- the design showcase page;
- WiFi network scan;
- the classic display preferences;
- weather location auto-fill and the OpenWeatherMap key wizard.

- **OpenWeatherMap option and the `SATweatherapikey` setting** (TASK-1280). Open-Meteo is now the only weather source. OpenWeatherMap replaced Open-Meteo whenever a key was set instead of backing it up, parsed only the temperature, and the web UI kept showing Open-Meteo as the source. Open-Meteo needs no key and returns humidity, wind and the 24-hour forecast. A stored `SATweatherapikey` in an older `settings.ini` is ignored and dropped on the next settings save; `GET /api/v2/settings` no longer lists it, and `GET /api/v2/sat/weather/needs-setup` no longer returns `has_key`.

### Fixed

- **A dropped filesystem update no longer loses your settings** (TASK-1495). When the upload stops part-way, the filesystem is left in a state that accepts no writes, so the settings were lost at the next reboot. The gateway now formats the filesystem after an incomplete image, then writes back the settings, the kept files, the OT profiles and the SAT state from memory. The web UI files are gone until the filesystem is uploaded again; the gateway then serves its fallback page with the Flash Utility / FSexplorer upload, and `device/info` reports `fsupdateincomplete`.

- **A SAT preset and a window switch survive a reboot, and a settings save keeps your own target** (TASK-1487). The active preset, the temperature from before the preset and the window pre-target were kept in RAM only, and settings.ini stored the preset temperature as `SATtargettemp`: after a reboot the preset was gone and its temperature had become your own target. They are now kept in `/sat/sat_preset.json` (written when they change, restored at boot, kept over a filesystem update), and settings.ini always stores your own target, as thermo-nova restores preset, pre-custom and pre-activity temperature after a restart (climate.py:176-194). A window that switched to the activity preset before a reboot is restored when it closes. A preset you choose while the window switch holds the activity preset (also `none`) ends the switch and stays when the window closes; the close restores the earlier target only while activity is still the preset, as in thermo-nova (climate.py:830-832). A manual target that really changes now clears an active preset, as in thermo-nova (climate.py:567); a settings save that carries the same target keeps it.

- **A weather fetch no longer blocks the main loop for about 5 seconds** (TASK-1478; reported from a gruijter alpha.527 capture: 5.15 to 5.43 s per fetch, every `[loop-stall]` of 1 s or more fell inside a fetch). The fetch used a keep-alive connection, so the socket stayed open after the last JSON byte and the parser waited for its 5 s idle timeout. The request is now HTTP/1.0 (no keep-alive, no chunked encoding) and the parser stops when the top-level JSON object closes. That needed a second fix: skipping an unused array also swallowed the byte after its closing bracket, which for the last hourly array was the closing brace of `hourly`. Host test: the real parser on an open stream now spends 0 ms after the last byte, against 5002 ms before.
- **The SAT PV boost switch keeps its state after a reboot when set over MQTT** (TASK-1459). `set/<node>/sat/pv_boost_enabled` changed the setting in memory only, so the switch fell back after a restart; the REST API and the web UI already saved it. It now goes through the same settings path as the other SAT switches, which saves the change and still stops a running boost when it is switched off. Payloads are unchanged (`1`, `0`, `true`).
- **The old SAT "OVP Enabled" switch disappears from Home Assistant** (TASK-1460). The overshoot-protection-value calibration was removed in TASK-891.6, but its switch `sat_ovp_enabled` was not added to the list of retired SAT entities, so brokers kept its retained discovery config and Home Assistant kept showing an unavailable switch. The SAT discovery pass now empties that config and the retained `sat/ovp_enabled` and `sat/ovp_value` state topics, like the other removed SAT switches.
- **A command the PIC refuses is reported instead of being taken as confirmed** (TASK-1457, port of 1.x TASK-1442). The PIC answers a command it does not accept with an error code, such as `ZZ: NG` for an unknown command or `PR: BV` for a register an older PIC does not have. The gateway took that answer as a confirmation, so nothing was reported and `command_retry_exceeded` went back to `OFF`. A refused command is now published on `otgw-firmware/command_dropped` with the code, for example `ZZ=1 (NG)`, counts in `cmd_dropped`, turns `command_retry_exceeded` `ON` and is not retried; this includes the SAT `PM=` and `CS=` commands. Checked with the host harness on the real queue code (the old code fails the four refusal cases, the new code passes all ten).

- **The PIC reset cause is valid text again after a PIC firmware update** (TASK-1406). After a PIC reprogram the PIC can answer its reset-cause query with the single byte `0xFF`. The gateway stored and published that byte as is, so `otgw-pic/settings/reset_cause` and `/api/v2/pic/settings` carried a byte that is not valid UTF-8: a strict JSON parser rejected the whole settings reply and the v2 PR= table showed a replacement character. An answer that is not plain text is now stored as hex, here `0xFF`; normal answers such as `P` or `E` are unchanged, and the v2 table shows `0xFF`. Checked with a host harness on the real `handlePRresponse()` (the old code fails it, the new code passes); not reproduced on a gateway, because that needs a PIC in this state. Same representation as 1.x TASK-1430 (GH #688).
- **SAT's push setpoint no longer flaps the thermostat override** (TASK-1272; Sergeant D, entries 14 and 16). With push setpoint on, SAT re-sent TC= every 5 minutes; on a thermostat that refuses a remote override (a Honeywell Round) the gateway dropped it after a few mismatches, so the override went on and off every few minutes. SAT now sends TC= only when the target changes, as thermo-nova does, and never sends back a setpoint it adopted from the thermostat. Turning the thermostat dial back after an adopt is now followed again.
- **A telnet debug session no longer drains the heap** (TASK-1333). The telnet library sent every small fragment of a log line as its own network packet, so a burst of debug output (all debug switches on, as the capture script does) held about 40 KB of memory in send buffers; the lowest free heap dropped to about 1 KB. The debug console now lets the network stack bundle small writes (Nagle). In the same test the lowest free heap stayed at 24 KB. A key press on the console is now answered after about 0.1-0.2 s. The serial bridge on port 25238 is unchanged.
- **SAT on PIC boards now really asks the boiler for its member ID (MsgID 3), capacity (15) and hot-water bounds (48) at start** (TASK-1315). The three requests were queued back to back, and the command queue merged commands with the same first two letters, so only the last one reached the PIC; MsgID 3, which SAT uses to recognise the boiler brand and its quirks, was never asked. On PIC boards the requests now go one at a time, each after the boiler answered the previous one (or after 60 s), with one more try for an ID that got no answer. OT-Direct boards send all three directly as before.
- **SAT's cycle analysis no longer writes outside its memory on long cycles** (TASK-1338). The sort behind the end-of-cycle percentile used an 8-bit signed index; from 129 samples on (a continuous cycle with more than about 2 minutes of tail) it wrote before the start of its array on the stack and returned a wrong value (p90 of 19 instead of 162 in the test). The index is now 16-bit. Today that value only feeds the cycle label, auto-tune and status.
- **SAT log lines no longer get written into the web log from two tasks at once** (TASK-1337). SAT status lines (for example "Control mode: PWM") triggered from the web interface or a settings save were written into the shared WebSocket log buffer from the network task while the main loop wrote other lines into it. They now go through a small queue that the main loop sends; when more than 4 lines are waiting, the oldest is dropped and a "lines dropped" note follows.
- **Weather location and two SAT tuning values keep their precision in the settings file** (TASK-1336). Every decimal setting was saved with 2 decimals, so a weather location of 52.1355 became 52.14 (about 1 km off) on the next save, and the thermal coefficient and auto-tune rate lost their small values. The location is now saved with 5 decimals and those two values with 4; every other setting is saved as before.
- **SAT resends CH, MM and the setpoint after its command cache is reset, also early after boot** (TASK-1324). A reset (for example after the OpenTherm link comes back) was meant to force a resend, but in the first 5 minutes of uptime a value equal to the cached one was skipped, so the boiler could miss `CH=1` after an early offline-online edge. The reset now marks every cached command as stale. The values SAT sends are unchanged.
- **OTGW simulation start/stop no longer writes the log buffer from the web server task** (TASK-1321). The REST `/simulate` handler and the telnet `s` key run on the async network task and wrote the shared WebSocket log buffer while the main loop wrote OT log lines into it, so a line could be mixed into another. They now hand the event to the main loop, which sends it. Other async writers of that buffer remain (TASK-1337).
- **Weather data arrives about 5 minutes after a reboot instead of about 15** (TASK-1319; reported by gruijter). The first weather fetch waited for the 15-minute poll timer, which runs from boot. It now happens right after the 5-minute start-up wait; a failed try is retried after 60 s, then with a doubling wait up to the poll interval. On a bench unit the weather became valid after 313 s instead of 911 s.
- **A link into Settings no longer drops unsaved edits in the v2 web UI** (TASK-1318). Following a BLE **Assign** or a sensor-roster link while a Settings or SAT field was edited but not saved refetched the settings and cleared the edit. The refetch now keeps the unsaved edits and their save bar; a save or **Discard** still clears them, and the setup wizards' own answers replace an older unsaved edit of the same setting.
- **An unreachable MQTT broker no longer freezes the main loop for 3 s per attempt** (TASK-1325). The MQTT TCP connect now times out after 300 ms instead of the network library's 3000 ms default (patch in the vendored espMqttClient, build flag EMC_CONNECT_TIMEOUT_MS). With the broker down, the longest loop gap went from 3037 ms to 425 ms on a bench unit; when the broker comes back the gateway connects as before.
- **The browser's live log no longer drops its WebSocket on a non-UTF-8 byte** (TASK-1300). A PIC line with a byte above 0x7F, or a log line cut inside a multi-byte character, made Chrome close `/ws` with "Could not decode a text frame as UTF-8". The WebSocket copy of such a line now shows `?` for each bad byte; serial, telnet, port 25238 and the raw diagnose path keep the PIC bytes verbatim. Thanks to gruijter.
- **Web UI follows the minute return-water refresh** (TASK-1314 follow-up). The live graph and Home readouts took Tret only from the thermostat's own frames (about every 5 minutes), so the new minute refresh from the gateway's wired sensor showed up in MQTT but not in the browser. The page now also reads the gateway's PR: E= replies that already reach it, for a sensor set to return water only.

- **Trigger-bit options for the GPIO output and the webhook were labelled the wrong way round** (TASK-1295). The stored value is the bit number of the OpenTherm status (boiler status byte: 0 = fault, 1 = CH active, 2 = DHW active, 3 = flame), which the firmware, the 1.x UI and the REST API have always used. The v2 settings page offered that number as "Flame" for 0 and "Fault" for 3, so choosing Flame gave an output or webhook that followed the fault bit. The labels and the help texts now name the bits correctly; stored values are unchanged, so check the select after the update: a unit configured through the v2 page as "Flame" holds 0 (fault) and one configured as "Fault" holds 3 (flame), select the intended option again. An untouched GPIO output keeps its default 0 (fault); the webhook default 1 (CH active) is unaffected. The REST settings list now advertises 0 to 15 for `gpiooutputstriggerbit` (was 16), the range the firmware has always stored.
- **SAT PWM duty cycle now uses the real boiler flow temperature** (TASK-1273). The duty formula used a fixed boiler temperature of base + 20 (47.2 °C for radiators) until a learned value had passed base + 5. That value started at 0 after every boot and took samples only inside a 30 s window. The boiler temperature is now seeded from the flow temperature at the first flame, at the first ignition of each ON phase and at an ON start with the flame already burning. It is smoothed (0.3 new, 0.7 old) during the 180 s after that, held until the next ON phase seeds it, and floored at base + 1 against a division by zero, as in thermo-nova. A relight after a flame-out inside the same ON phase keeps the value. Without a flow temperature PWM stays idle and the continuous setpoint is used. A mode change or disabling SAT clears the value. Sergeant D confirmed the 180 s window. Example: requested 40 °C with a 60 °C boiler gave a duty of 0.64 and now gives 0.39.

- **SAT follows a thermostat setpoint change again on thermostats that do not mirror the remote override** (TASK-1298). With `satpushsetpoint` on, a dial change on a Honeywell Round (which reports its own setpoint, not the accepted override, and drops the override again) was only recorded, never adopted, because the rule required the previous thermostat value to match SAT's last push. A confirmed change is now adopted once SAT has pushed since boot, unless it echoes the last push. Bench: the replayed dial sequence 15 -> 19.5 -> 20 -> 19.5 moved the SAT target on the new build and not on the old.

- **SAT hot water card shows that the setpoint needs standalone DHW control** (TASK-1279; reported by gruijter). The slider only had an effect when `dhw_enabled` (Standalone DHW control) was on, and gave no hint. The card now has that toggle with a one-line explanation, and the slider is disabled while it is off. A setpoint below 30 shows as "off" with the thumb at the minimum instead of "0 C". The slider range, the settings clamp and the control code now share one range, 30 to 60 C (`SAT_DHW_SETPOINT_MIN` / `SAT_DHW_SETPOINT_MAX`); SAT control behaviour is unchanged. The boiler's MsgID 48 range is intersected with it. `GET sat/status` also reports `dhw_enabled`.
- **Monitor > Graph PNG export shows the axis labels and the legend, and the graph has a time axis** (TASK-1267; reported by gruijter). The Y-axis labels were exported black on the dark canvas because an SVG drawn as an image cannot load the page stylesheet; the legend sat outside the SVG and was never exported. The PNG now carries the labels in the colours of the current theme, the legend of the visible series and a time axis, at 2x resolution. The graph itself gets a time axis with 4 to 8 ticks on round clock times (HH:MM, dd-mm at midnight) and faint vertical gridlines; no extra API calls.
- **A filesystem update kept only the settings and the sensor names; SAT's learned values restarted at every update** (TASK-1286). With the one-action update every release rewrites the filesystem, so the SAT energy totals (`sat_energy.json`, `sat_energy_estimate.json`), the PID state, the heating-curve data and the OpenTherm profiles were lost each time. After the remount the firmware now writes the SAT state and the OpenTherm profiles back from RAM (so the energy counted since the last hourly save is kept) and restores `sat_markers.json`, `reboot_log.txt`, `reboot_count.txt` and `dallas_labels.ini` from a heap copy (2 to 3 KB per file, 8 KB in total, skipped when the heap is low; the copies are also freed when the upload fails). The `/update` page and the update guide list what is kept. The heating-curve file `/sat/sat_hcr.json` also never survived a restart on a unit that started from a fresh filesystem image: the writer and the migration did not create the `/sat` folder, which a fresh image lacks, so the write failed silently. Both now create it.
- **Home Assistant PV-boost switch and numbers were dead** (TASK-1284). Discovery pointed at `sat/boost_*`, while the firmware publishes and accepts `sat/pv_boost_*`. The state and command topics now match. Entity ids are unchanged. Run discovery again (or restart Home Assistant) to pick it up.
- **SAT log flood during a burn** (TASK-1269). Once the per-cycle flow ring was full, the 'SAT sample' debug line fired on every loop (about 3300 lines a minute, 94 % of a field capture), and the ring filled at loop rate so it covered only about 0.5 s (p90 = p10). The ring now samples at 1 Hz like the tail ring (last 256 s of a cycle) and the line is written once every 30 s. The cycle classifier keeps using the 180 s tail; the 4 h average flow (`sat/4h_avg_flow_temp`) now reflects the last 256 s of each cycle instead of its last half second.

- **Monitor > Graph plots the control setpoint the boiler was sent** (TASK-1271). MsgID 1 is taken only from Write-Data frames, because the boiler's Write-Ack echo carries no data under the spec. The gateway's R frame wins over the thermostat's T frame for 30 s. With SAT or a `CS=` override the graph now shows what the boiler got, not the thermostat's own request.
- **SAT no longer adds the +18 C flame-off offset outside the PWM ON hold, and PWM suppression is a setting** (TASK-1268). Before, every mode added `flame_off_offset` (18 C) to the setpoint while the flame was off, so PWM OFF sent CS=28 with CH=1 and the boiler relit at once (about 20 CH bursts per hour against 3 cycles per hour), and continuous start-up jumped to PID+18. Now the offset applies only while PWM waits for the flame (return + offset), as in thermo-nova. New setting `overshoot_protection` (`SATovershootprotection`, MQTT `sat/overshoot_protection`, HA switch, default on, all heating sources): with it on, PWM OFF sends CS=10 and CH=0, PWM ON holds return + offset, then sets flow - `mod_sup_offset` and refreshes it about every 3 s; with it off, PWM sends no CS override (CS keeps its previous value and CH stays on; only MM=0 acts). MM is 0 in both PWM phases; heat pumps and hybrid keep the configured maximum modulation (never 0); Geminox gets MM=10 instead of 0. The continuous-to-PWM auto-switch now compares the flow temperature with the requested (PID) setpoint, and the overshoot margin default is 2.0 C (was 3.0 C; stored values are kept).
- **SAT on Classic (PIC) boards: the boiler setpoint no longer lapses between SAT updates** (TASK-1236). The PIC drops a `CS=` override about 63 to 126 seconds after the last one, while SAT only sent a new setpoint every few minutes, so the boiler fell back to the thermostat's setpoint in between. SAT now re-sends its setpoint every 30 seconds on PIC boards. While SAT is enabled it owns the control setpoint: a `CS=` from MQTT, REST or the serial bridge is refused, as on OT-Direct boards. Bench: 22 acknowledgements in 660 s with the longest gap 34 s (before: one in 330 to 400 s).
- **The v2 heating colour was missing** (TASK-1225). A CSS comment closed early and dropped the `--hot` colour token.
- **The File Explorer fallback now also works when the web UI file is lost at runtime.** Before, `/` only fell back when the file was missing at boot.
- **The File Explorer's Update Firmware button was hidden on narrow screens.** It is now shown on phones too.
- **`flash_otgw.bat` returned exit code 0 on some errors** inside a block.

- **Answers the gateway made itself counted as boiler evidence** (TASK-1185, follow-up of TASK-1086). An OT-Direct answer the gateway built (SR= table, master-mode WRITE-ACK echo or cache replay, loopback mode) no longer marks a message id as acknowledged in `GET /api/v2/otgw/ot-support`, and its type-7 OT log line reads `(gateway answer)` instead of `(boiler does not implement)` or `(boiler rejected write)`. A loopback B frame no longer switches SAT simulation off, and on a combo board running OT-Direct in loopback the SAT availability gate no longer reports a real boiler, so the simulation enable is not refused with HTTP 409 (ADR-117 section 2). PIC frames, the PIC's proxy answers (ADR-103) and genuine boiler frames count as before. Lines the `/otgw_simulation.log` replay injects are no boiler evidence either: they are decoded and published and keep `boiler_connected` up, but set no verdict or acknowledged bit and no longer switch SAT simulation off. `/ot-boiler.json` moves to format 2: the first boot on this version ignores a format-1 file once, so false verdicts and acknowledged bits from older builds disappear, and live OpenTherm traffic relearns the real ones within minutes.
- **Telnet and ser2net discarded client payload at accept, teardown and slot release** (TASK-1145, SimpleTelnet `260aca5`). The vendored SimpleTelnet submodule moves off the diverged `123106f` (GPLv3 relicense, no fixes) onto upstream `main`, which carries the relicense *and* the fixes. Two are behavioural. `_flushTx(idx)` now runs before `_tx` is cleared on slot release, so bytes that `write()` already reported as accepted get one bounded chance to leave instead of being destroyed along with our own ring buffer ahead of lwIP. The unconditional inbound-discard loop at accept and teardown is gone: it could not tell an IAC sequence from payload, so a client that pipelined a command with `connect()` lost it, and on the raw ser2net/OTmonitor bridge (ADR-143) — where there is no negotiation to strip at all — it could only destroy user data. Telnet negotiation remains handled per byte by `_filterByte()` on the read path. The third change replaces the deprecated `AsyncClient::close(bool)` with `close()` (the parameter is documented "ignored" and the overload forwards to `close()`), removing ~27 deprecation-warning instances per build.

- **HA `DHW Control` / `Thermostat` / SAT / sensor entities flapping `unavailable`** (TASK-608, ADR-102; 2.0.0 port of dev TASK-607/ADR-074, PR #583). HA entity availability (`avty_t` = the base `<toptopic>/value/<nodeid>` topic) now reflects only the ESP↔MQTT link (birth/LWT) instead of OpenTherm-bus liveness: `publishOTGWConnectedState()` no longer overwrites the base topic with the `state.otBus.bOnline` 30 s wall-clock window, which flapped every HA entity when OT traffic was bursty or the clock unstable. OT-bus liveness remains on the dedicated `otgw_connected` sensor (unchanged). ESP32-S3's faster NTP makes the trigger rarer than on ESP8266, but the defect/fix are platform-independent. **Contract change:** consumers that read the base topic as OT-bus liveness must migrate to the `otgw_connected` sensor.

- **`bleSensorPublishHaDiscovery` sticky on transient failure** (TASK-489 + TASK-493). The per-MAC `bDiscoveryPublished` flag was set to `true` even when MQTT was not yet connected at first BLE scan, permanently silencing HA discovery for that sensor. Helper now returns `bool`; caller gates the flag on success and retries on the next iBleInterval cycle. Slot-recycling reset added so a recycled slot for a new MAC re-publishes discovery.
- **`setRemoteOverride()` float-narrowing UB on out-of-range setpoints** (TASK-491 sign-extend + TASK-495 clamp). Cast through `(int16_t)` to preserve f8.8 sign; range-clamp `celsius` to `-40..127` before the cast.
- **`feedWatchDog()` only on success path in BLE HA-discovery** (TASK-496). The 16-publish first-scan burst could starve the watchdog when broker timeouts piled up. Watchdog now fed before every `return false` after a network attempt.
- **NimBLE `_bleSensors[]` cross-task data race on ESP32-S3** (TASK-497). BLE scan callback (BLE host task on core 0) and Arduino loop task (core 1) accessed shared slot data without synchronisation. Added `portMUX_TYPE` snapshot pattern: scan callback locks during slot updates, loop task takes a snapshot under the lock and processes outside. Critical sections stay short (one struct copy ~ 40 bytes). ADR-090 amended for the FreeRTOS cross-task case.
- **`platformio.ini` auto-format scope creep** (TASK-494 / 1B-H1). The build-rationale prose-rich layout was restored after an inadvertent auto-formatter pass erased ~90 lines of comments and silently bumped `tool-esptoolpy` from `~1.30000.0` to `^2.41100.0`. Original pin restored.
- **`evaluate.py` PROGMEM gate false positives** (TASK-482). The gate now skips lines following backslash-continued `#define` macros and excludes the auto-generated `*.ino.cpp` (Arduino preprocessor concatenation). Baseline drops from 15 to 14 violations and is now stable across builds.
- **`bleMacToCompact` dead duplicate consolidated** (TASK-492). The parallel-agent execution left two implementations of the same MAC-compact helper; the canonical one in MQTTstuff.ino is now exported via `OTGW-firmware.h` and the duplicate in SATble.ino is removed.
- **WiFi association without DHCP/IP after reboot** (TASK-432, ported from `dev`). Removed three `platformRestartDHCP()` call sites in `networkStuff.ino` (startWiFi catch-all, WIFI_RECONNECTED, startNTP) and the function itself from `platform_esp8266.h` and `platform_esp32.h`. Calling `wifi_station_dhcpc_stop/start` while the station is connected takes DHCP ownership away from the SDK and breaks subsequent `setAutoReconnect()`-driven DHCP. Returns to v1.2.0 baseline pattern: SDK manages DHCP autonomously.
- **MQTT publish gating per source topic** (TASK-478, ported from `dev`). Implements ADR-066: the base topic `{prefix}/value/{id}/{label}` publishes only thermostat-side intent (Read-Ack and Write-Data); slave Write-Ack values no longer flow to the base topic. The `/boiler` subtopic (when `bSeparateSources=true`) is gated by a per-MsgID `bSlaveEchoesValue` flag for the six OpenTherm v4.2 messages whose slave Write-Ack data byte is per-spec undefined (`Tr` 24, `TrSet` 16, `MaxRelModLevelSetting` 14, `TrSetCH2` 23, `TRoomCH2` 37, `RFstrengthbatterylevel` 98). Spec-audit covering all OT v4.2 MsgIDs lives in `docs/api/MQTT-message-id-echo-audit.md`.

- **SAT no longer detects solar gain without a valid sun elevation** (TASK-1408). With no elevation, or one older than 1 hour, solar gain stays off and its hold is cleared; before, the indoor rise rate alone could switch it on.

### Added

- ADR-066 documenting the source-aware MQTT publish gating decision (ported verbatim from `dev`)
- `docs/api/MQTT-message-id-echo-audit.md` spec-audit reference per OpenTherm v4.2 (ported from `dev`)
- `bSlaveEchoesValue` field on `OTlookup_t` populated for every MsgID
- Added .github/workflows/dependency-scan.yml weekly outdated-deps visibility job (TASK-501 4B-M3)
- Added tools/generate_release_sha256sums.py + docs/MANUAL.md release-verification section (TASK-501 4B-M4)
- Added docs/templates/hardware-verification-log.md template for AC evidence capture (TASK-501 4B-M5)
- Polished 8 comment / `static_cast` / named-constant Lows from Phase 1-4 review (TASK-502): BLE_STALE_MS static_assert tripwire, malformed-MAC debug log in satBLEMacToCompact, NimBLE 0.625 ms tick conversion comment, OT_OVERRIDE_*_DELTA_F88 float-truncation foot-gun note, satBLEPublishHaDiscovery / OTGW-firmware.h "publish cycle vs scan" wording refresh, named BLE_SCAN_INTERVAL_TICKS / BLE_SCAN_WINDOW_TICKS / BLE_SCAN_MAX_RESULTS constants, static_cast<int8_t> for getRSSI(), shared BLE_MAC_COMPACT_SIZE constant.

### Documentation

- **The SAT manual chapters and the MQTT guides match the code again** (TASK-1368, TASK-1365, TASK-1282, TASK-1454). The manuals describe the 60 s PID timer and the OpenTherm-loss hand-back, and MQTT_LWT.md now says that a command sent while the gateway reconnects is lost.

## [2.0.0-beta] - 2026-04-11

### Added

- **Dual-platform support**: Single codebase targeting both ESP8266 and ESP32 with a unified platform abstraction layer (`platform.h`, `platform_esp8266.h`, `platform_esp32.h`).
- **OTDirect (OTGW32)**: Direct GPIO OpenTherm bus communication on ESP32 using the Phunkafizer OT library, bypassing the PIC controller. Full OT-Thing parity including protocol handshake, `CC=` command support, self-disabling poll, and command ring buffer.
- **W5500 Ethernet support (OTGW32)**: Hardware Ethernet with automatic WiFi/Ethernet failover. Unified network access via `isNetworkUp()`, `getActiveIP()`, `getActiveMAC()` helpers.
- **SAT (Smart Autotune Thermostat)**: Embedded heating controller with PID control, heating curve advisor (INCREASE/DECREASE/HOLD), adaptive stalled-ignition thresholds, multi-zone PID with MQTT room sensor inputs, DHW setpoint slider, boiler gas consumption estimation, Summer Simmer Index thermal comfort calculation, and MQTT humidity input.
- **OLED display module**: SSD1306 OLED support with hardware platform info display in UI and MQTT.
- **BLE (Bluetooth Low Energy)**: SAT BLE interface for wireless configuration and status.
- **AP fallback mode** (beta builds): When no configured WiFi is available, the device opens a fallback access point (`OTGW-<MAC>`, IP `192.168.4.1`, password `otgw123`) rather than going offline.
- **WiFi signal quality bars**: Visual signal strength indicator in the Web UI header using quadratic RSSI-to-percentage mapping.
- **WiFi SSID display and Reset WiFi button**: Settings page now shows the connected SSID and provides a one-click Reset WiFi button.
- **SAT WebSocket status events**: WebSocket events for flame, mode, and curve changes.
- **SAT opt-in browser console logging**: `window.SAT_DEBUG` flag enables verbose SAT diagnostics in the browser console.
- **Full OpenTherm MsgID coverage**: Extended message ID handling and protocol handshake support.
- **OTGW32 REST/MQTT parity**: OTGW32-specific endpoints matching PIC-based API surface.
- **ELF file in build artifacts**: `.elf` output included in CI artifacts for crash debugging.
- **PlatformIO support**: `platformio.ini` added for ESP32 builds alongside Arduino CLI builds.

### Fixed

- **OTGW Answer Thermostat MQTT routing**: Messages were published to the wrong source topic; now correctly routed to the boiler source topic.
- **NTP last-sync guard**: `NtpLastSync` now ignores bogus SDK initial time value on first boot.
- **ESP8266 and ESP32 compilation**: Multiple compilation errors resolved; ESP32 compatibility fixes in OTDirect and SATble.
- **SAT LittleFS migration**: `satMigrateFile()` was passing PROGMEM pointers to LittleFS, causing Exception 3 crash.
- **SAT stack pressure**: Stacked 556-byte local buffers in weather JSON function flattened to reduce stack pressure.
- **collectHeaders compile error**: `#ifdef` guard restored for `collectHeaders` on ESP8266 Core 3.x.

### Changed

- **Settings/state architecture**: 62+ flat globals replaced with `OTGWSettings` and `OTGWState` structs (ADR-051).
- **Network abstraction**: All code uses `isNetworkUp()`/`getActiveIP()` rather than direct `WiFi.*` calls.
- **SAT subsystem**: Reorganized LittleFS layout under `/sat/`, added timestamps and stale detection.
- **SAT window ring buffer**: Reduced to 30 slots on ESP8266 to save 720 bytes SRAM; expanded on ESP32.
- **Version**: Bumped to `2.0.0-beta`.

---

## [1.3.5] - 2026-04-06

### Added

- **MQTT uptime and version publishing**: Firmware publishes uptime and version info to MQTT on connect for better device visibility.

### Fixed

- **WiFi reconnection regression** (#530): The WiFi state machine timeout (5 s) was too short for ESP8266 association (5-10 s+), causing repeated connection cancellations. Timeout increased to 30 s, restoring the behavior of v1.2.0. Devices that went offline periodically since v1.3.0 should now stay connected.

---

## [1.3.4] - 2026-04-01

### Added

- **Thermostat-only MQTT support**: OTGW now stays online via MQTT when only a thermostat is connected; a boiler is no longer required.

### Fixed

- **MQTT throttle slot suppression**: Stable values such as Room Temperature could become permanently suppressed after a transient publish failure. The throttle slot is now only updated after a successful publish.
- **Debug Information page tooltips**: Tooltips were defined but never wired up to the device info labels; now visible on hover.

### Changed

- **Renamed "OTGW Connected" to "OpenTherm Active"**: Clearer label with updated tooltip.

---

## [1.3.3] - 2026-03-31

### Added

- **PIC-less OTGW support**: All PIC functions are automatically disabled when no PIC is detected at boot, with auto-recovery if the PIC appears later. REST API returns 503 and the UI hides PIC-specific elements.

### Fixed

- **Dashboard showing unsupported OT values**: Empty or zero values for message IDs the boiler does not support are no longer displayed; only valid responses are tracked and shown.
- **Gateway mode detection**: Non-gateway PIC firmware now correctly shows "N/A" instead of continuously polling.
- **"Home Assistant Integration" label**: Renamed to "OTGW Connected" with an accurate tooltip describing OTGW online status.

---

## [1.3.2] - 2026-03-29

### Fixed

- **File delete reliability**: The delete handler used the global `cMsg` buffer shared with MQTT, webhooks, and settings. Background tasks could overwrite it mid-operation, causing "Failed to delete file" errors and unresponsive behavior. Now uses a dedicated local buffer with proper HTTP status codes.
- **File listing**: Replaced the RAM-heavy `dirMap[]` array and bubble sort with direct LittleFS streaming. Sorting and size formatting moved to the frontend. Hidden files (`.` prefix) are now filtered out.

### Changed

- MQTT LWT developer documentation added.

---

## [1.3.1] - 2026-03-28

### Fixed

- **CS override interference**: Setpoint commands (`CS=`) from Home Assistant/MQTT are no longer intermittently overridden by the thermostat's lower setpoint. PIC settings readout triggers are now whitelisted to specific commands only.
- **PR command queue matching**: Responses like `PR: S=16.00` now correctly remove `PR=S` from the queue instead of the first `PR=` entry found, preventing unnecessary retries and queue slot waste.
- **PR=A banner dequeue**: The PIC banner response is now properly detected and removes `PR=A` from the command queue, preventing 5 unnecessary retries.
- **`strstr_P` crash**: Inverted PROGMEM/RAM arguments in the PIC settings trigger check caused an Exception (2) crash.
- **SC= time-sync bypassed queue**: The time synchronization command now goes through the command queue like all other PIC commands, preventing serial bus collisions.
- **UI footer overlap**: The log window no longer overlaps the status bar in Firefox/LibreWolf when stretched to the bottom of the screen.

### Added

- **Ser2net awareness**: The command queue detects traffic from port 25238 (OTmonitor) and pauses queue processing for 2 seconds to avoid conflicting commands. Conflicting queue entries are automatically removed.

### Changed

- **Non-blocking PIC queries**: `PR=` settings and gateway mode queries are now fully asynchronous, no longer blocking the HTTP server.
- **Queue code**: Deduplicated queue removal logic into a single `removeFromCmdQueue()` helper.

---

## [1.3.0] - 2026-03-26

### Added

- **PIC gateway settings panel**: All 15 PIC configuration registers (setpoint override, GPIO, LEDs, tweaks, smart power, thermostat detection, voltage reference, etc.) exposed via REST API (`/api/v2/pic/settings`), MQTT (`otgw-pic/settings/*`), and a new "Gateway Settings" section in the Web UI. Human-readable formatting, color-coded live/cached indicators, and browser localStorage caching per hostname for up to 7 days.
- **Single-click GitHub release OTA**: The update page lists GitHub releases with Installed/Update/Rollback badges. One-click download and flash with semver-aware version comparison including pre-release tags and Intel HEX integrity validation for PIC firmware.
- **Optional HTTP Basic Auth**: Settings, maintenance, file-management, reboot, and OTA routes can be protected with an admin password via the Protected Endpoints Password setting.
- **Configurable MQTT publish gating**: OpenTherm and `PS=1` summary data can be rate-limited to reduce MQTT broker load and WiFi chatter, with automatic status republish after boot and reconnect.
- **Full `PS=1` summary integration**: `PS=1` output is now parsed into the normal data pipeline, published to MQTT, and exposed through Home Assistant discovery.
- **Monitor-page command bar**: Send one-shot OTGW PIC commands (`TT=20.5`, `GW=R`, etc.) directly from the Web UI with command/response/error feedback in the log.
- **Light/dark theme toggle button**: Switch between themes with a single click; preference persists across sessions.
- **Triple-reset WiFi recovery**: Three quick hardware resets reopen the captive portal and clear stale WiFi credentials without requiring a reflash.
- **Safer OTA/LittleFS flashing**: Reboot verification via `/api/v2/health`, browser backups of `settings.ini` and `dallas_labels.ini`, WiFi reconnect suppressed during flash writes, full partition erase before write.
- **OTGW simulation mode**: Test firmware and Web UI without physical hardware. SIMULATION badge shown in the monitor header.
- **Crash log endpoint**: `/api/v2/device/crashlog` exposes ESP8266 crash information for diagnostics.
- **OTGW event reporting**: PIC restart, serial overrun, and RX errors forwarded via MQTT and WebSocket.
- **Heap memory in status**: Free heap and fragmentation shown in the Web UI footer and device info API.
- **GPIO conflict detection**: Conflicting GPIO pin assignments detected and warned about at boot.

### Fixed

- **ESP hostname reverting to ESP-XXXXXX**: Deep audit and fix of all hostname code paths; hostname now set before `WiFi.begin()`, before/after `configTime()`, and after SDK auto-connect.
- **OTA filesystem corruption**: WiFi reconnect suppressed during flash writes; full LittleFS partition erased before writing.
- **IP validation**: Only `255.255.255.255` is now rejected; valid addresses containing a `255` octet are no longer blocked.
- **Boot-time restart cleanup**: Prevents spurious service restarts immediately after startup.
- **Webhook payload truncation**: Buffer widened to accommodate full webhook payloads after reboot.
- **File Explorer delete**: File deletion works consistently again.
- **NTP hostname**: `startNTP()` moved after `startWiFi()` so the configured hostname is active.
- **MQTT subscription truncation**: Buffer size increased for long topic strings.
- **WiFi portal false trigger**: Stale RTC data after USB flash no longer triggers triple-reset detection.

### Changed

- **ArduinoJson removed**: All JSON paths use bounded manual handling, reducing flash and RAM usage.
- **Settings/state reorganized**: 62+ flat globals replaced with `OTGWSettings` and `OTGWState` structs.
- **String class eliminated from hot paths**: Protocol, settings, and HTTP handlers use `char[]` buffers, reducing heap fragmentation.
- **MQTT autodiscovery optimized**: Streaming template rendering and in-place line parsing replace bulk buffer allocation.
- **Non-blocking WiFi reconnect**: State machine replaces the blocking 30-second reconnect loop.
- **REST API v2 completed**: Dispatch table routing; all remaining v1 calls migrated.
- **Security hardening**: Centralized auth for all POST/PUT in API dispatcher; CORS wildcard replaced with dynamic origin; webhook SSRF prevention via DNS resolution; CSRF validation rewritten without String class; XSS fix in statistics table.
- **Dead code removal**: ~450 lines of legacy v1 JSON functions, unused helpers, dead enums, and stale CSS removed.
- **Stack pressure**: ~1,400 bytes freed via centralized `otTopic` buffer and static local buffers in hot-path functions.
- **WiFiManager**: Upgraded from 2.0.15-rc.1 to stable 2.0.17.

---

## [1.2.0] - 2026-03-02

### Added

- **Comprehensive Home Assistant auto-discovery**: All OpenTherm message types automatically exposed to Home Assistant — 309 discovery configurations across 80+ message IDs covering heating, cooling, solar thermal, DHW, ventilation/heat recovery, CH2, relative humidity, operational counters, and system status. No manual YAML required.
- **Webhook support**: Configurable outbound HTTP call triggered on any OpenTherm status bit change (e.g. flame on/off). Separate URL, payload, and content type for on/off events. Restricted to local network URLs; disabled by default.
- **Source-separated MQTT topics**: Optional `mqttseparatesources` setting publishes per-source topics (`<metric>/thermostat`, `<metric>/boiler`, `<metric>/gateway`) alongside the existing unsuffixed topics for backward compatibility.
- **OpenTherm v4.2 protocol alignment**: Added missing message IDs 39 and 93-97; corrected direction flags, type semantics, and units for several IDs; added legacy ID compatibility profile (`AUTO` / `V4X_STRICT` / `PRE_V42_LEGACY` modes) for IDs 50-55 and 58-63.

### Fixed

- **`MQTTseparatesources` not persisted**: Setting was written to `settings.ini` but never read back on boot, silently resetting to `false` after every reboot.
- **Gateway mode parsing**: Fixed `PR=M` response parsing (`M=G`/`M=M`); added explicit detecting/unknown state.
- **MQTT/HA topic typos**: `eletric_production` → `electric_production`, `solar_storage_slave_fault_incidator` → `solar_storage_slave_fault_indicator`, and several `vh_*_ventilation_*` spelling fixes.
- **HA discovery mismatches**: `vh_configuration_*` now keyed to correct message ID 74; `Hcratio` discovery `stat_t` corrected; `FanSpeed` split into `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` (Hz).
- **MQTT autoconfig re-entry**: Shared static buffer workspace and scoped re-entry lock prevent buffer clobbering during concurrent auto-configuration runs.

### Changed

- **REST API v0 and v1 removed**: All `/api/v0/` and `/api/v1/` endpoints now return **410 Gone**. Only `/api/v2/` remains.
- **ArduinoJson dependency removed**: Settings I/O replaced with lightweight streaming helpers (`wStrF`, `wBoolF`, `wIntF`, `parseSettingsLine()`), reducing flash and RAM.
- **Device info API key renames**: `gatewaymode`/`mode` → `otgwmode`; `wifiqualitytldr` → `wifiquality_text`.
- **Runtime safety hardening**: OT map bounds checks in parser and REST paths; safe fallback metadata for unknown IDs; `sendOTGWvalue()` validates message ID range before map access.
- **Serial line buffer**: Increased to 512 bytes (PS=1 summary lines can exceed 256 bytes).

### Removed

- **REST API v0 and v1**: Removed (see above). Requests to these paths return 410 Gone.

### Breaking Changes

- REST API v0 and v1 endpoints removed — migrate to `/api/v2/`.
- MQTT topic renames (OpenTherm v4.2 alignment): delete orphaned Home Assistant entities after upgrading. See [docs/fixes/opentherm-v42-mqtt-breaking-changes.md](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).
- `FanSpeed` (rpm) split into `FanSpeed_setpoint_hz` + `FanSpeed_actual_hz` (Hz).
- Source-specific MQTT topics changed from underscore-separated (`TSet_thermostat`) to path-separated (`TSet/thermostat`).

---

## [1.1.0] - 2026-02-22

### Added

- **Dallas sensor custom labels**: Inline non-blocking label editor in Web UI. Labels stored in `/dallas_labels.ini` on LittleFS with zero backend RAM usage (max 16 characters per label). Automatic backup/restore during filesystem flash.
- **Dallas sensor graph visualization**: DS18x20 sensors automatically appear in the real-time temperature graph with a 16-color palette. Sensor labels displayed in graph legend.
- **Dallas sensor REST API**: `GET /api/v2/sensors/labels` and `POST /api/v2/sensors/labels` for bulk label management.
- **RESTful API v2**: 13 new `/api/v2/` endpoints with consistent JSON error responses (`{"error":{"status":N,"message":"..."}}`), proper HTTP status codes (202 Accepted for async operations), CORS support, OPTIONS preflight, and RESTful resource naming. Full OpenAPI 3.0 specification in `docs/api/openapi.yaml`. API compliance score improved from 5.4 to 8.5/10.
- **WebUI data persistence**: Automatic log data persistence to browser `localStorage` with debounced 2-second saves, dynamic buffer sizing, and auto-restore on page load. Log buffer cleared automatically after firmware/filesystem flash.
- **Browser debug console (`otgwDebug`)**: Full diagnostic toolkit in the browser JavaScript console: `status()`, `info()`, `settings()`, `wsStatus()`, `wsReconnect()`, `otmonitor()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, `exportData()`, `persistence()`.
- **Non-blocking modal dialogs**: Custom HTML/CSS modals replace blocking `prompt()`/`alert()` calls; WebSocket data flow continues uninterrupted while a modal is open.
- **PS mode detection**: Automatic detection of `PS=1` from the OTGW PIC. When active, hides the OT log section, disables WebSocket OT streaming, and suppresses time-sync commands (improves Domoticz compatibility). WebSocket events broadcast on PS mode transitions.
- **Gateway mode overhaul**: Complete refactor of detection and display logic. REST API field renamed from `mode` to `otgwmode`. Gateway mode polling limited to once per minute.
- **Heap memory health system**: 4-level monitoring (CRITICAL <3 KB, WARNING 3-5 KB, LOW 5-8 KB, HEALTHY >8 KB) with adaptive throttling and WebSocket backpressure control.
- **ADR-030 through ADR-035**: Six new Architecture Decision Records covering heap memory monitoring, two-microcontroller coordination, no-auth LAN security model, Dallas sensor labels, non-blocking modals, and RESTful API compliance.

### Fixed

- **MQTT auth after upgrade from v0.10.x**: MQTT credentials are now automatically whitespace-trimmed on boot and on change — invisible trailing spaces from text editor copy/paste no longer cause authentication failures.
- **Slow Web UI** (index.html loaded into RAM): Replaced full-file-to-RAM loading (`readString()`) with streaming chunked transfer encoding. 95% reduction in RAM used for file serving.
- **Settings reverting to defaults**: Settings now written to flash synchronously before HTTP confirmation. ArduinoJson replaces fragile string-split parsing.
- **Serial buffer**: `MAX_BUFFER_READ` increased to 512 bytes; overflow now discards the corrupt partial line entirely rather than processing it.
- **Out-of-bounds write on message ID 255**: `msglastupdated` array enlarged from `[255]` to `[256]`.
- **Wrong MQTT hour bitmask**: Mask `0x0F` truncated hours 16-23, corrupting night setpoint schedules. Fixed to `0x1F`.
- **`is_value_valid()` wrong data source**: Function used global `OTdata` instead of the passed parameter.
- **ISR race conditions in S0 pulse counter**: TOCTOU races, missing `volatile`, and `uint8_t` overflow on high pulse rates fixed.
- **Reflected XSS**: Request URI injected into HTML error page without escaping. Added HTML entity escaping.
- **GPIO outputs non-functional**: Feature was gated by a debug flag, making it completely non-functional in production builds.
- **750 ms blocking Dallas sensor read**: Replaced with async non-blocking mode.
- **Disconnected sensor publishing -127°C**: Added `DEVICE_DISCONNECTED_C` guard to suppress publishing.
- **File descriptor leak**: File opened before existence check; reordered to check existence first.
- **Null pointer crash on malformed MQTT topics**: Added `strtok()` null guards throughout MQTT callback.
- **Settings flash wear**: 20 separate flash writes per save operation reduced to 1 via 2-second debounce.
- **Year overflow**: Year stored in `int8_t` (overflows at year 2128 and cannot represent 2026). Changed to `int16_t`.
- **Dark mode PIC firmware icons**: Black icons invisible in dark mode; fixed with `filter: invert(1)` in dark mode CSS.
- **Settings persistence**: Deferred save timer could be lost before firing on reboot; now synchronous write before HTTP 200.

### Changed

- **Frontend fully migrated to v2 API**: Zero legacy API calls remain in `index.js`.
- **OTmonitor refresh interval**: Improved from 5 s to 1 s for more responsive UI.
- **`getOTGWValue()` refactored**: Eliminates `String` allocations; uses C-string buffers.
- **Build system**: `version.hash` always generated; centralized `config.py`; reusable GitHub Actions composite actions; automated release workflow publishing `.elf`, `.bin`, and `.littlefs.bin` artifacts.
- **CI/CD**: New ADR Compliance workflow to detect architectural file changes and validate ADR references.
- **Documentation**: OpenTherm v4.2 specification converted to searchable Markdown.

### Deprecated

- REST API v0 and v1 endpoints deprecated; scheduled for removal in v1.3.0 (removed in v1.2.0). Migrate to `/api/v2/`.

### Breaking Changes

- Gateway mode field in REST API response renamed from `mode` to `otgwmode`. Update any custom integrations that read this field directly.

---

## [1.0.0] - 2026-02-08

### Added

- **Real-time graphing**: ECharts-based interactive graphs for OpenTherm data (temperatures, setpoints, pressure) with adjustable time windows directly in the Web UI.
- **Statistics dashboard**: Dedicated tab with session and long-term heating system statistics.
- **WebSocket architecture**: Moved from HTTP polling to WebSockets for live data (logs, status, updates). Significantly reduces network overhead and latency.
- **Dark mode**: Fully integrated dark theme that respects system preferences or can be toggled manually; state saved across sessions.
- **Live log viewer**: Re-engineered with WebSockets for true real-time streaming. Includes filtering, pausing, and raw message decoding.
- **File System Explorer**: Redesigned file manager with upload/download/delete capabilities.
- **MQTT Auto Discovery**: Enhanced Home Assistant integration with improved stability, reliable reconnections, and cleaner entity naming.
- **Stream logging**: Ability to stream OpenTherm logs to files on the file system for troubleshooting.
- **Interactive flash tool**: Improved `flash_esp.py` script for firmware updates.
- **PIC firmware upgrade**: Safer PIC controller flashing from the Web UI with binary data validation.
- **Live OTA progress**: Real-time progress bars and status messages during firmware updates via WebSocket.
- **Settings preservation**: Smart preservation of configuration during firmware upgrades.
- **Evaluation tool**: `evaluate.py` for code quality checks (PROGMEM usage, unsafe patterns).
- **Architecture Decision Records (ADRs)**: ADR system established in `docs/adr/`.
- **Legacy Dallas sensor format**: Added setting for legacy Dallas sensor ID format.

### Fixed

- **PIC flashing crashes**: Fixed critical crashes during PIC firmware updates due to binary data handling.
- **MQTT buffer fragmentation**: Improved reconnection logic and buffer management.
- **Timezone initialization**: Fixed timezone initialization issues.
- **CSRF protection**: Added CSRF protection and better input sanitization.
- **Multiple Exception (2) and Exception (28) crashes**: Related to memory access patterns.

### Changed

- **Build system**: Removed `make` dependency; fully integrated `arduino-cli`.
- **Log viewer**: Switched from HTTP polling to WebSockets.
- **Memory**: Aggressive optimization of string literals using `F()` and `PSTR()`.
- **Responsive design**: Improved settings pages and controls for mobile/desktop.

### Removed

- **HTTP polling for logs**: Replaced by WebSocket streaming.
- **Legacy commented-out code and unused libraries**.

### Breaking Changes

- Default GPIO for Dallas sensors changed to GPIO 10 to match standard hardware.
- Some settings may need re-verification due to new preservation logic (auto-migration attempted).

---

## [0.10.3] - 2024-04-17

Legacy release. See git history for details.

## [0.10.x] and earlier

Legacy releases. See git history for details.

[Unreleased]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.5...HEAD
[2.0.0-beta]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.5...HEAD
[1.3.5]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.4...v1.3.5
[1.3.4]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.3...v1.3.4
[1.3.3]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.2...v1.3.3
[1.3.2]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.1-fix...v1.3.2
[1.3.1]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.0...v1.3.1-fix
[1.3.0]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.0.0
