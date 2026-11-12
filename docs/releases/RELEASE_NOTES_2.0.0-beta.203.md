# OTGW-firmware v2.0.0-beta.203 Release Notes

**Release date:** 2026-11-12
**Branch:** dev
**Build:** 2.0.0-beta.203 (the githash is in the asset names)
**Compare:** [v2.0.0-beta.174...v2.0.0-beta.203](https://github.com/rvdbreemen/OTGW-firmware/compare/v2.0.0-beta.174...v2.0.0-beta.203). "What changed since beta.174" is new for a beta.174 user; a beta.170 user also reads "What changed from beta.170 to beta.174", and so on down the list. Coming from an older beta: the [beta.174 notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.174.md) hold beta.170 to beta.174 as published then and link the older notes. Coming from 1.x: everything is new to you, read the whole document and the earlier notes.

> **Disclaimer: this is a beta of 2.0.0.** It is not a stable release. It has had alpha and beta testing on a small group of units, but not the long field runs a release needs. What was and was not tested for this build:
>
> - Since beta.174: the 29 builds beta.175 to beta.203. The firmware fixes have host tests on the real code, and the web UI changes tests on the real web UI files; for each fix the task records a run that fails on the old code and passes on the fix. Each batch of builds was installed over WiFi on the OT-Direct bench unit (a board without a PIC and without PSRAM) and checked there, with the settings compared before and after. The release candidate beta.203 then ran for 1 hour on both bench units, the OT-Direct bench unit and the OTGW Classic with an S3 Mini, with MQTT connected: the OTGW Classic with an S3 Mini (PIC firmware 6.8) from 06:09 to 07:11 on 12 November (1 h 02 min) and the OT-Direct bench unit from 06:10 to 07:11 (1 h 01 min), each without a restart (bootcount unchanged), with MQTT connected throughout and no new crash log. The free heap went from 55,016 to 53,884 bytes on the Classic and from 68,832 to 68,668 bytes on the OT-Direct unit, with the largest free block flat at 31,732 bytes on both; one hour is too short to call that a trend either way. **The OTGW Classic with an S3 Mini, the supported public hardware, ran only beta.203, for that 1 hour soak;** the builds beta.175 to beta.202 did not run on it one by one. **Not tested on a bench:** the OT-Direct boot change (TASK-1835) at frame level, because the bench has no boiler and no bus listener; the OT-Direct clock answer to a real thermostat (TASK-1840); the OT-Direct return temperature read in a thermostat slot (TASK-1759) with a real thermostat; SAT's own `PM=28` request for the return temperature on a PIC board (TASK-1759) with a real boiler and thermostat; the restore of the OT-Direct command state after a reboot with a real boiler (TASK-1836); the self-tune changes (TASK-1796, TASK-1815, TASK-1816) over days on a real house.
> - From beta.170 to beta.174: the four builds beta.171 to beta.174 change the web UI translations, the hot water card and a boot memory trace; SAT control did not change. Each web UI change has a test on the real web UI files that fails on the old files and passes on the fix, beta.173 (with beta.172) and beta.174 were each installed over WiFi on the OTGW Classic with an S3 Mini bench unit, with the settings compared before and after (150 settings, 0 differences) and the changed screens checked in a browser in English, Dutch, German and Greek; beta.171 ran on the OT-Direct bench unit. The OTGW Classic with an S3 Mini has run beta.174 since 00:28 on 11 November without a restart; its MQTT broker was down until 03:59, so it ran with MQTT connected from 03:59 on (more than the required hour before this release). **Not tested on a bench:** beta.172 to beta.174 on the OT-Direct bench unit (a board without a PIC and without PSRAM), which still ran beta.171 for this release; the boot memory trace of beta.171 (TASK-1757) is diagnostic output only and was not yet used to change anything.
> - From beta.167 to beta.170: the boot change (TASK-1763) has a host test on the real setup() order and was bench-tested in 24 reboots of the OT-Direct bench unit (12 with the old order, 12 with the new one, 0 panics); the translation fixes of TASK-1775 have a test on the real web UI files that fails on the old files (3 of 3), and the Greek wording of TASK-1777 passes the translation tests. The OTGW Classic with an S3 Mini has run beta.170 since 15:21 on 10 November without a restart, with MQTT connected and the free heap flat (checked again before this release was tagged). The OT-Direct bench unit (a board without a PIC and without PSRAM) ran beta.170 too; it was restarted on purpose many times for another test that day, with no crash. **Not tested on a bench:** the Greek wording changes made after beta.170 (two strings of the hot water slider) are in this release build but were not part of the bench run. The boot change lowers the chance of the rare boot panic; it does not remove its cause (see Known issues).
> - From beta.162 to beta.167: every firmware change has host tests on the real code that fail on the old code and pass on the fix (the OT-Direct override list, TASK-1749; the queued boiler write after a mode switch, TASK-1752; the SAT flame-out protection, TASK-1762). Both bench units ran overnight with MQTT connected, the free heap flat and no restart: the OT-Direct bench unit (a board without a PIC and without PSRAM) on beta.167 for 3 h 35 min, and the OTGW Classic with an S3 Mini on beta.166 for 4 h 53 min (the firmware code of beta.166 and beta.167 is the same; beta.167 only changes the web UI). Neither unit logged a new crash. The translation (TASK-1764) has tests that every one of the about 1,500 texts exists in all four languages and that no untranslated text is left, plus tests of the language menu; Home, Settings and Monitor were checked on screenshots in all four languages. Not checked per language: the SAT and Advanced pages, the dark theme and phone width. **Not tested on a bench:** the SAT flame-out protection switching to pulse mode on a real boiler (TASK-1762: host tests only), the override list with a real thermostat in gateway mode (TASK-1749), and the flush on a real mode switch (TASK-1752).
> - From beta.158 to beta.162: every firmware change has tests on the real code that fail on the old code and pass on the fix (browser tests that run the real web UI, and host tests on the real OT-Direct code). The release candidate beta.162 ran on the OT-Direct bench unit (a board without a PIC and without PSRAM) for more than 7 h without a restart, with MQTT connected and the free heap flat; that release build had the same source. The one crash log entry on that unit is from the reboot test before that run (see Known issues). Bench-tested on that unit: the SAT settings after a busy answer at page load (TASK-1743: the SAT page opened directly showed all 41 rows, 3 of 3 times), and the busy answers with two browser tabs open on Monitor > Values (TASK-1741 and TASK-1747: from 15.6 per minute on the old code to 1.9 and then 0.5 per minute, 0.4 % of requests). **Not tested on a bench:** the OT-Direct `TT=` / `TC=` change (TASK-1733) with a thermostat attached; the bench unit has no thermostat, so it rests on host tests on the real gateway code (40 of 40 pass, 16 of 27 fail on the old code). The poll changes were not tested in Safari or Firefox, nor on an OTGW Classic with an S3 Mini.
> - From beta.144 to beta.158: every change has host tests on the real code that fail on the old code and pass on the fix. The release candidate beta.158 ran 6 h 21 min on the OT-Direct bench unit (a board without a PIC and without PSRAM) without a restart, with MQTT connected and no crash log; that release build had the same source. Bench-tested on that unit or on an OTGW Classic with an S3 Mini: the hidden OT-Direct settings in PIC mode (TASK-1719: 164 settings before and 150 after, only the 14 OT-Direct keys gone), the OT-Direct mode selector (TASK-1716), BLE off by default and the warning when you switch it on (TASK-1693, on the board without PSRAM), the locked SAT settings with SAT off (TASK-1737: 41 of 41 locked, 41 of 41 editable with SAT on) and the decoded water pressure (TASK-1734). **Not tested on a bench:** the restart loop fix of TASK-1730 on the unit that had it (host tests on the real detection code; crashevans's confirmation is pending), the home dial with SAT off with a real thermostat (TASK-1735), the 0.10 bar display with a real boiler (TASK-1734; the bench has no boiler), the 0.1 °C setpoint rounding on a real boiler (TASK-1738), and the Dutch and German tab names in a browser set to those languages (TASK-1689).
> - From beta.130 to beta.144: each change has host tests on the real code; for TASK-1698, TASK-1699, TASK-1681, TASK-1703, TASK-1707, TASK-1718, TASK-1696 and TASK-1682 those tests fail on the old code and pass on the fix. Bench-tested on the OT-Direct bench unit (a board without a PIC) and an OTGW Classic with an S3 Mini: the S0 and output pin move (TASK-1698; the settings diff after the update was exactly `s0counterpin` 12 to 13 and `gpiooutputspin` 16 to 11), the Home Assistant discovery after a reboot (TASK-1699; the same 193 configs, at most 1 per second, in 206 s), the start, stop and download of a recording from the web UI (TASK-1700; the file decoded with `scripts/diag_decode.py`), the favicon served from flash (TASK-1696; the bytes equal the file, a second request answers `304`), the override list with its Clear button (TASK-1681) and `TT=20` then `TT=0` being accepted on the OT-Direct bench unit (TASK-1718). The OT-Direct bench unit has no thermostat, so **the gateway-mode changes were not tested with a thermostat attached**: the alternative slots (TASK-1707) and the answer to the thermostat's read of the room setpoint override (TASK-1718) are covered by host tests and wait for a field check by tjfs. **Also not tested:** a real browser doing a full update with the automatic return to the web UI (TASK-1703), the pressure switch flag on a real boiler (TASK-1682; the bench has no boiler), the S0 pulse count on the moved pin with a meter (TASK-1698), the pin move on an OTGW Classic with an S3 Mini Pro (host tests only), and the recording controls on a board with PSRAM (TASK-1700). No separate soak of the beta.144 release build is recorded (see Known issues).
> - From beta.118 to beta.130: each change has host tests on the real code that fail on the old code and pass on the fix. Bench-tested on the OT-Direct bench unit: the mode that takes effect at once (TASK-1702), the master mode that boot auto-detect no longer saves (TASK-1694, two reboots), master mode without the OT-Thing controller (TASK-1684), the refused central heating commands with SAT on and accepted with SAT off (TASK-1695: `CH=0`, `MM=50`, `TT=19.5`, `CS=55` refused, `HW=1` accepted), `C2=` and `H2=` over REST (TASK-1709), the wider pin range (TASK-1686) and the theme toggle (TASK-1692). **Not tested on a bench:** the port 25238 line filter of TASK-1695 on an OTGW Classic (host tests only; the bench unit has no PIC); the gateway-mode fix (TASK-1701) during a heat demand; the new Windows capture launcher on a PC whose Defender flagged the old one; the Safari capture on any Mac but the contributor's.
> - From beta.105 to beta.118: the fixes have host tests on the real code that fail on the old code and pass on the fix (the weather fetch, TASK-1669; the file buffer and the free-heap file gate, TASK-1673; the recorder's secret redaction, TASK-1665; the capture tool's broker, TASK-1677; the OLED, TASK-1678). The host test of the web send cap (TASK-1673, ADR-216) only shows that the call compiles and runs; what it does to memory was measured on the bench. Bench-tested on the OT-Direct bench unit (a board without a PIC): the single gzipped page (it renders with its styles and fonts, two tabs 4 x 25 rounds without a 503), the file queue (a burst of 8 requests: 6 served, 2 answered 503 with Retry-After; a disconnect while queued freed the queue), the send cap (two-tab load, 6 runs with a reboot before each), the recorder start, download and stop on a board without PSRAM, and the weather fetch on its new task. On the OTGW Classic with an S3 Mini: the weather fix over 14 hours on beta.106, and the capture tool reading the broker from the gateway.
> - From beta.77 to beta.105: each change has host tests on the real code that fail on the old code and pass on the fix, except the Home Assistant names and the flow dots (checked on the bench). Bench-tested on a beta.79, beta.87 or beta.94 build: the new Home Assistant names on the broker, the stylesheet served by the device, the telnet switch (off within 4 s and on again without a reboot), the page load with 3 injected `503` answers, the PIC frame decode and PIC version after the size reduction, the "No setup portal" setting with the saved WiFi away for 5 minutes (TASK-1645), the support map files rewritten in their new format, and the WiFi dead-link recovery (a forced reconnect followed by the `WiFi: connected but no traffic` restart).
> - **Not tested on a bench, or only partly:** the OLED fix on a board with a display (TASK-1678: our bench unit has no display; tjfs's field confirmation is pending); the diagnostic recorder on a board with PSRAM, which is the 1.5 MB ring of the OTGW Classic with an S3 Mini, and the download time of a full 1.5 MB recording (TASK-1665, TASK-1666); whether the web server adds a second `Content-Encoding` header to a gzipped page (not inspected header by header; the bench download unpacked correctly); the single gzipped page in Safari and Firefox; the WiFi outage record (TASK-1446, TASK-1444) under a real WiFi drop; the skip-portal boot after a WiFi restart (TASK-1455); the OLED text of the "No setup portal" setting (TASK-1645); the dead-link recovery's guards, its restart cap and the Ethernet case (TASK-1450); keeping graph history across a device reboot in a browser (TASK-1306); a boot with the telnet console switched off (TASK-1644); the replay fixes on a device (TASK-1649, TASK-1650); the integer parser (TASK-1639) on OT-Direct command parsing and on every message of a real boiler; the support maps relearning from a real boiler (TASK-1391); the SAT request for the boiler flow temperature on a PIC board (TASK-1653), which needs a thermostat that never reads MsgID 25.
> - **No real Immergas boiler was tested**, and the OT-Direct change for a thermostat in gateway mode (TASK-1600, beta.68) has not been seen on a bench with a thermostat attached.
>
> Expect bugs, and keep a way back: save your settings, and keep the flash zip of the 2.0.0 build you run now, so you can flash it back over USB. A 1.x zip is ESP8266 firmware and does not run on an S3 Mini; going back to 1.x means putting the Wemos D1 mini back in. If you need a gateway that just works, stay on the [1.x releases](https://github.com/rvdbreemen/OTGW-firmware/releases) for now.

## What this beta is

A public beta of the 2.0.0 line. 2.0.0 runs on the ESP32-S3 only and uses an asynchronous web server and FreeRTOS. The ESP8266 is no longer supported.

The purpose of this beta is field data. We want more people to run it on real heating systems and tell us what happens.

**Supported hardware for this beta:** the OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro. Use the combo image (`esp32-combo`). The PIC stays in place.

Read "Before you flash", "BREAKING changes" and "Known issues" first.

---

## What changed since beta.174

The builds beta.175 to beta.203. Most firmware changes are for OT-Direct, the mode of a board without a PIC; they do nothing on an OTGW Classic, where the PIC does this work. On the Classic the changes are SAT, the update path and the web UI.

### SAT (experimental, off by default)

- **Change: the heating curve self-tune looks at two phases** (TASK-1796, beta.176, ADR-228). Each signal tick of a clean hour goes to the warm-up phase (the room more than 0.1 °C from its target) or the floating phase (within 0.1 °C). Each phase keeps its own 3-day mean, and the coefficient moves by 0.1 only when both phases point the same way. A phase without data does not veto. Decided by Sergeant D (entries 302 to 307).
- **Fix: the self-tune signal stops at the boiler's own maximum** (TASK-1815, beta.189). The signal now uses the lowest of the user maximum, the boiler's maximum (MsgID 57) and the `SH=` limit, so the tune no longer learns from setpoints the boiler cannot reach (Sergeant D, entry 317).
- **Fix: the floating phase votes only with at least 1 hour of data** (TASK-1816, beta.190, ADR-230). Its ticks are counted over the counted days; with less than 1 hour in total the warm-up phase decides alone, so a few floating minutes no longer block a change (Sergeant D, entries 313 and 315). The daily `SATTUNED` log line has a new `fvote=` field.
- **Fix: an early filesystem update no longer wipes the SAT state** (TASK-1771, beta.183). A filesystem upload right after a firmware update could reset the active preset, the energy total and the self-tune window. They now survive.
- **New: on a PIC board SAT asks the boiler for the return temperature itself** (TASK-1759, beta.200). When no valid return temperature (MsgID 28) came for 30 s, SAT sends `PM=28`, the PIC's priority read, at most once per 30 s. It shares the PIC's one priority slot with SAT's `PM=25`, so they never overlap. The PIC sends it only in place of a thermostat request the boiler does not support; without such a request, nothing is sent (decided by Sergeant D, entry 296; the slot rule by Robert).
- **Web UI: the SAT Settings card no longer shows the target temperature** (TASK-1814, beta.182). Set the target on the Home dial, over MQTT or the REST API, as before.

### Web UI

- **New: Monitor > Graph in stacked panels like the 1.x graph** (TASK-1821, beta.187, and TASK-1828, beta.192). Flame, hot water and central heating as on/off lanes, modulation and the temperatures each on their own axis. Dashed lines mark where the live connection dropped and came back; the "Disconnects" chip hides them. The temperature axis shows °C and the window chips are translated.
- **New: Monitor > Values has a compact grid view** (TASK-1823, beta.191) next to the table, like the 1.x home screen. The choice is remembered per browser.
- **New: a Light/Dark switch in the header** (TASK-1830, beta.193), as on the OTGW website.
- **Fix: Monitor > Values names follow a language switch** (TASK-1820, beta.186); before, a row kept the language it was first drawn in.

### Firmware update path

- **Change: `device/info` names how this unit takes a firmware update** (TASK-1819, beta.185): the new field `firmware_ota` is `direct`, `safeboot` or `usb`. The texts on the update guard now match the safeboot layout. How the update works did not change.

### OT-Direct (boards without a PIC only)

These bring OT-Direct closer to what the PIC does. They do not change an OTGW Classic.

- **Fix: answers to Home Assistant and OTmonitor in the PIC format** (TASK-1797, beta.177; TASK-1800, beta.178). `PR=A` answers `OpenTherm Gateway 6.6`, which the Home Assistant `opentherm_gw` integration can parse (it failed on the old text); `PR=T` and the `PS=1` summary line are byte for byte the PIC's.
- **Fix: answers the gateway makes itself reach the thermostat** (TASK-1798, beta.181). In master mode, and for `UI=` and `SR=` answers, the reply was refused by the OpenTherm library's 50 ms gate and never left; it now waits for the gate.
- **Fix: commands that change the thermostat's status flags work in gateway mode** (TASK-1805, beta.184, ADR-229). `HW=`, `CE=`, `H2=`, `SM=` and `BW=` now set their bits in the thermostat's relayed MsgID 0, as the PIC does; before they were acknowledged and did nothing.
- **Fix: `MW=`, `MH=`, `M2=` and `HW=P` write MsgID 99 with the bytes in the right place** (TASK-1832, beta.194). Hot water mode and push sit in the high byte, the heating circuits in the low byte, as the spec and the PIC.
- **Fix: `VS=` writes the ventilation setpoint in the low byte and keeps overriding the thermostat** (TASK-1831, beta.195).
- **Fix: `CE=` and `H2=` default to on, and `CL=0` holds 0 %** (TASK-1833, beta.196), as on the PIC.
- **Fix: the manual hot water push (`HW=P`) works like the PIC** (TASK-1845, beta.197). It keeps your `HW=` setting, forces hot water only when the boiler refuses the push, and has no 30 s cut-off.
- **Fix: no messages of its own at boot unless it is the master** (TASK-1835, beta.198). In gateway, monitor and bypass mode the gateway no longer sends its own status request and handshake to the boiler before the thermostat speaks.
- **Fix: `GW=1` in gateway mode no longer clears your overrides** (TASK-1839, beta.199). A `GW=` for the mode that already runs changes nothing, as on the PIC; a real mode change still starts clean.
- **New: the return temperature is read in a thermostat slot in gateway mode** (TASK-1759, beta.175, ADR-224), while SAT is on and the boiler supports MsgID 28.
- **Fix: `SC=` sets the time for the thermostat and never writes MsgID 20 to the boiler** (TASK-1840, beta.201).
- **Fix: the command state the PIC keeps in its EEPROM survives a reboot** (TASK-1836, beta.202): `SM=`, `SB=`, `MI=`, `FS=`, `HW=`, `SW=`, `SH=`, `IT=`, `OH=` and the `UI=` list.
- **Fix: the loopback test mode no longer counts as a real boiler for SAT** (TASK-1856, beta.203). After loopback, SAT simulation stayed off until a reboot.

---

## What changed from beta.170 to beta.174

The builds beta.171 to beta.174. Web UI fixes and one diagnostic aid. SAT control did not change.

- **Fix: one Hot water card** (TASK-1779, beta.173). The SAT settings no longer repeat the hot water controls; the Hot water card is the one place to set them.
- **Fix: more of the web UI is translated** (TASK-1777, beta.172, and TASK-1791, beta.174). The hot water abbreviation on the Home chips and in the cell grid, and the two short axis labels of the Monitor graph (hot water and central heating), now follow the chosen language.
- **Diagnostics: a memory trace during the MQTT connect at boot** (TASK-1757, beta.171). While the gateway connects to the broker after a boot, it now records which parts were active at each new low of the free memory, and prints the last 16 lows to telnet as `[HEAP] connect-trace` lines when that phase ends. It changes no behaviour; it is there to find the cause of the low free memory right after a boot.
- **Docs: what to do when the PIC is not found** on an OTGW Classic with an S3 Mini (TASK-1788): a new section 7.1a in the troubleshooting chapter of the manual, in English and Dutch.

---

## What changed from beta.167 to beta.170

The builds beta.168 to beta.170, plus two Greek wording fixes made after beta.170. One change at boot and translation fixes in the web UI. SAT control did not change.

Sergeant D reviewed the Greek translation. Thank you.

- **Change: the rare boot panic should happen less often** (TASK-1763, beta.168). We found the cause of the panic listed under Known issues in beta.167: a small internal task (the inter-processor call task) ran out of stack while the interrupt service for the OpenTherm pins was being set up at boot. The firmware now sets up that interrupt service earlier in the boot, before WiFi starts, when much less is going on. This lowers the chance; it does not remove the cause. The real fix (a larger stack for that task) needs a change in how the firmware is built and comes later.
- **Fix: more of the web UI is translated** (TASK-1775, beta.169). The on/off word of the webhook test, the abbreviations on the Home screen display and the counts of unsaved changes and active overrides now follow the chosen language, including the plural forms.
- **Fix: Greek wording reviewed by Sergeant D** (TASK-1777, beta.170 and after). The hot water title, the hot water switch, the room cell on the Home screen and the label and hint of the hot water slider now use the terms Sergeant D chose.

---

## What changed from beta.162 to beta.167

The builds beta.163 to beta.167. One change in how SAT controls the boiler, two OT-Direct fixes that were known issues in beta.162, and the web UI translation.

Sergeant D decided the SAT change. Thank you.

- **SAT: a protection against flame-outs** (TASK-1762, beta.165, Sergeant D's decisions 288 and 289). SAT now switches from continuous to pulse mode (PWM) when the boiler water stays 3.5 °C above the requested temperature for 30 s. The existing rule (60 s above the requested temperature plus the margin setting) stays as it was. This is an addition of our own; thermo-nova, the SAT original, has only the 60 s rule. The switch was seen in host tests only, not on a real boiler.
- **Fix: OT-Direct gateway mode shows an active `TT=` / `TC=` in the override list again** (TASK-1749, beta.163). Monitor > Connection and `GET /api/v2/otdirect/overrides` list it as a remote override, with whether the thermostat has read it, and a Clear button that sends `TT=0`. `overrides_active` counts it. The API gained the `overrides.remote` field; nothing was renamed or removed.
- **Fix: OT-Direct no longer sends one queued boiler write after a mode switch** (TASK-1752, beta.164). A `TT=` queued in master mode could still reach the boiler once after switching to gateway mode. The queue is now emptied on a mode switch and on `GW=R`.
- **New: the web UI in English, Dutch, German and Greek** (TASK-1764, beta.166; fixes in beta.167). A flag menu at the top right switches the language at once, without a reload, and remembers your choice. English stays the default; on your first visit the web UI follows your browser language if it is one of the four. Only the web UI is translated: firmware texts, MQTT, Home Assistant names, the API and telnet stay English. The translations had two reviews per language; if a text reads oddly in your language, please tell us. beta.167 fixes three errors that the translation brought into beta.166: the SAT page did not update its values or lock the SAT settings while SAT is off, the Monitor > Graph readout did not work, and the Monitor > OT support table did not show. It also gives the "Board mode" choice room for its longer translated texts (TASK-1768).

---

## What changed from beta.158 to beta.162

The builds beta.159 to beta.162. Three changes make the web UI cope better with a busy gateway, and one changes how `TT=` and `TC=` work in OT-Direct gateway mode; that one is listed under BREAKING changes. SAT control did not change.

tjfs sent the report behind the OT-Direct change, and Sergeant D decided how `TC=` behaves. Thank you.

- **Changed: on OT-Direct in gateway mode, `TT=` and `TC=` work like on the PIC** (TASK-1733, beta.161, ADR-223, reported by tjfs). The override is now only an answer to the thermostat: the thermostat sees it when it reads MsgID 9 (remote override room setpoint) and MsgID 100 (flags). The gateway no longer writes MsgID 16 and MsgID 100 to the boiler for it, and passes the thermostat's own MsgID 16 unchanged. The override ends at `TT=0` / `TC=0`, at a new value, or by the PIC rule: once the thermostat has read MsgID 9, the 4th MsgID 16 that differs from the override releases it. To write a room setpoint to the boiler, use `BS=` or the MQTT `room_setpoint` topic. Master mode (no thermostat) still writes MsgID 16. PIC boards are not affected. See BREAKING changes.
- **Fix: two open browser tabs get far fewer "busy" answers** (TASK-1741, beta.160, and TASK-1747, beta.162). Every tab started its periodic requests at the same moment, and after each health check it sent two follow-up requests at once, while the gateway accepts only 2 web requests at a time from all tabs together. Each tab now spreads its requests over the cycle with its own random offset, sends the follow-ups one after the other, and stops polling while it is hidden. A tab you show again after at least 15 s refreshes at once. On the bench, two tabs on Monitor > Values went from 15.6 busy answers per minute to 0.5 (0.4 % of requests). A busy answer no longer clears the override list either (the "injected" badge stays).
- **Fix: the SAT settings no longer stay empty after a busy answer at page load** (TASK-1743, beta.159). The web UI loaded the settings once at start; one busy answer left the SAT page without its settings until you opened the Settings page. It now tries again up to 5 times, waiting longer each time, one request at a time.
- **Docs: SAT and the thermostat's heat demand** (TASK-1755). The SAT chapter said the wall thermostat still decides whether heating is on. That was wrong: while SAT is enabled, SAT's `CH=` decides the central heating enable bit the boiler sees, on PIC boards and in OT-Direct gateway mode (ADR-208). When SAT is off, the thermostat decides again.
- Tests only, no firmware change: steadier web UI tests (TASK-1742, TASK-1739), a host test of the beta.155 reply buffer change against the real buffer code (TASK-1736), and a replay of the field capture behind the beta.156 setpoint fix through the real senders (TASK-1738).

---

## What changed from beta.144 to beta.158

The builds beta.145 to beta.158. SAT control changed in one place: a rounding fix in the setpoint it sends (TASK-1738). Three changes alter what you may see after the update and are listed under BREAKING changes: the home dial with SAT off (TASK-1735), the locked SAT settings with SAT off (TASK-1737) and the OT-Direct settings that are hidden in PIC mode (TASK-1719).

tjfs, crashevans and Sergeant D sent the reports and questions behind most of this build. Thank you.

- **Fix: an OTGW Classic whose PIC is not detected no longer ends up in a restart loop** (TASK-1730, beta.147, reported by crashevans). When the PIC did not answer at boot, the firmware treated the board as PIC-less hardware: it probed the Ethernet chip, whose pins on a Classic drive the PIC reset line, and its Ethernet task hung until the watchdog restarted the unit. All 6 crash dumps from crashevans show that watchdog. The Ethernet probe and OT-Direct now run only on hardware that can be PIC-less: not in board mode 1 or 3, not on an S3 Mini Pro, and not on a board with PSRAM. Host test on the real detection code: old code 4 of 6 cases wrong, fix 6 of 6.
- **Changed: the home dial controls the temperature only while SAT does** (TASK-1735, beta.153, Robert's decision). With SAT on, or while SAT has taken over after a lost connection, the +/- buttons set the SAT target as before. With SAT off the buttons are hidden, a press sends nothing, and the dial shows the thermostat's room setpoint (MsgID 16) and room temperature (MsgID 24) as they are, with the text "SAT is off: the thermostat controls the temperature". This replaces an alpha-only change (TASK-1717, beta.146) that sent `TT=` from the dial.
- **Changed: with SAT off every SAT setting is greyed out and locked** (TASK-1737, beta.154, Robert's decision). Switch SAT on to edit them. The values stay stored. On the bench all 41 SAT settings were locked with SAT off and editable with SAT on.
- **Changed: no OT-Direct settings or elements in PIC mode** (TASK-1719, beta.145). On an OTGW Classic with an S3 module the Settings page, Device info and the Monitor no longer show the 14 OT-Direct (`otd*`) settings and panels, which do nothing there. See BREAKING changes for the REST change.
- **Changed: the web UI shows the central heating water pressure as the boiler reports it** (TASK-1734, beta.158, Robert's decision). The 0.1 bar "switch?" mark of beta.144 (TASK-1682) is removed. A boiler that only has a pressure switch and reports 0.10 bar now shows "0.10 bar" in the normal low-pressure colour, the same value MQTT, REST and Home Assistant show.
- **Fix: SAT no longer sends a control setpoint 0.1 °C above the one just sent** (TASK-1738, beta.156, found in a field capture). The `CS=` text and the value SAT remembered as sent were rounded in two different ways. For some values (for example 45.55 °C) the 5 s repeat sent `CS=45.6` after `CS=45.5`, and the next refresh sent 45.5 again. Both now use one rounding, on PIC and OT-Direct. The same applied to `TC=`.
- **Fix: a busy answer from the gateway no longer blanks the SAT page or raises a false alarm** (TASK-1741, beta.157). With two browser tabs open the gateway answers some requests with "busy" (HTTP 503). Three places showed that to you: the SAT page hid all its data behind "Couldn't load SAT status", the SAT setup wizard showed the OpenTherm bus "Offline" and MQTT "Not connected" in warning colour, and the SIMULATION badge disappeared. They now keep what they showed, or say "Unknown" in grey. The cause of the busy answers is being worked on (see Known issues).
- **Changed: BLE starts off on a board without PSRAM, and warns when you switch it on** (TASK-1693, beta.148, ADR-222). The OTGW Classic with an S3 Mini or S3 Mini Pro has PSRAM, so nothing changes there. On a board without PSRAM the consent dialog after boot is gone; the stability warning shows once when you enable BLE. Stored values are kept. Switching BLE on no longer shows "Settings saved" before you answered the warning (TASK-1732).
- **Changed: the OT-Direct mode selector offers gateway, monitor and master** (TASK-1716, beta.149, after Sergeant D's question). Bypass and loopback are service modes; a unit stored in one of them still shows it as "(service mode)". PIC-less hardware only.
- **Web: the busiest polled responses allocate one buffer** (TASK-1736, beta.155). The live value table (`otmonitor`), the BLE sensor list and the weather answer now reserve their reply buffer once, instead of growing it in hundreds of small steps, which split up free memory on a board without PSRAM. The effect on an OTGW Classic with an S3 Mini was not measured.
- **New: the web UI follows your browser language, groundwork** (TASK-1689, beta.150). English stays the default. In a browser set to Dutch or German only the five main tabs are translated in this build; more text follows in later builds.
- **Fix: the OT-Direct "Switch to" button keeps its mark after the settings reload** (TASK-1731, beta.151). PIC-less hardware only.

---

## What changed from beta.130 to beta.144

The builds beta.131 to beta.143, plus the release build beta.144, as published then. SAT control itself did not change. Four changes alter what you may see after the update and are listed under BREAKING changes: the S0 and output pin defaults on an OTGW Classic with an S3 module (TASK-1698), the OT-Direct gateway-mode traffic with a thermostat (TASK-1707), the new answers to the thermostat in gateway mode (TASK-1718) and the slower Home Assistant discovery after boot (TASK-1699).

tjfs sent the logs and reports behind most of this build: the gateway-mode traffic, `TT=` that did nothing, the stray override on the Connection page, the pump symbol, the 0.1 bar pressure and the update page. Thank you.

- **Fix: in OT-Direct gateway mode `TT=` and `TC=` now reach the thermostat** (TASK-1718, beta.142, reported by tjfs). `TT=20` sent the new room setpoint to the boiler, but the thermostat never learned about it: nothing answered the thermostat's read of the remote room setpoint override (MsgID 9), so it never raised its heat demand and the boiler did not start. As on the PIC, the gateway now answers that read with the override value, and the read of the override flags (MsgID 100) with the `TT`/`TC` flags, for as long as the override is active. After `TT=0` the next read is answered once with 0 and then goes to the boiler again. With no override active, a boiler that answers MsgID 9 or 100 with Data-Invalid or Unknown-DataID now gets a read-ack of 0 passed to the thermostat. While an override is active (or its clear is pending), MsgID 9 and 100 are not used as alternative slots (TASK-1707). Host replay through the real gateway path: on the old code 0 of 18 reads of MsgID 9 during `TT=20` were answered with 20.0, on the fix 18 of 18. On the bench unit `TT=20` and `TT=0` were accepted and cleared. **Not yet seen with a thermostat:** whether the thermostat shows the override and raises its demand is waiting for tjfs.
- **Changed: OT-Direct gateway mode with a thermostat sends one boiler frame per thermostat frame, as the PIC does** (TASK-1707, beta.132). Before, the gateway also sent frames of its own between the thermostat's (in tjfs's log 634 own frames against 369 thermostat frames in 6 minutes). Now it sends nothing of its own between them. Its own reads and queued commands go out only in the slot of a thermostat request for a Data-ID the boiler does not support, and the thermostat gets Unknown-DataID for that request, as on the PIC. The gateway never uses a slot for a write of a Data-ID the thermostat writes itself, and releasing the fail-safe setback now also drops its cached TSet write. Without a thermostat, and in master mode, the gateway runs its own schedule as before. See BREAKING changes for what this means for values and commands. Host test on the real code: old code 1109 boiler frames for 360 thermostat frames and a stale TSet written 23 times; fix 360 for 360 and no stale write.
- **Changed: Home Assistant discovery after boot goes out one config per second** (TASK-1699, beta.131). The drip sent a whole OpenTherm ID at a time; ID 0 alone put about 40 configs on the broker at once. It now sends one config per second (10 seconds while memory is low). A config that cannot be sent because the broker, the publish gate or the memory is not ready is retried, not skipped. Bench after a reboot: beta.128 sent 193 configs in 32 s with a peak of 39 per second and an MQTT queue high-water mark of 49; this build sends the same 193 configs in 206 s, at most 1 per second, with a high-water mark of 17. The 1.x line was not changed.
- **Changed: the S0 counter and GPIO output pins move off board pins on an OTGW Classic with an S3 module** (TASK-1698, beta.135). The 1.x defaults, GPIO 12 for S0 and GPIO 16 for the output, are pins the S3 boards use themselves, so the firmware refused them and S0 or the output did nothing until you changed the pin. On the first boot after the update, a stored default now moves to a free hole: S0 to GPIO 13 (D6) and output to GPIO 11 (D7) on the S3 Mini; S0 to GPIO 39 (D6) and output to GPIO 37 (D8) on the S3 Mini Pro. See BREAKING changes. The output pin check also accepted only pins up to 16, so the Pro holes 37 to 39 were refused as invalid; it now uses the same range as the sensor and S0 pins (0 to 21 and 26 to 48).
- **Fix: the override list on Monitor > Connection names each write override and can clear it** (TASK-1681, beta.143, reported by tjfs). A value such as MsgID 1 = 45.0 C on a fresh unit looked like a stray injection. Each row now names its command (`CS`, `CL`/`CC`, `C2`, `MM`, `TT`/`TC`, `SW`, `SH`) and has a Clear button that sends the matching clear command. While SAT owns central heating control, the rows for `CS`, `MM`, `C2` and `TT`/`TC` say that SAT controls them instead of offering a Clear that would be refused. `GET /api/v2/otdirect/overrides` has a new field `overrides.sat_owns`. On the bench, with SAT on, `SW=50` appeared as a row and Clear removed it. Clearing `CS`, `MM`, `C2` and `TT` with SAT off, and `CL=0`, were tested in the web UI harness only.
- **New: the update page opens the web UI by itself after a successful update** (TASK-1703, beta.133, asked by tjfs). The one-action update on `/update` used to stop at "Done" with a link. It now opens the web UI 3 seconds after the update engine has seen the unit back with the expected version and filesystem; the link stays for anyone who wants to go sooner. A failed update stays on the page with its error.
- **New: start, stop and download the diagnostic recording from the web UI** (TASK-1700, beta.134). Advanced > Debug Information has a "Diagnostic recording" card with a status line, Start, Stop and Download (`otgw-recording.bin`). A refused start shows the reason from the firmware. On a board without PSRAM, Stop clears the 4 KB ring, so the card warns you to download first. Decode the file with `scripts/diag_decode.py`.
- **Web: `/favicon.ico` is served from the firmware, not from the filesystem** (TASK-1696, beta.136). The request no longer opens a file or takes a web file slot, and it answers `304` to a repeated request. The icon is the same file. In a two-tab load test (5 runs, a reboot before each) the lowest free heap was 9.6 KB at the deepest and about 26 KB at the median, with 0 timeouts in 500 requests.
- **Web UI: a boiler that reports a fixed 0.1 bar is flagged as a probable pressure switch** (TASK-1682, beta.138, reported by tjfs). Some boilers (Ideal Logic) only have a low-pressure switch and report 0.10 bar. After 10 minutes at 0.10 bar (within 0.005) the pressure on the home page, the boiler chip and the grid reads `0.10 bar - switch?` with a tooltip and loses the red low-pressure colour. Any other value clears the flag at once. MQTT, REST and Home Assistant keep the raw value. The flag has not been seen on a real boiler.
- **Web UI: the pump in the home schematic is a pump symbol** (TASK-1705, beta.139, reported by tjfs). The spinning pie chart became the usual circulation-pump symbol, a triangle in a circle along the return flow. It is grey when idle and blue with a gentle pulse while central heating runs.
- **Help and manual: pin settings say GPIO number, not the hole** (TASK-1688, TASK-1662). The help for the sensor, S0 and output pins names the GPIO number and the per-board defaults, and the manual has a table of the socket holes with their GPIO numbers on the Wemos D1 mini, the S3 Mini and the S3 Mini Pro, for anyone whose Dallas sensor stopped working after the Wemos to S3 swap.

---

## What changed from beta.118 to beta.130

The builds beta.119 to beta.129, plus the release build beta.130. SAT control itself did not change; what other channels may send while SAT is on did (TASK-1695). Two changes are BREAKING (TASK-1684, TASK-1695): read that section.

tjfs found the gateway-mode cycling and the OT-Direct mode that only changed after a reboot, and sent the captures that showed both. Crash-Evans sent pull request #699 (Safari capture on macOS) and ran the capture tool on a Mac. Thank you.

- **Fix: in OT-Direct gateway mode the boiler no longer switches between heat demand and standby** (TASK-1701, beta.126, reported by tjfs). The gateway sent a status message (MsgID 0) of its own every 0.8 s with central heating on, between the thermostat's messages with central heating off. The boiler followed whichever came last. As on the PIC, the boiler now gets only the thermostat's status while a thermostat is connected; without a thermostat, and in master mode, the gateway still sends its own. Host replay of the report: 568 central heating changes in 6 minutes on the old code, 0 on the fix. Field: in tjfs's 20-minute capture on beta.127 the gateway sent none of its own status messages (the old code would have sent about 1,000) and the boiler stayed in standby. A heat demand was not in that capture (see Known issues).
- **Fix: an OT-Direct mode chosen in the settings takes effect at once** (TASK-1702, reported by tjfs). Before, the new mode was only saved, and the bus kept its mode until a reboot. Saving the mode (web UI, REST or MQTT) now switches the bus at once, also when you save the same mode again. When the bus runs another mode than the saved one, the settings page says so (`Running now: Master (no thermostat seen at boot)`) and offers `Switch to gateway`.
- **Fix: OT-Direct boot auto-detect no longer saves the master mode it picks** (TASK-1694). With no thermostat heard in the first 5 seconds the unit runs in master mode for that boot only. Before, the saved master mode skipped auto-detect on every later boot, so a thermostat connected later was never relayed. See BREAKING changes for a unit an older build left in master mode.
- **Changed: while SAT is enabled, other channels can no longer send central heating control commands** (TASK-1695, ADR-217, beta.127). BREAKING, see that section.
- **Removed: the OT-Thing standalone controller of OT-Direct; SAT is the only temperature controller** (TASK-1684, beta.123). BREAKING, see that section.
- **Fix: REST accepts `C2=` and `H2=`** (TASK-1709, beta.128). `POST /api/v2/otgw/command` answered `400 Invalid command format` for the CH2 setpoint and CH2 enable commands.
- **New: sensor, S0 and output pins accept every ESP32-S3 GPIO** (TASK-1686, beta.125). The pin settings kept the ESP8266 range 0 to 16, so the free holes D6 to D8 (GPIO 39, 38, 37) of an S3 Mini Pro could not be used for the 1-Wire bus. They now accept 0 to 21 and 26 to 48. Pins the board itself uses stay refused, and the defaults are unchanged.
- **Web UI: the theme toggle says what it does** (TASK-1692, beta.124). Screen readers no longer hear the opposite of the label. The "Dark theme by default" setting is hidden from the settings page because the web UI never used it; the key stays in REST and `settings.ini`.
- **Diagnostics: new memory keys in `/api/v2/device/info`** (TASK-1679). `hd_dip_mqtt_queue`, `hd_dip_mqtt_tx_bytes` and `hd_mqtt_queue_hwm` show what MQTT held at the lowest free heap; `hd_ack_low_min`, `hd_ack_low_free_before`, `hd_ack_low_hits` and `hd_ack_low_files` show the lowest point inside a web file send. No behaviour change.
- **Capture tool: finds your gateways, reads the MQTT password, captures Safari on a Mac, and the Windows launcher no longer uses PowerShell** (TASK-1690, TASK-1708, TASK-1711). See "Capture tool".

---

## What changed from beta.105 to beta.118

The builds beta.106 to beta.117, plus the release build beta.118. Seven changes. SAT control did not change.

Two items are pull requests by André Brait (andrebrait): #697 and #698. tjfs reported the dark OLED on Discord, and maarten_41762's field capture showed us the broker problem in the capture tool. Thank you.

- **Fix: free memory no longer drops slowly while weather data is on** (TASK-1669, beta.106). This was the known issue of beta.105: about 470 bytes per hour on an OTGW Classic with an S3 Mini. The cause was the weather fetch: each fetch from Open-Meteo started its own task, and each start and end of that task lost about 130 bytes. The fetch now runs on the existing webhook task, so no task is created per fetch. Only units with weather data switched on (off by default) were affected. Bench: on the OTGW Classic with an S3 Mini, beta.106 ran 14 hours overnight without a reboot, with MQTT connected, and the free heap trend was -24 bytes per hour (beta.95: about -470 bytes per hour over 21 hours). That curve is from beta.106, not from this exact build.
- **Web UI: each page is one gzipped file, and the server queues file requests instead of refusing them** (TASK-1659, ADR-212, PR #697; TASK-1640, TASK-1674, ADR-215, PR #698; both by André Brait). The build now puts the stylesheets, the script and the fonts inside the page and gzips it: `v2.html.gz` (about 171 KB) and `FSexplorer.html.gz` (about 6 KB) replace about 475 KB of separate files. Every page is sent with an ETag, so a reload transfers only headers. The firmware update pages are gzipped inside the firmware. When both file streams are busy, up to 4 file requests now wait in arrival order for up to 10 s, instead of getting `503` at once. The separate asset routes are gone (see BREAKING changes), and so are the JavaScript page loader and the stylesheet retry of TASK-1643 (see "What changed since beta.77"). New firmware on a filesystem from before this change serves the plain page from that filesystem, and the file explorer stays reachable, so `flash_otgw --migrate` can still save `settings.ini`. Measured in PR #697 on an ESP32-S3 before the send cap below: first load about 1.1 to 1.4 s instead of 2.9 to 4.2 s. With the send cap the bench two-tab test shows a median tab load of 1.4 to 1.5 s.
- **Less memory pressure when two browser tabs load the web UI at once** (TASK-1673, ADR-216). Two tabs loading at the same moment could drive the free heap of the OT-Direct bench unit down to about 3 KB. Three changes: each open web file uses a 512-byte read buffer instead of 4 KB; the server streams one file at a time instead of two when the free heap is below 24,000 bytes; and each web connection holds at most 2,872 bytes (two TCP segments) in the network stack instead of 5,744. Bench (two-tab load, a reboot before every run): the median lowest free heap rose from about 3.3 KB to about 14.5 KB, and runs below 4 KB went from 2 of 3 to 1 of 6. The cost: page loads are about 0.3 s slower, and the worst case is not bounded yet (see Known issues).
- **New in `/api/v2/device/info`: what held the memory at the lowest point** (TASK-1673). Nine `hd_dip_*` keys record, at the lowest free heap the main loop has seen: the free heap, the largest free block, the bytes and connections in the web send queues, the active and `TIME_WAIT` connections, the open file streams, the REST requests and the WebSocket clients. Seven `hd_webfile_*` keys (PR #697) count queued, admitted, cancelled and timed-out file requests, the longest wait and the queue depth. If your unit reboots or the web UI hangs, these values help us.
- **New: diagnostic recorder, opt-in and off by default** (TASK-1665, TASK-1666, ADR-213, ADR-214). It records OpenTherm frames, PIC and OT-Direct commands, MQTT, REST, weather, BLE, NTP and boot events in a ring in memory: 1.5 MB in PSRAM when the board has it, 4 KB otherwise. It is controlled over REST only (there is no web UI page for it yet): `POST /api/v2/diag/recording/start` and `/stop`, `GET /api/v2/diag/recording/status`, and `GET /api/v2/diag/recording` downloads the recording as a binary file. A recording keeps running after a reboot until you stop it. The start is refused when it would leave less than 5,120 bytes of free heap. Secrets never enter the ring: passwords, keys, tokens, MQTT credentials, webhook URLs and BLE MAC addresses are recorded as "set" or "empty", the location is rounded to 0.1 degree, and secret-looking keys inside free-text values such as a webhook payload are blanked. Bench on a board without PSRAM: a start cost about 5 KB of heap, and a 4,300-byte download took 0.16 s while 5 parallel `device/info` requests were answered in 0.09 to 0.18 s. **Not tested** on a board with PSRAM.
- **Fix: the OLED shows the WiFi setup screen on a board without a PIC** (TASK-1678, reported by tjfs). The combo image looks for the display on the OTGW Classic I2C pins until it has detected the board, and that detection runs after the WiFi setup. On a board without a PIC the display stayed dark while the setup portal was open, and came on only once the gateway had joined WiFi. When no display answers on the Classic pins, the gateway now also looks on the other pin pair before the portal opens. Until the board is detected the OLED button does not change pages there, because on that bus its pin is a data line. Nothing about the detection is saved (ADR-201). Host tests on the real code fail on the old code, pass on the fix and fail on a mutant. **Not seen on a bench with a display**, because our bench unit has none. tjfs's confirmation in the field is pending; if your OLED still stays dark during the setup, please tell us.
- **Capture tool: the MQTT broker comes from the gateway** (TASK-1677). The tool reads the broker address, port and user name from the gateway's settings, so a typo can no longer leave the MQTT part of a capture empty. A `--broker` that differs from the gateway's is used, with a WARNING. With MQTT off on the gateway the MQTT stream is skipped. When the gateway has an admin password, or does not answer, the tool asks for the broker as before. The MQTT password is never read from the gateway: the tool asks for it when the broker needs one. Bench: a 25 s capture on the OTGW Classic with an S3 Mini printed the broker it read and recorded 1,225 MQTT messages.

---

## What changed from beta.99 to beta.105

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
- **After a WiFi restart the gateway no longer waits in the setup portal for an access point that is still away** (TASK-1455, ADR-211). When the WiFi supervisor restarted the gateway and the access point was still away, a release build opened the setup portal for 240 s, and the gateway could not rejoin the returning access point while it ran. On the boot after such a restart the gateway now skips the portal, finishes start-up without WiFi and retries the saved network every 30 s. A power cycle, a first install, the triple reset and every other restart still open the portal as before. **On this beta build** the gain is small: the boot itself skips the beta AP fallback too, but after 2 failed retries (about 1 minute) the gateway still starts that fallback access point, which retries the saved network only every 5 minutes; see Known issues. With the `wifinoportal` setting on, this no longer applies (TASK-1645, see "What changed from beta.95 to beta.99").
- **New setting: no setup portal when the saved WiFi is down at boot** (TASK-1645, GitHub #154, #131). Settings, Network, Wi-Fi: "No setup portal when Wi-Fi is down" (`wifinoportal`, off by default; it applies from the next boot). With it on, a boot that cannot reach the saved network keeps retrying every 30 s instead of opening the setup portal, and the OLED shows "Triple reset to reconfigure WiFi" while offline. Without saved WiFi, or after a triple reset, the portal still opens. Since beta.99 the beta AP fallback stays closed as well with this setting on.

### Telnet

- **New setting: switch the telnet debug console (port 23) off** (TASK-1644, GitHub #154). Settings, System: "Telnet debug console" (`telnetenabled`, on by default). Port 23 has no login and its keys can switch the GPIO output, so switch it off if you do not use it. Off closes the port at once and drops a connected client, without a reboot. When you switch it on again, the gateway keeps trying to open port 23 for up to 3 minutes, because the network stack can hold the old port for about 2 minutes after it closed.
- **Telnet action keys wait until start-up is done** (TASK-1395). A key such as `q` (reload settings) sent while the gateway was still starting ran next to the start-up code, and its output got mixed into the start-up log. Until `Setup finished!`, action keys are now refused with `Key 'q' ignored: setup still running, retry after 'Setup finished!'`. The debug toggles and `h` (help) still work.

### OpenTherm

- **The support maps learn only from frames the boiler and thermostat really sent** (TASK-1391, TASK-1612). The gateway keeps maps of which messages the boiler supports and which the thermostat asks for. Answers and requests the PIC made itself (`A` and `R` frames) were counted as if the boiler or thermostat had sent them, so a message could be marked unsupported by a boiler that never saw it. Now only `B` frames count for the boiler map and only `T` frames for the thermostat map. **Do:** nothing. After the update both maps start empty once and refill from live traffic; the MQTT value topic `otgw-firmware/boiler/unsupported_msgids` is published empty once.
- **About 9 KB more free space in the firmware slot** (TASK-1639, GitHub #694). The firmware now uses an integer-only number parser; the floating point parser it pulled in is gone. On the bench the PIC frames, the PIC version and the boot counter read the same as before.

### Web UI

- **No unstyled first paint** (TASK-1643, reported by André Brait). The stylesheet now loads before the page is drawn, so elements no longer jump while the page loads. If the gateway answers `503` (busy), the page retries the stylesheet up to 8 times, as before. Since beta.111 the stylesheet is inside the single gzipped page, so this retry is gone (see "What changed since beta.105").
- **The graph keeps the history from before a device reboot** (TASK-1306). With the Graph page open, the next 10-minute reload after a device reboot dropped everything from before that reboot, because the gateway no longer holds it. It now stays on the page.
- **The flow dots on the Connection map loop without a jump** (TASK-1609, PR #695 by André Brait).
- **Settings help texts are a little shorter** (TASK-1646), to stay within the flash budget for them. Every setting keeps its help text.

### Home Assistant

- **The SAT zone and PV-boost entities get readable names** (TASK-1608, PR #693 by André Brait). The display name was `<hostname>_SAT_Zone_1_<value>` and `<hostname>_SAT_PV_<value>`; it is now `SAT Zone 1 <value>` and `SAT PV <value>`, spaced and without the hostname, like the other entities. Unique ids and MQTT topics are unchanged, so existing entities keep their entity id and history; only the name shown in HA changes. **Do:** nothing, unless a dashboard or automation matches on the old display name.

### Replay (bench and diagnostics)

- **A replayed outside temperature no longer counts as a fresh boiler reading** (TASK-1649). The value is still shown and published; only its age stamp, which SAT uses to judge whether the boiler's outside temperature is current, ignores replayed frames.
- **On OT-Direct, a replayed PIC start-up line no longer makes SAT resend its commands** (TASK-1650). There is no PIC on OT-Direct, so the line can only come from the replay. On a PIC board it still does.

### SAT

Since beta.77 SAT control changed in beta.99 (derivative gain factor, and the PM=25 request on PIC boards; see "What changed from beta.95 to beta.99") and in beta.104 (the 180 s minimum burn before the cold cutoff; see "What changed from beta.99 to beta.105"). One report was checked and needs no change: hot-water flames count in the SAT 4 h and 24 h cycle windows, as in the SAT reference code and as decided by Sergeant D (TASK-1512).

---

## SAT is experimental and opt-in

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- While SAT is on, it owns central heating control: `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT, REST, the boot commands or the serial bridge are refused (TASK-1695; before beta.127 only `CS=` was). `HW=`, `SW=`, `SH=` and every other command still pass.
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
- **A unit that already runs 2.0.0** (beta.1 or later, or a 2.0.0 alpha with the safeboot layout) **updates over WiFi with the flash zip.** Open `/update`, give it the release zip and press **Update**. The page runs the whole sequence: firmware through the safeboot updater, then the filesystem. Settings are kept. Let it finish: this build expects the new filesystem with the gzipped pages. Between the two steps it serves the plain page of the old filesystem.
- Coming from 1.x: this is a different firmware line. Settings and the web UI differ. Read the breaking changes below.

**3. Interrupted filesystem upload.** If the filesystem upload stops part-way, the settings are no longer lost. The gateway wipes the filesystem, writes your settings back from memory and asks you to upload the filesystem again. Until you do, it serves a fallback page with the file explorer upload (`device/info` reports `fsupdateincomplete`).

**4. Do not flash the PIC over WiFi with OTmonitor.** That has not changed.

**5. OTmonitor or the Home Assistant `opentherm_gw` integration?** They connect to the TCP serial bridge on port 25238, which is **off by default (opt-in)**, as in 1.x. Switch on "Legacy TCP port 25238" in the OTGW settings if you use either of them. While SAT is enabled, the bridge checks every line (TASK-1695, ADR-217): it drops the central heating commands above and any binary data, so a PIC firmware upgrade through OTmonitor needs SAT switched off first. With SAT off the port passes everything unchanged.

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
## BREAKING changes

### New from beta.174 to beta.203

No MQTT topic, setting or API field is renamed or removed. `device/info` has a new field, `firmware_ota` (TASK-1819). On OT-Direct (boards without a PIC) some commands now behave as on the PIC, which is a change if you relied on the old behaviour:

- `GW=` for the mode that already runs keeps the active overrides (TASK-1839).
- `HW=`, `SW=`, `SH=`, `IT=`, `OH=`, the `UI=` list, `SM=`, `SB=`, `MI=` and `FS=` are kept across a reboot (TASK-1836). To clear one, send its clear command.
- `MW=`, `MH=`, `M2=` and `HW=P` now write the bytes of MsgID 99 as named (TASK-1832); before, `MW=` set the heating circuit.
- `VS=` now sets the value you give (TASK-1831); before, `VS=50` reached the boiler as 0 %.
- `CE=` and `H2=` are on until you switch them off (TASK-1833).
- `HW=P` no longer switches `HW=` to 1 (TASK-1845).
- `PR=A` answers `OpenTherm Gateway 6.6` (TASK-1797).

### New from beta.170 to beta.174

None. No MQTT topic, setting or API field is renamed or removed.

### New from beta.167 to beta.170

None. No MQTT topic, setting or API field is renamed or removed.

### New from beta.162 to beta.167

None. No MQTT topic, setting or API field is renamed or removed.

### New from beta.158 to beta.162

No MQTT topic or setting is renamed or removed. One behaviour change:

- **OT-Direct gateway mode: `TT=` and `TC=` no longer write MsgID 16 and MsgID 100 to the boiler** (TASK-1733, ADR-223). The value only reaches a thermostat that reads MsgID 9; a thermostat that never reads it never shows the override. `TT=0` / `TC=0` no longer clear a room setpoint written with `BS=` or the MQTT `room_setpoint` topic. **Do:** to write the room setpoint to the boiler, publish it to the `room_setpoint` topic or send `BS=<value>`, and clear it with `BS=0`. PIC boards and OT-Direct master mode are not affected. Details: [BREAKING_CHANGES.md](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/BREAKING_CHANGES.md).

### New from beta.144 to beta.158

No MQTT topic or setting is renamed or removed. Three changes alter what you may see:

- **`GET /api/v2/settings` lists no `otd*` settings in PIC mode** (TASK-1719). On an OTGW Classic with an S3 module (board mode 1 or 3, or a live PIC) the 14 OT-Direct settings are left out of the list: on the bench 164 settings before, 150 after. `settings.ini` keeps their values and a POST still accepts them. **Do:** a script that reads an `otd*` key on a Classic must not expect it in the list.
- **With SAT off the home dial has no +/- buttons** (TASK-1735). Set the temperature on your thermostat, or switch SAT on. `TT=` over MQTT or REST works as before.
- **With SAT off the SAT settings are locked** (TASK-1737). **Do:** switch SAT on first when you want to prepare its settings.

### New from beta.130 to beta.144

No MQTT topic, REST route or setting is renamed or removed. Additive: the field `overrides.sat_owns` in `GET /api/v2/otdirect/overrides` (TASK-1681). Four changes alter behaviour you may notice:

- **On an OTGW Classic with an S3 module, two settings change on the first boot after the update** (TASK-1698). Only when the stored value is still the 1.x default, and only if the new pin is not held by another feature:
  - OTGW Classic with an S3 Mini: `s0counterpin` 12 to 13 and `gpiooutputspin` 16 to 11.
  - OTGW Classic with an S3 Mini Pro: `s0counterpin` 12 to 39 and `gpiooutputspin` 16 to 37.
  - A pin you chose yourself stays. PIC-less hardware keeps its pins. The change is saved once; the next boot writes nothing. On the bench (Classic with an S3 Mini) the settings diff was exactly those two keys.
  - **Do:** if you wire an S0 meter or an output, use the D6 hole for S0, and the D7 hole (S3 Mini) or the D8 hole (S3 Mini Pro) for the output, or set another free pin in the settings. Check the settings page after the update.
- **OT-Direct gateway mode with a thermostat: values and commands depend on slots, as on the PIC** (TASK-1707). Values the thermostat does not read itself (return temperature, pressure, DHW temperature, counters) update only when a slot comes by. With tjfs's thermostat that is about once per 30 seconds for all of them together. Commands such as `MM=` wait for a slot, and a thermostat that never asks for a Data-ID the boiler does not support gives no slots at all. Differences with the PIC: the thermostat gets the slot answer at once instead of after the boiler's answer, and `SR=` Data-IDs are still answered locally. **Do:** if an automation or dashboard needs those values faster, tell us. This is the PIC model, chosen on purpose, and it has not run with a thermostat on a bench.
- **OT-Direct gateway mode: the gateway answers the thermostat's reads of MsgID 9 and 100** (TASK-1718). While `TT=` or `TC=` is active, the thermostat gets the override value and flags instead of the boiler's answer. **Do:** if you use `TT=`/`TC=` with SAT off on PIC-less hardware, the thermostat should now take over the override setpoint, as with a PIC. This is covered by host tests only; tell us whether your thermostat shows the override.
- **Home Assistant entities appear over about 3.5 minutes after a boot instead of about 30 seconds** (TASK-1699). Measured with 193 configs on the bench unit: 206 s against 32 s. Nothing is lost; the configs arrive one per second. **Do:** do not judge a missing entity in the first minutes after a reboot.

### New from beta.118 to beta.130

Removed: 13 OT-Direct settings, the `ch_suspended` status field and its MQTT topic (TASK-1684). Additive: `ext_cmd_refused`, `ext_cmd_last_refused` and `ext_bridge_dropped` in `GET /api/v2/sat/status` (TASK-1695), and seven `hd_*` keys in `GET /api/v2/device/info` (TASK-1679).

- **While SAT is enabled, other channels can no longer send central heating control commands** (TASK-1695, ADR-217). `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT, REST, the boot commands and the serial bridge (port 25238) are refused; before, only `CS=` was. Each refusal is logged on telnet and counted in `ext_cmd_refused` of `GET /api/v2/sat/status`. On port 25238 the bridge drops such a line, and also binary data and lines longer than 127 characters, so a PIC upgrade through OTmonitor needs SAT off. With SAT off everything is accepted as before. **Do:** if a Home Assistant automation, a Node-RED flow or a script sends one of these commands, stop it while SAT is on, or switch SAT off.
- **The OT-Thing standalone controller of OT-Direct is removed; SAT is the only temperature controller** (TASK-1684). It only ran in OT-Direct master mode with SAT off. Gone: the settings `OTDchmode`, `OTDflowtemp`, `OTDflowmax`, `OTDroomsetpoint`, `OTDgradient`, `OTDexponent`, `OTDoffset`, `OTDroomcomp`, `OTDkp`, `OTDki`, `OTDkboost`, `OTDhysteresisenable` and `OTDhysteresis` (settings page, `/api/v2/settings`, `/api/v2/otdirect/settings`, `/api/v2/debug`); `POST /api/v2/settings` with one of them now answers 400. Also gone: the field `ch_suspended` of `GET /api/v2/otdirect/status` and its retained MQTT topic, which the gateway empties. In master mode central heating now stays off until SAT or a `CS=` command sets a flow setpoint, and an external `CS=` must be repeated within 60 s. An old `settings.ini` with these keys loads without error. **Do:** in OT-Direct master mode without SAT, send `CS=` at least once a minute, or enable SAT. Gateway mode is unchanged.
- **A master mode that boot auto-detect saved on an older build stays until you set the mode once** (TASK-1694). **Do:** if your OT-Direct unit has a thermostat but runs in master mode, choose Gateway in the OT-Direct settings once. It now takes effect at once (TASK-1702).

### New from beta.105 to beta.118

No MQTT topic and no setting is renamed or removed. Additive: the REST routes `GET /api/v2/diag/recording`, `GET /api/v2/diag/recording/status`, `POST /api/v2/diag/recording/start` and `POST /api/v2/diag/recording/stop` (TASK-1666; the download and the status are open, start and stop ask for the admin password when one is set), the `DiagRecordingOn` entry in `settings.ini` (TASK-1665, off unless you start a recording), and 9 `hd_dip_*` and 7 `hd_webfile_*` keys in `GET /api/v2/device/info` (TASK-1673, TASK-1659).

- **The separate web UI asset routes are removed** (TASK-1640, ADR-215): `/v2.js`, `/v2-bundle.css`, `/fsexplorer.css`, `/fonts/*` and `/FSexplorer.png`. The pages now carry these inside. **Do:** check bookmarks, scripts and reverse proxy rules that fetch one of these paths. The page itself (`/`) and `/FSexplorer.html` stay.
- **Web pages are sent gzipped** (`Content-Encoding: gzip`) with an ETag. Browsers handle this. **Do:** a script that downloads a page with an HTTP client that does not unpack gzip gets the packed bytes; let it accept gzip.
- **Web file requests can wait up to 10 s instead of getting `503` at once** (TASK-1659, ADR-212). A `503` with `Retry-After: 1` still comes when 4 requests are already waiting or the 10 s run out.

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
- **Heating curve self-tune is on by default** (TASK-1525). On a unit with SAT enabled, the heating curve coefficient can now change by 0.1 per step, with at least 3 counted days between steps. Do: switch "Heating curve self-tune" off if you want to keep your coefficient fixed.

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

- **SAT's own requests for the flow and return temperature on a PIC board have not run on a real boiler with a thermostat** (TASK-1653 `PM=25`, TASK-1759 `PM=28`). They are covered by host tests on the real queue and frame code. The PIC sends such a request only in place of a thermostat request the boiler does not support; if your thermostat only asks supported values, SAT sends nothing and `sat/pm25_no_slot` says so. The REST SAT status counts `pm28_sent`, `pm28_answered` and `pm28_unanswered`.
- **OT-Direct: the boot change (TASK-1835) is not proven at frame level.** The bench unit has no boiler and no bus listener, and telnet starts after that part of the boot. On the bench, gateway mode came up about 1 s sooner than master mode in the same build, which matches the one boot status request it no longer sends; that is an indication only. A boot capture with a thermostat in gateway mode is welcome.
- **OT-Direct: the clock answer to a real thermostat (TASK-1840) has not been seen.** The bench checked that `SC=` puts nothing on the bus; the answer to the thermostat's MsgID 20 rests on host tests.
- **Used SAT in an alpha before 30 October 2026? Check the heating curve slope** (TASK-1787). The hourly heating curve auto-tune of the early 2.0.0 test builds (the `SAT Auto Tune` switch, removed by TASK-1413 on 30 October 2026) could lower the heating curve coefficient. 2.0.0 keeps a stored coefficient as it is, so a unit that ran that auto-tune can still carry a value far below the default of 2.0; one tester's unit was at 1.12. The daily self-tune moves the coefficient only by 0.1 per step, with at least 3 counted days between steps, so it does not correct such a value quickly. **Do:** after the upgrade, open Settings > SAT and check **Heating curve slope** (default 2.0).
- **Several open browser tabs still get a few "busy" answers** (TASK-1741, TASK-1747). On a board without PSRAM, two visible tabs on Monitor > Values got about 0.5 HTTP 503 answers per minute on the bench (0.4 % of requests), down from 15.6. The pages keep showing their last values, so a busy answer only makes a value one poll older. **Do:** close the gateway tabs you do not use; a hidden tab stops polling by itself.
- **The first periodic refresh of a page can come up to 15 s later than before beta.160** (TASK-1741). This is the price of spreading the requests of each tab.
- **A reboot can occasionally take about 25 s** (TASK-1763). One panic in the interrupt setup at boot was seen in 10 reboots of the bench unit with beta.162; the gateway restarted by itself and then ran normally. The cause is known: a small internal task runs out of stack during that setup. Since beta.168 the setup runs earlier in the boot, which lowers the chance (0 panics in 24 bench reboots), but the stack is still as small. The fix, a larger stack, comes in a later build. On 10 November one more panic was seen once, at boot of beta.167 on the OTGW Classic bench unit, on a second internal task of the same kind; its report was damaged, so the cause is likely the same but not proven.
- **The thermostat in OT-Direct gateway mode has not been tested on a bench** (TASK-1707, TASK-1718, TASK-1733). The bench unit has no thermostat. These changes are covered by host tests on the real gateway path. Whether the thermostat shows a `TT=` override and raises its demand, and how the slots behave with a real thermostat and boiler over time, is waiting for tjfs. If your boiler or thermostat misbehaves in gateway mode, please send a capture.
- **OT-Direct: the override flags (MsgID 100) differ from the PIC** (TASK-1750). The PIC answers with the bits of the active override and applies `OH=`; OT-Direct answers `TT=` with a fixed value and ignores `OH=`.
- **OT-Direct master mode: `TT=0` also clears a room setpoint written over MQTT or with `BS=`** (TASK-1751).
- **The update page's automatic return to the web UI has not been seen in a real browser** (TASK-1703). It is covered by a test that runs the page script with a stub; the safeboot page still reloads after a fixed 20 seconds. If the page stays on "Opening the web UI", open the link.
- **The moved S0 and output pins** (TASK-1698) were bench-tested on an OTGW Classic with an S3 Mini: the settings diff, the output switching without a refusal, and a reboot with S0 enabled. S0 pulse counting with a real meter and the S3 Mini Pro pins were not tested.
- **Page loads are about 0.3 s slower than they would be without the send cap** (TASK-1673, ADR-216). The cap on each web connection's send queue halves the data in flight. The page still loads much faster than in beta.105, because it is now one gzipped file. In the bench two-tab test the median tab load is 1.4 to 1.5 s against 1.1 to 1.3 s without the cap. It is the price for the higher free-heap margin.
- **Two tabs loading at once can still push free memory low, but more rarely** (TASK-1679). With the send cap, 1 of 6 bench runs still reached a lowest free heap of 3,252 bytes. The rest of that dip sits outside the send queues, in the web server's own task, and is being measured. No crash or reboot was seen in these tests. Avoid loading the web UI in several tabs at the same moment.
- **The OT-Direct gateway-mode fix (TASK-1701) has not been seen during a heat demand.** The field capture covered 20 minutes in standby: no own status messages from the gateway and no cycling. Whether the boiler follows the thermostat correctly when it asks for heat, and switches off again, is not proven yet. If your boiler still switches between heating and standby, please send a capture.
- **Free memory went lower during a field capture than on our bench** (TASK-1679). In a 20-minute capture on beta.127 (OT-Direct, web UI open in the capture's browser, telnet, WebSocket and polls at once) the lowest free heap was 16,124 bytes, with one REST and one connection answered `503`. The OT-Direct bench unit on beta.129 reached 18,384 bytes; the OTGW Classic on beta.118 24,132. No crash or restart came with it. The new `hd_ack_low_*` and `hd_dip_mqtt_*` keys are there to find where it goes.
- **One unexplained 3-minute connection drop in a field capture.** On beta.127 the telnet, WebSocket and browser connections of a capture dropped at the same moment for 197 s, while the gateway kept running: no restart, no WiFi drop recorded, the main loop never stalled for more than 0.8 s. The cause (the PC's network, the access point, or the gateway's network stack) is not known. If you see the web UI and telnet stop answering for minutes, a capture of it helps.
- **The port 25238 line filter (TASK-1695) has been tested on the host only.** Our bench unit has no PIC. If OTmonitor or the Home Assistant `opentherm_gw` integration misbehaves while SAT is on, `ext_bridge_dropped` in `/api/v2/sat/status` and the telnet log show what the bridge dropped.
- **A rare `/favicon.ico` request gets no answer** (TASK-1675). In the bench two-tab test of beta.112, 4 of 400 requests got no answer within 12 s; the one with a logged path was `/favicon.ico`. It was not reproduced with favicon requests alone, and the cause is not found yet. Since TASK-1696 the favicon no longer opens a file; the later two-tab runs (500 requests) had no timeout, but this was not tested specifically for that rare case. If a page hangs while loading, a capture of it helps us.
- **The diagnostic recorder has not run on a board with PSRAM** (TASK-1665, TASK-1666). It was tested on a board without PSRAM, with its 4 KB ring. The 1.5 MB ring of an S3 Mini with PSRAM, the time to download it and the web UI response during that download are not measured yet.
- **The OLED fix (TASK-1678) has not been seen on a display.** It is proven by host tests only; our bench unit has no OLED. If your OLED stays dark during the WiFi setup, please tell us.
- **The skip-portal boot after a WiFi restart does not yet stop the beta AP fallback** (TASK-1455). On the boot after a WiFi restart, the boot itself skips the setup portal and the AP fallback. But this is a beta build: after 2 more failed retries (about 1 minute) it still starts its fallback access point, as all beta builds do when the saved WiFi stays away. Only the "No setup portal when Wi-Fi is down" setting (TASK-1645) keeps it closed. The setting itself has been bench-tested for a 5 minute outage, not for days.
- **The new WiFi features have not run in the field** (TASK-1450, TASK-1455, TASK-1446, TASK-1444). The dead-link recovery was seen once on the bench (forced reconnect, then the restart); its guards, the restart cap, the skip-portal boot and the outage record under a real WiFi drop are covered by host tests only. If your WiFi drops, the `Drop :` line in the telnet banner and the `wifi_drop_*` keys in `device/info` are what we would like to see.
- **A boot with the telnet console switched off** was checked in the code only (TASK-1644); switching it off and on while running was bench-tested.
- **Decoding a core dump:** the thread listing of `esp-coredump` may fail inside the tool. The decode still shows the panic reason. Send the downloaded file anyway.
- **The OT-Direct thermostat relay change (TASK-1600)** has not been seen on a bench with a thermostat attached; in the field it ran on one unit with a thermostat for 20 minutes (beta.127, standby).
- **No Immergas boiler has run this firmware.** The Immergas `TP=11:12` behaviour is proven by host tests and by the commands seen on the bench only. On the beta.17 bench the `MM=` could reach the PIC up to about 1 s before its `TP=11:12`; this is open with Sergeant D, and the queue has changed since (strict first-in, first-out, TASK-1569), so the order was not re-checked on this build. Since beta.48 (TASK-1581) SAT also sends `TP=11:12=80` before the `MM=T` at the maximum PWM duty; that is likewise untested on a real Immergas. If you own an Immergas, your capture is valuable.
- **The 3-hour on/off heat-pump cycle** (TASK-1542) has run on the bench with a simulated heat pump, not on a real heat pump or house. Treat it as experimental.
- **An undecodable crash** was seen once on our bench (TASK-1498): a core dump with the program counter in IRAM, and no matching ELF file to decode it. Not reproduced. If you see a crash, the capture tool grabs the crash log.
- **One crash seen in the field** (alpha.566, TASK-1538): a panic inside a memory allocation, with a backtrace too short to name the caller. Not reproduced; crash reports now keep up to 16 return addresses. If your unit reboots unexpectedly, send the crash log (`GET /api/v2/device/crashlog`) and `reboot_log.txt`.
- **Free memory dips under heavy diagnostic load** (TASK-1528). During long capture runs the lowest free-heap mark fell to between 0.5 and 1.7 KB for a moment, while the free heap stays around 71 KB. No crash was linked to it. Under investigation; the new `hd_dip_*` keys in `device/info` show what held the memory at that moment.
- **Cycle verdicts may differ from beta.38** (TASK-1584, 1585, 1586, 1588): the classifier now follows thermo-nova more closely. The change is covered by host tests against the thermo-nova reference, not by a long field run.
- **SAT's flame-out protection (TASK-1762) has not run on a real boiler.** If SAT switches to pulse mode more often than you expect, look for the SAT log line "Control mode: PWM (fail-safe: ...)", and please send a capture.
- **SAT is still being validated.** A list of open SAT questions is with Sergeant D, the SAT designer; his answers can change SAT behaviour in later builds. The ignition logic for a boiler without a return temperature (TASK-1371) was bench-tested; it has not run on such a boiler in the field.
- **Heating curve self-tune** (on by default) passed a 4-day run in the bench simulation (TASK-1525): hourly and daily records, and on simulated day 3 the curve was raised one step. It has not yet run for days on a real house. Switch it off if you want a fixed coefficient. The phase split of beta.176 (TASK-1796) and the changes of beta.189 and beta.190 (TASK-1815, TASK-1816) are covered by host tests and short bench runs in the SAT simulation; the two-phase decision over several real days is not tested yet.
- **Pressure alarm:** a pump-head step of about 0.3 bar after the pump stops can still raise it. It is display only and sends no command.
- **Updating through the Home Assistant panel** completed in 2 of 3 bench runs (one 502 during the upload). Update by IP when you can.
- **Filesystem update while a capture runs** (TASK-1524): on our bench a filesystem update failed once while a long capture was connected, and the unit served the file explorer upload page until a second update. Settings were kept. Stop the capture before you update.

---

## How to report

- Discord: #beta-testing or #alpha-testing, with the capture upload zip. #beta-testing needs the Beta tester role: ask for it in #english-support or #nederlandse-ondersteuning, and until you have it, report there.
- GitHub: [open an issue](https://github.com/rvdbreemen/OTGW-firmware/issues) and attach the capture files.

Thank you to everyone who tested the alphas and betas, to gruijter and maarten_41762 for the detailed feedback and the field captures, to tjfs for the OLED, OT-Direct, override, gateway `TT=`, pump symbol, pressure, home dial and update page reports and the captures, to Crash-Evans for pull request #699 and the macOS capture runs, to André Brait for the pull requests (#697 and #698 in beta.118), the code size analysis and the page-load report, and to Sergeant D for the SAT design decisions.
