# OTGW-firmware v2.0.0-beta.405 Release Notes

**Release date:** 2026-11-21
**Branch:** dev
**Build:** 2.0.0-beta.405 (the githash is in the asset names)
**Compare:** [v2.0.0-beta.401...v2.0.0-beta.405](https://github.com/rvdbreemen/OTGW-firmware/compare/v2.0.0-beta.401...v2.0.0-beta.405). "What changed since beta.401" is new for a beta.401 user; a user of an older beta also reads the sections below it.

> **Disclaimer: this is a beta of 2.0.0.** It is not a stable release. It has had alpha and beta testing on a small group of units, but not the long field runs a release needs. What was and was not tested for this build:
>
> - Since beta.401: the 4 builds beta.402 to beta.405, all web UI changes. Each has a test on the real web UI files that fails on the old code and passes on the change. Bench checks on the OTGW Classic bench unit for the About/Credits cards (beta.403) and the accessibility structure (beta.404, 8 of 8 checks passed). Nothing in SAT control, MQTT or the firmware itself changed.
> - From beta.392 to beta.401: the 9 builds beta.393 to beta.401. Each firmware change has host tests on the real code that fail on the old code and pass on the change, and each web UI change a test on the real web UI files. Bench runs on the OTGW Classic bench units for the web UI changes and the WiFi fallback, and a 60 min run of this release build. Nothing in SAT control changed.
> - From beta.382 to beta.392: the 10 builds beta.383 to beta.392. Each firmware change has host tests on the real code that fail on the old code and pass on the change, and each web UI change a test on the real web UI files. Bench runs: 31 min regression runs on the OTGW Classic bench unit for beta.386, beta.389 and beta.391, and a 60 min run of this release build. The bench units have no boiler, so the SAT rule of beta.391 has not run with a real boiler yet. The WiFi recovery of beta.385 is proven in host tests only.
> - From beta.374 to beta.382: the 8 builds beta.375 to beta.382. Each firmware change has host tests on the real code that fail on the old code and pass on the change, and each web UI change a test on the real web UI files. Bench runs on the OTGW Classic bench unit with PIC 6.8 and on the OT-Direct bench unit without PSRAM, no thermostat or boiler connected: for the SAT changes these are regression checks only, SAT control against a real boiler was not exercised. None of these changes has run on a tester's real heating system yet. The release build ran 1 h on the OTGW Classic bench unit with MQTT connected, without a restart.
> - From beta.360 to beta.374: the builds beta.361 to beta.374 (the number beta.366 is not in that release). Each firmware change has host tests on the real code that fail on the old code and pass on the change. Bench runs: the OTGW Classic bench unit with PIC 6.8 and the OT-Direct bench unit, no thermostat or boiler connected. None of these changes has run on a tester's real heating system yet. The release build ran 1 h on the OTGW Classic bench unit with MQTT connected, without a restart.
> - From beta.351 to beta.360: the 9 builds beta.352 to beta.360. Each firmware change has host tests on the real code that fail on the old code and pass on the change. Bench runs: the OTGW Classic bench unit with PIC 6.8 and the OT-Direct bench unit, no thermostat or boiler connected; the tolerance band has not run with a real boiler yet. The release build ran 1 h on the OTGW Classic bench unit with MQTT connected, without a restart.
> - From beta.339 to beta.351: the 12 builds beta.340 to beta.351. Each firmware change has host tests on the real code that fail on the old code and pass on the change. Bench runs: the OTGW Classic bench unit with PIC 6.8, no thermostat or boiler connected (SAT behaviour there came from the firmware's software simulator, not from hardware) and the OT-Direct bench unit (no boiler, no thermostat); SAT control was not tested with a real boiler. Per change the notes below say whether it was checked on the host, on the bench, or both. The release build ran 1 h on the OTGW Classic bench unit with MQTT connected, without a restart.
> - From beta.323 to beta.339: the 16 builds beta.324 to beta.339. Each firmware change has host tests on the real code that fail on the old code and pass on the change, and the capture tool changes have tests on the real tool. Bench runs on the OT-Direct bench unit: the Home Assistant card of TASK-1979 checked on the MQTT side (discovery, target, presets, SAT off and on, republish, heartbeat), smoke runs of TASK-1867, TASK-1990, TASK-1412.18.1 and TASK-1932 with the SAT simulation only (no real boiler controlled by SAT), and the TASK-1804 command answers. The release build ran 1 h on the OTGW Classic bench unit with MQTT connected, without a restart.
> - From beta.294 to beta.323: the 24 builds beta.295 to beta.323 (the numbers beta.302, beta.303, beta.318, beta.321 and beta.322 are not in this release). Each firmware change has host tests on the real code that fail on the old code and pass on the change, and each web UI change a test on the real web UI files. Each alpha build was bench-tested before it was published; what was not checked on a bench is named per item under "What changed since beta.294".
> - From beta.250 to beta.294: the 42 builds beta.252 to beta.294 (the numbers beta.251 and beta.287 are not in this release). Each firmware change has host tests on the real code, and each web UI change a test on the real web UI files; for each change the task records a run that fails on the old code and passes on the change. Each alpha build was bench-tested before it was published; the release candidate beta.294 passed 22 of 23 bench checks on the OT-Direct bench unit (the one is the bare `CS=` gap, fixed since by TASK-1957). What was not checked on a bench is named per item under "What changed since beta.250".
> - From beta.237 to beta.250: the twelve builds beta.238 to beta.250 (the number beta.249 was not used). Each firmware change has host tests on the real code, and each web UI change a test on the real web UI files; for each change the task records a run that fails on the old code and passes on the change. Bench runs on the OTGW Classic with an S3 Mini (PIC 6.8) and on the OT-Direct bench unit (a board without a PIC and without PSRAM), one or more per build. On the OTGW Classic with an S3 Mini: a filesystem update deliberately interrupted by a panic (on a test build with an abort hook) lost 16 settings, the boot counter and the reboot log on the old code and kept all of them on the fix, and the shipped beta.238 then updated, saved settings and rebooted with no setting changed (TASK-1774); a test build of beta.241 showed the raw reset code of a task watchdog and an RTC watchdog in the reboot log and in `device/info`, on both bench units (TASK-1904); beta.242 sent 113 `CS=` in about 40 minutes, 2.7 per minute, none dropped, largest gap 30.11 s (TASK-1924); on beta.243, 3 restarts through `/ReBoot` and 2 resets over USB kept the WiFi and opened no portal (TASK-1930); beta.245 served existing files without the web UI files (TASK-1926); beta.246 served a file that beta.245 answered with the start page (TASK-1936); beta.247 dropped the weather outside value from the graph of a hidden tab, beta.246 kept it (TASK-1875); beta.248 kept the restart reason of an update through safeboot, beta.247 lost it (TASK-1918); beta.250 sent no old `CS=` just before a new one (29 of 37 on beta.242), largest gap 30.10 s at a 30 s and 31.00 s at a 60 s control interval (TASK-1924). On the OT-Direct bench unit: beta.239 had no regression (`CS=45` expired after 64.3 s, `CS=5` stayed for 2 minutes, `CS=0` released it, no setting changed; TASK-1808); beta.240 showed the SIMULATION badge from the health poll with 0 calls to `/api/v2/simulate` in 3.2 minutes (TASK-1915). The release candidate beta.250 (build 2.0.0-beta.250+baf84d9) ran a soak on the Classic bench unit for 1 h 02 min with MQTT connected: bootcount stable, no crash log entry. No 24 h soak was run on this build, and a capture run from a release zip on macOS by a tester is still missing; Robert keeps both as gates for the stable 2.0.0, not for betas (TASK-1544). **Not tested on a bench:** the Data-Invalid answer of OT-Direct master mode to a thermostat (TASK-1808: no bench unit has a thermostat or a boiler; host test only); a reset by the external 0x26 watchdog (TASK-1904: no bench unit has the chip; host test only); the three-reset gesture with the reset button or the plug (TASK-1930: host test only); the 60 s flow follow of SAT continuous mode after an ignition and the 2 s start-up of a boiler without a return temperature with the new `CS=` timing (TASK-1924: not reached in the bench simulation; host tests only), and a 30 minute uninterrupted `CS=` run (the beta.250 bench ran 2 x 9.5 minutes); a power cut during a filesystem update, which the fix does not cover (TASK-1774); the capture tool fix (TASK-1922: host test only).
> - From beta.224 to beta.237: the thirteen builds beta.225 to beta.237. Each firmware change has host tests on the real code, and each web UI change a test on the real web UI files; for each change the task records a run that fails on the old code and passes on the change. Bench runs on the OT-Direct bench unit (a board without a PIC and without PSRAM) and on the OTGW Classic with an S3 Mini (PIC 6.8), mostly one per build. On the OT-Direct bench unit: beta.226 took 20 restarts through `/ReBoot` with a connection storm running through shutdown and boot, with no panic and the restart page arriving every time (TASK-1746.1; that run is a regression check, the repair itself is proven by a host test), and showed the new self-tune MQTT topics on the broker, none with self-tune off (TASK-1881); beta.228 polled `/api/v2/simulate` and `device/info` every 2 s for 125 minutes with a filesystem write every minute, 3,736 poll pairs, no restart (TASK-1899; the later beta.230 differs in the touched files by banner lines only); beta.232 logged the simulation note once per run and complete (TASK-1891); beta.234 and beta.236 measured the `CS=` expiry at 63.53 s and 63.84 s and the `C2=` expiry at 63.73 s, `CS=5` staying for 2 minutes, and `CS=0` and `CS=0.001` releasing within about 1 s (TASK-1834, TASK-1917); beta.235 ran a 30 minute load on the Dallas labels with a simulated sensor: 900 GET requests, 0 answered 503 (8 before the change), 10 of 10 read-after-write (TASK-1912). On the OTGW Classic with an S3 Mini: beta.227 and beta.229 had no regression and the corrected panel titles showed on the real page (TASK-1790, TASK-1885); the restart-during-boot upload race of TASK-1909 was reproduced on beta.230 in 3 of 3 tries and gone on beta.231 in 3 of 3 tries (reboot counter +2 and both log lines every time); beta.233 saved the settings back to back and rebooted 3 times with no error in 7 flushes and no leftover temp file (TASK-1913). The release candidate beta.237 (build 2.0.0-beta.237+c66c9b9) ran a soak on the Classic bench unit from 07:03 to 08:04 on 14 November (1 h) with MQTT connected: bootcount unchanged, no crash log entry. On the OT-Direct bench unit the same build was checked for no regression: `CS=45` expired after 64.06 s, `CS=5` stayed for 2 minutes and `CS=0` released it after 1.2 s, the restart counter rose by 2 (the flash), the crash log stayed empty and no setting changed (TASK-1811). No 24 h soak was run on this build, and a capture run from a release zip on macOS by a tester is still missing; Robert keeps both as gates for the stable 2.0.0, not for betas (TASK-1544). **Not tested on a bench:** the new outside temperature rules for a PIC sensor or an `OT=` value (TASK-1896, TASK-1897: the bench has no thermostat and no boiler, OT-Direct loopback never asks MsgID 27, and no capture of a PIC with a DS18B20 exists yet, so the frame sequence comes from the PIC 6.8 source; host tests only, field confirmation by indigo_light is open); the late-PIC case of the Auto board mode (TASK-1790: the bench cannot hold the PIC silent at boot; host test only) and a bare S3 module with PSRAM and without a PIC; the first Dallas reading (TASK-1824: no bench unit has a DS18B20; host test against a sensor model only); a reset or power cut in the middle of a settings save (TASK-1913: a host model only, no bench reset is possible) and the password guard on `/settings.tmp` (static check only); the Data-Invalid answer to MsgID 99 (TASK-1907: OT-Direct loopback never reaches that branch; host test only); the thermostat write path of master mode, which is the new part of TASK-1811 (no bench unit has a thermostat on the slave side), and the gateway-mode substitution next to the new `CS=` timings (TASK-1834, TASK-1917); the failure path inside the connection accept that TASK-1746.1 repairs (host test on the real code only); the filesystem read change of TASK-1899 against the field watchdog resets themselves (field confirmation by gruijter is open); the Dallas labels with a real DS18B20 (TASK-1912: simulated sensor); the flow, return and flame fields of the SAT status against a real boiler (TASK-1885, carried over from beta.223).
> - From beta.218 to beta.224: the six builds beta.219 to beta.224. Each firmware change has host tests on the real code, and each web UI change a test on the real web UI files; for each change the task records a run that fails on the old code and passes on the change. Bench runs on the OT-Direct bench unit (a board without a PIC and without PSRAM): beta.219 raised the free stack of both internal IPC tasks by about 250 bytes (ipc0 from 152-232 to 404-484 bytes, ipc1 from 80-88 to 332 bytes; TASK-1870), and 21 restarts through `/ReBoot` gave no panic, with the reboot page arriving every time (TASK-1746; the old builds had 1 panic in 62 such restarts, so these counts alone do not tell the builds apart). Each of beta.220 to beta.223 was installed over WiFi on that unit and checked there with no extra restart, an empty crash log and no settings change, beta.222 and beta.223 also on the OTGW Classic with an S3 Mini; beta.224 was checked on the OT-Direct bench unit for its new features. On the OTGW Classic with an S3 Mini beta.219 raised the ipc0 stack margin from 80-88 to 340 bytes and stayed there during every filesystem write; 1 full update and 10 filesystem updates gave no panic (TASK-1870, TASK-1774). That is not proof on its own: at the old rate of 1 panic in 9 writes, 10 clean writes still happen about one time in three, so the measured stack margin is what carries it. The release candidate beta.224 ran a soak on the Classic bench unit (PIC 6.8) from 16:21 to 19:21 on 13 November (3 h), with MQTT connected since 16:28: bootcount unchanged, no crash log entry. No 24 h soak was run on this build. **Not tested on a bench:** the watchdog resets of TASK-1865 and TASK-1899 (not fixed in this build, see Known issues); the new answer status of `/api/v2/otgw/messages/<id>` for a real boiler's acknowledge, Unknown-DataID and Data-Invalid answers (TASK-1871: the bench has no boiler, host tests only); the new flow and flame fields of the SAT status and chart against a real boiler (TASK-1885: simulation only); the PWM phase record for a phase in which the burner never lights, for a phase that ends because its ON time ran out, on a PIC board and with a real boiler (TASK-1880: one simulated phase on the OT-Direct bench unit); the SAT page texts for a refused pulse mode and for the takeover after a lost connection (TASK-1884: web UI tests only); the SAT simulation fix (TASK-1869) was checked on the bench on the new build only, the old-code reference is from beta.216.
> - From beta.203 to beta.218: the 15 builds beta.204 to beta.218. Each web UI change has a test on the real web UI files that fails on the old files and passes on the change; each firmware fix has host tests on the real code, with a run that fails on the old code and passes on the fix recorded in its task. Bench runs on the OT-Direct bench unit (a board without a PIC and without PSRAM): beta.217, 38 reboots without a watchdog reset (TASK-1865); beta.218, 12 reboots without a reset, with the largest heap drop in one MQTT cycle after the connect down from 23-43 KB to 7-20 KB (TASK-1757). The OTGW Classic with an S3 Mini ran beta.217, where a filesystem update panicked once (see Known issues, TASK-1774), and then the release candidate beta.218: beta.218 ran a 1 h soak on the Classic bench unit (PIC 6.8) from 22:03 to 23:04 on 12 November with MQTT connected: no restart, no new crash log entry. No 24 h soak was run on this build, and a capture run from a release zip on macOS by a tester is still missing; Robert keeps both as gates for the stable 2.0.0, not for betas (TASK-1544). **Not tested on a bench:** the health check change (TASK-1865) against the field crash itself, which was never reproduced on the bench; the 60 s fallback of the SAT publish hold after an MQTT connect (TASK-1757); the self-tune rule for pulse mode on a gas boiler (TASK-1817) over days on a real house; the OT-Direct changes `RT=` (TASK-1842), the PIC's own answers for MsgID 6, 125, 56 and 57 (TASK-1844) and the single WRITE per command (TASK-1855) with a real thermostat and boiler.
> - From beta.174 to beta.203: the 29 builds beta.175 to beta.203. The firmware fixes have host tests on the real code, and the web UI changes tests on the real web UI files; for each fix the task records a run that fails on the old code and passes on the fix. Each batch of builds was installed over WiFi on the OT-Direct bench unit (a board without a PIC and without PSRAM) and checked there, with the settings compared before and after. The release candidate beta.203 then ran for 1 hour on both bench units, the OT-Direct bench unit and the OTGW Classic with an S3 Mini, with MQTT connected: the OTGW Classic with an S3 Mini (PIC firmware 6.8) from 06:09 to 07:11 on 12 November (1 h 02 min) and the OT-Direct bench unit from 06:10 to 07:11 (1 h 01 min), each without a restart (bootcount unchanged), with MQTT connected throughout and no new crash log. The free heap went from 55,016 to 53,884 bytes on the Classic and from 68,832 to 68,668 bytes on the OT-Direct unit, with the largest free block flat at 31,732 bytes on both; one hour is too short to call that a trend either way. **The OTGW Classic with an S3 Mini, the supported public hardware, ran only beta.203, for that 1 hour soak;** the builds beta.175 to beta.202 did not run on it one by one. **Not tested on a bench:** the OT-Direct boot change (TASK-1835) at frame level, because the bench has no boiler and no bus listener; the OT-Direct clock answer to a real thermostat (TASK-1840); the OT-Direct return temperature read in a thermostat slot (TASK-1759) with a real thermostat; SAT's own `PM=28` request for the return temperature on a PIC board (TASK-1759) with a real boiler and thermostat; the restore of the OT-Direct command state after a reboot with a real boiler (TASK-1836); the self-tune changes (TASK-1796, TASK-1815, TASK-1816) over days on a real house.
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

## What changed since beta.401

> **Running a SAT field run on beta.401 or earlier? Stay on it until your run is done.** This beta changes nothing in SAT control; it only changes the web UI.

The builds beta.402 to beta.405. Web UI only: the Home page shows the room temperature SAT regulates on, the About screen has new credits, and the web UI is easier to use with a keyboard and a screen reader.

### Web UI

- **beta.402: with SAT on, the Home page shows the room temperature SAT regulates on** (TASK-2946). That is your MQTT, BLE or other room sensor, the same value as the setpoint next to it and the Home Assistant thermostat card. The thermostat's own reading is in the tooltip, and only when the thermostat reports one (beta.403). With SAT off, or while SAT has no valid room temperature, the page shows the thermostat's reading as before. Tested: web UI tests that fail on the old code.
- **beta.403: the About screen at the end of Settings has three cards: Makers, Credits and Community** (TASK-2948), in the same type as the settings rows. It names the people and projects the firmware builds on and the community that tested and helped, with a link to the full list in [CONTRIBUTORS.md](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/CONTRIBUTORS.md). Tested: a web UI test that fails on the old cards; bench check on the OTGW Classic bench unit.
- **beta.404 and beta.405: keyboard and screen reader support** (TASK-2958, TASK-2982). A "Skip to content" link is the first Tab stop; the page has one main heading, labelled navigation and a heading per page; the active page, SAT view, Monitor tab and Advanced tab are marked for screen readers from the first load; the SAT switch is named "SAT"; disabled buttons look disabled; one focus ring that is visible on every background, also inside the scrolling page switcher; the Settings categories are buttons, so Tab, Enter and Space reach them. Tested: web UI tests that fail on the old code (beta.404: 8 of 8; beta.405: 12 of 12); bench check of beta.404 on the OTGW Classic bench unit.

### Thanks

To the UX review of the web UI, which found the keyboard and screen reader gaps, and to everyone named on the new About screen.

---

## What changed since beta.392

> **Running a SAT field run on beta.392? Stay on 392 until your run is done.** This beta changes nothing in SAT control. A field run on beta.392 counts for the 2.0.0 stable gate, and so does one on this beta.

The builds beta.393 to beta.401. Small changes before the 2.0.0 stable release: the update-check question no longer pops up over the page, the WiFi fallback access point is now part of the stable build too (and no longer starts while memory is critically low), and a few web UI and display fixes.

### All boards

- **beta.401: the WiFi fallback access point is no longer beta-only; it is in the stable build too** (TASK-2931; Robert's decision). When the gateway cannot reach your saved WiFi, it opens its own access point after 2 failed attempts so you can reach it, unless `wifinoportal` is set or memory is critically low (see beta.394). The log lines lose their "BETA" prefix. Tested: a host test that compiles the real WiFi decision in a beta and a stable configuration, red on the old code, green on the fix; the WiFi test suites updated and green; a stable-style build of this code ran on a bench unit.
- **beta.394: the WiFi fallback access point is no longer started while memory is critically low** (TASK-2923). Starting it then could crash the gateway (found on a bench unit while memory was held low on purpose). With free memory under 8 KB or the largest free block under 4 KB, the gateway keeps retrying your saved network and restarts after 10 failed attempts. Tested: a host test on the real WiFi code, red on the old code (2 of 4), green on the fix. Bench: with memory held low, two clean restarts and no crash (the old build crashed twice).
- **beta.400: the display (OLED) no longer shows a raw build token in a stable build** (TASK-2932). Betas are not affected; this prepares the stable build. Tested: a source scanner test, red on the old code; a stable-style build holds no stray token. Not verified: the OLED screen itself.

### Web UI

- **beta.393: the one-time question whether to check GitHub for new versions is a chip in the header** (TASK-2918). It no longer opens as a dialog over the page you are on, about 30 s after the page loads. A click on "Check for new versions?" shows the explanation and the No / "Yes, check" buttons; closing it without an answer means not now. Your answer is still stored on the gateway, as before. Tested: web UI tests that fail on the old code, including one on a 390 px phone screen that checks the popover stays on screen. Bench: no dialog over the page, popover on screen at 390 px, no script errors.
- **beta.393: the "New version available" notice now shows on phones** (TASK-2918). Since beta.387 it was hidden in windows 480 px wide or less.
- **beta.396: in the Monitor graph legend, dashed series (control setpoint, thermostat setpoint, SAT target) now have a dashed swatch, like their line**, on screen and in the PNG export (TASK-2037). Tested: web UI tests that fail on the old code; bench check of the legend and the PNG.
- **beta.399: the Settings page now calls SAT the Smart Autotune Thermostat**, like the SAT page and the manual, in all four languages (TASK-2928). Tested: a test that fails on the old texts; bench check in all four languages.

### Capture tool

- **The capture tool now records a sleep of the capture computer as a gap in its gap list** ("capture machine asleep or suspended"), so a silence caused by the PC is no longer mistaken for the gateway (TASK-2922). Tested: tests that drive the real check with simulated clocks, red on the old tool.

### Diagnostics (prerelease builds only)

- beta.394, beta.397 and beta.398: bench test hooks to hold memory low and to make the web server act as if memory were nearly full (TASK-2026, TASK-2924, TASK-2921). They are compiled only into prerelease builds and do nothing unless a developer starts them over telnet.

(beta.395 is a one-line web UI test fix with no visible change.)

### Thanks

To indigo_light, whose macOS run and network outage showed the capture tool needed a clearer gap list, and to the testers lining up field runs for the stable release.

---

## What changed since beta.382

The builds beta.383 to beta.392. For users with SAT on, the main change is Sergeant D's new rule against switching back and forth between continuous and PWM within one burn. For everyone: a fix for a crash when two web requests arrive at the same time while memory is nearly full, a WiFi recovery fix for a nearly full memory, an optional "new version available" notice, and the web UI no longer calls SAT experimental.

### SAT

- **beta.391: during one burn SAT enters PWM at most once** (TASK-2913; Sergeant D's entries 428, 429 and 430, "top priority for 2.0.0"). Once the burner runs continuous it stays continuous until the flame goes out; the only way back to PWM in that burn is the fail-safe, now at a flow 3.0 C above the request for 30 s (was 3.5 C). Only one start per flame counts toward the cycles-per-hour limit. Before, the switches back and forth inside one burn used up the starts, and the limit then held heating back while the room was still too cold. Thanks indigo_light for the 24 h log that showed it. Tested: host tests on the real SAT code that fail on the old code. Bench: 31 min regression on the OTGW Classic bench unit, no restart (the bench has no boiler, so the rule itself does not run there). Not verified yet: real boilers; that is what the field runs on this beta are for.
- **beta.386: the PID derivative gain factor is 0.05 instead of 0.063** (TASK-2043; Sergeant D's entry 425, in 2.0.0 per entry 427). The derivative reacts a little less strongly to a changing room temperature. Tested: host tests on the real PID. Bench: 31 min regression with SAT enabled (no boiler), no restart. Not verified yet: the effect in a real house.
- **beta.390: the web UI no longer calls SAT experimental** (TASK-2909). SAT stays opt-in and off by default, and the setup wizard still asks you to confirm before it takes over your boiler. The heat-pump type is still marked experimental. Tested: tests on the texts in all four languages; bench: no "experimental" left on the SAT page, the wizard confirmation works, Cancel writes nothing.

### All boards

- **beta.389: fixed a crash when two web requests arrive at the same time while memory is nearly full** (TASK-2910). The busy answer (503) itself needed memory and could crash the gateway; it now closes the connection without building an answer when memory is that low. The crash has existed since September. Tested: a host test on the real REST code that crashes on the old code and passes on the fix. Bench: 31 min regression without a crash, and 25 parallel requests got 12 normal answers and 13 busy answers (503 with Retry-After). Not verified on hardware: the nearly-full-memory case itself (host test only).
- **beta.383: the WiFi check counts a probe that cannot run for lack of memory as a dead link when the largest free memory block is under 4 KB** (TASK-2026, ADR-258). Before, a gateway with a nearly full memory could stay on WiFi without working. Tested: host tests on the real WiFi code that fail on the old code.
- **beta.385 and beta.388: the dead-link count is kept across a WiFi re-association while memory is nearly full** (TASK-2026, ADR-261), so the gateway still recovers by itself, and the log says when that happens. Tested: host tests on the real code that fail on the old code; on the bench no regression. **Not proven on hardware** (see Known issues).

### Web UI

- **beta.387: optional "new version available" notice** (TASK-2023, ADR-262). Your browser asks GitHub for the latest release; the gateway itself never connects to the internet. It is off until you say yes to a one-time question, checks at most once per 6 hours, and sends no cookies or referrer. Tested: web UI tests on the real code; bench: no GitHub request before you agree or after "No", one request after "Yes".
- **beta.392: the "injected override" badge no longer disappears on a dropped connection** (TASK-2914). A connection reset during the overrides poll now keeps the badge, the same as a busy (503) answer, and the poll retries once. Tested: web UI tests that fail on the old code; bench with simulated answers (no OpenTherm bus on that bench unit). Not verified: a real injected override on a unit with a live bus.

### Capture tool

- **The flash zip now contains `sat_log_to_csv.py`, so the capture tool writes its SAT CSV files again** (TASK-2917). Before, every capture logged "csv conversion of telnet.log failed ... No module named sat_log_to_csv"; the raw telnet log was complete, so no data was lost. Thanks indigo_light, whose macOS run showed it. Tested: a test on the real zip builder that fails on the old build.py and runs the conversion from the unpacked zip.

### Diagnostics (prerelease builds only)

- beta.384 and part of beta.388: a bench test hook to reproduce a nearly full memory. It is compiled only into prerelease builds and does nothing unless a developer starts it over telnet.

### Thanks

To indigo_light (the 24 h log that found the start-limit problem, and the macOS capture run) and Sergeant D (the fast decision on it).

---

## What changed since beta.374

The builds beta.375 to beta.382. For users with SAT on, the main changes are a faster heating curve self-tune, no switch to PWM while the tolerance band pauses heating, and a fix for a brief hand-back in the middle of a burn. On boards without PSRAM, BLE can no longer be switched on.

### BLE (Bluetooth sensors)

- **beta.375: on boards without PSRAM, BLE can no longer be switched on, so the gateway cannot run out of memory and hang** (TASK-2025). The "Enable BLE sensors" switch is greyed out with the note "BLE needs PSRAM", a stored "on" is cleared at the first start, and REST (409 with the reason) and MQTT refuse it. The LOLIN S3 Mini and S3 Mini Pro have PSRAM and keep BLE as before. Tested: host and web UI tests that fail on the old code; on the bench unit without PSRAM, BLE stored on was switched off at boot and saved within 10 s, the scanner never started in 4 minutes, free memory stayed at 47-51 KB. Not verified: why a gateway that ran out of memory hung instead of restarting itself (TASK-2026, open).

### SAT

- **beta.376: when the room leaves the 0.1 C band around the target, the PID integral is halved once and then held** instead of being reset to 0; back inside the band it builds up again from there. The same rule applies to the zone thermostats (TASK-1890; Robert's decision, entry 333; zones confirmed by Sergeant D, entry 414). Tested: host tests on the real PID that fail on the old code. Bench: regression only, no boiler on the bench. Not verified yet: the effect on a real house.
- **beta.377: SAT no longer switches from continuous to PWM while the tolerance band pauses heating** (TASK-1887; Sergeant D's entries 410 and 417). Before, leftover heat after hot water could trigger the switch with the burner off. Both switch rules (2 C for 60 s, and the 3.5 C for 30 s fail-safe) now only run while heating is allowed. Thanks indigo_light for the capture that showed it. Tested: host tests on the real control tick that fail on the old code. Bench: regression only. Not verified yet: a real boiler.
- **beta.379: the heating curve self-tune reacts within hours instead of days** (TASK-1889, ADR-260). It decides every counted hour on the last 12 counted hours and changes the curve by 0.05 per step (was 0.1 at most every 3 days); after a step it waits at least 12 counted hours. The exclusions and checks stay. Sergeant D confirmed these numbers (entries 423 and 424). The saved tuner progress of an older build is dropped once (file format v3), so it starts counting again. The MQTT and REST field names changed: see BREAKING changes. Tested: a new hour-window host test that fails on the old tuner, and a closed-loop convergence test; bench: the REST fields, the file restore and the tune card in all four languages. Not verified yet: how fast it converges in a real house.
- **beta.380: fixed a brief hand-back in the middle of a burn** (TASK-2039). When the boiler's outside temperature (or the one Home Assistant sends) was about to turn one hour old, the weather service had not been fetched, so both sources went stale at the same moment: SAT handed the boiler back (setpoint 0) and took over again a fraction of a second later. The weather is now fetched 10 minutes before the other source expires. Thanks marceld91d for the 24 h capture that showed it. Tested: a host test that replays marceld91d's sequence and fails on the old code. Bench: regression only, the weather is still fetched every 15 minutes. Not verified yet: a field run.
- **beta.381: an outside temperature that Home Assistant sends to the gateway (`OT=`, boards with a PIC) now counts for SAT** (TASK-2040; entries 330 and 331). Before, the PIC answered the thermostat with that value itself and SAT did not see it as fresh, so it used the weather service instead. Tested: a host test on the real frame path that fails on the old code. Not verified yet: a gateway with a thermostat attached (the bench units have none); a field check is welcome.

### Web UI and graphs

- **beta.378: the Monitor graph and its CSV export show SAT's target temperature as its own dashed line while SAT is enabled**, also in the 24 h history (TASK-2018, ADR-256). The capture tool writes it too. A window without SAT gives exactly the same data as before. Tested: a host test on the history format (a 24 h response grows from 23.9 to 26.8 KB with SAT on) and web UI tests that fail on the old code; bench on the OTGW Classic unit.
- **beta.382: the SAT messages "... boiler handed back" now say "SAT paused"**: "SAT paused: no outside temperature for 1 hour, your thermostat controls the boiler" and "SAT paused: no OpenTherm connection, the boiler is back under normal control", in English, Dutch, German and Greek (TASK-2036; Robert's wording). Thanks gruijter for the feedback. Tested: tests on the texts and the language packs; bench: shown correctly in all four languages, no overflow at phone width.

### Thanks

To gruijter (an A/B comparison of SAT and his own thermostat with gas meter data, and the wording feedback), marceld91d (the 24 h capture that found the hand-back) and indigo_light (the band-pause capture).

---

## What changed since beta.360

The builds beta.361 to beta.374. The main change for users with SAT on is the switch from continuous to PWM: it now needs the flow to stay 2 C above SAT's request for a full minute, which should stop the back-and-forth a tester saw on a Viessmann Vitodens.

### SAT

- **Change: SAT switches from continuous to PWM only when the flow stays at least 2 C above its request for 60 seconds** (was 1.5 C for 30 seconds). The margin is the setting `satpwmswitchmargin` (0.5 to 5.0 C), now 2.0 C by default; a stored old default of 1.5 C moves to 2.0 C once, a value you chose yourself stays. The safety switch at 3.5 C for 30 seconds is unchanged (Sergeant D entries 407 and 359; Robert, Q-407a; ADR-254, Proposed; TASK-2005, beta.361). Checked with host tests and a replay of a tester's Vitodens capture through the real code: the old rule switched 3 times in that stretch, the new rule not at all. **Not yet run with a real boiler.**
- **Fix: SAT learns how fast your house cools only while SAT is actually controlling the boiler** (TASK-2010, beta.362). Until beta.361 the learning also ran while there was no boiler reply, so a room that simply cooled down was learned as heat loss. **If SAT ran for a long time without a boiler connection (for example a test setup), reset the learned thermal coefficient (`SATthermalcoeff`).** Bench: the OTGW Classic bench unit without a boiler kept the coefficient unchanged for 78 minutes in the situation that changed it before. Learning while SAT controls a real boiler is covered by a host test only, not by a bench run.
- **Fix: a settings re-read no longer drops your active SAT preset, and starting the capture tool no longer triggers that re-read** (TASK-2013, beta.369). The capture tool asked the gateway to re-read its settings at start and after every telnet reconnect; that re-read put back an old stored SAT target and set an active preset to none. Found and traced by indigo_light, thank you.

### Filesystem and main loop

- **Fix: a setting or SAT state saved while a filesystem update starts is no longer lost** (TASK-2007, beta.363 and beta.365). The background writer now pauses and flushes at the start of a filesystem update and stays paused until the saved files are put back. Bench on the OTGW Classic bench unit with the same reproduction: beta.364 lost the save, beta.365 kept it.
- **Fix: the once-a-minute filesystem checks (a write probe and a mount check) now run on the background file writer instead of the main loop**, which removes the short loop stalls they caused (TASK-1995 Step B, beta.370; bench, 62 minutes on the same filesystem: 0 stalls against 2 on the old code). Found thanks to gruijter's beta.250 capture.
- **Fix: the filesystem health check no longer writes to flash every 5 minutes**, which removes a short stall it could cause; a filesystem that can no longer be written is now reported by health status DEGRADED after the next SAT state-file write; with SAT off there is no periodic state-file write, so this check only runs while SAT is on (TASK-2020, beta.373; host tests red on the old code; bench on a bench unit: 122 minutes on beta.373 with 0 loop stalls, the filesystem check flat at about 158 ms, memory flat, MQTT 122 of 122, no restart; on beta.370 the mount check kept climbing).

### Home Assistant and MQTT

- **Change: the SAT Simulation switch is now shown as unavailable while a real boiler answers**, because simulation is refused then. Before, it jumped back to off without a reason. Needs Home Assistant 2021.2 or later. Thanks to thaghostnl for the report (TASK-2014, beta.374, ADR-255; host test red 5/6 on the old code, 6/6 on the fix; bench on the OTGW Classic bench unit with a boiler emulated by a test hook. Not yet seen greyed out in a real Home Assistant with a real boiler; a field check is welcome).
- **Fix: a retained `<label>/override` topic left on the broker from before a reboot is now cleared after the next connect**, so a client that reads it (for example Home Assistant) no longer sees an override that is not active (TASK-1991, beta.364; bench on the OT-Direct bench unit).

### Web UI

- **Change: the SAT weather card uses Home Assistant's sun position (`sat/sun_elevation`) while a value younger than 1 hour is published, and then asks for no location; without it, it still asks for latitude and longitude** (TASK-2015, beta.371). Not everyone uses Home Assistant, so the location stays the default. Suggested by thaghostnl, thank you. The local sun-position fallback is host-tested only.

### OT-Direct (boards without a PIC)

- **Change: OT-Direct command errors now read like the PIC's, for example `CH: BV`** (TASK-1804 item B3a, beta.362; bench on the OT-Direct bench unit, 4 of 4).
- **Change: OT-Direct handles boiler messages the boiler does not support like the PIC does** (TASK-1804, beta.367): `MH=`, `MW=`, `M2=` and `HW=P` skip the write of MsgID 99 when the boiler is known not to support it, and an ID requested with `SR=` that the boiler answers three times with Unknown-DataID is left out from then on (bench on the OT-Direct bench unit, no boiler).

### Capture tool

- **Fix: on macOS with the built-in Python 3.9 the capture tool now also writes the graph history as CSV**; it failed with "write_text() got an unexpected keyword argument 'newline'" (TASK-2016; found in indigo_light's Mac capture, thank you).
- **Fix: the device check of the capture tool no longer counts an OT-Direct route that answers "not on this board" as a failure** on an OTGW Classic (TASK-2012; host test on the real tool).

### Diagnostics (no change in behaviour)

- New telnet line `[fs-slow]` names a filesystem step that took 100 ms or more; it helps us find the periodic loop stalls (TASK-1995, beta.362 and beta.368).
- When SAT takes over from the thermostat, the firmware logs which step uses heap memory (telnet lines `[HEAP] sat-takeover-probe`) (TASK-2019, beta.372; 21-minute bench smoke without a SAT takeover: no crash, no loop stall, heap flat; the trace itself is host-tested).

### Documentation

- Manuals and API docs updated for beta.360 to beta.374 (TASK-2021). Among them: the SAT sun elevation comes from `sat/sun_elevation` over MQTT while it is younger than 1 hour, otherwise it is computed from the weather latitude/longitude and the NTP time.

---

## What changed since beta.351

The 9 builds beta.352 to beta.360. The main change for users is the tolerance band: SAT now restarts heating just below the target + 0.1 C, after a pause of at least 10 minutes, so the room settles about 0.1 C higher than with beta.351.

### SAT

- **Change: the tolerance band now restarts heating as soon as the room is below the target + 0.1 C, after a pause of at least 10 minutes.** With beta.351 heating restarted only below the target - 0.1 C. What you will notice: the room settles about 0.1 C higher, and after a band stop the boiler stays off for at least 10 minutes, counted from the moment the band turns heating off. A band decision while heating was already off (for example a warm room right after a restart) starts no pause, so the first heating start is not delayed. If the room temperature is lost during the pause, heating follows the heating curve at once (Sergeant D entries 401, 402, 403; our readings in entries 405 and 406; ADR-253; TASK-1887, beta.359 and beta.360; host tests, and a bench run on the OTGW Classic bench unit without a boiler; the band itself has not run with a real boiler yet).
- **Change: PWM ON phases that start during a band pause no longer count as starts** toward the hourly start limit, and the SAT narration names the value that decided a band stop or restart (Sergeant D entry 398, interpretation entry 400; TASK-1887, beta.358; host test and bench).
- **Fix: SAT's state files are written by a background writer** instead of on the main loop, so a slow flash write no longer stalls the control loop (TASK-1989, beta.357; host test, bench without SAT ticks).

### OT-Direct (boards without a PIC)

- **Fix: a client that connects to port 25238 right after a reboot no longer gets a stray "Thermostat disconnected" line**, which made pyotgw (Home Assistant's OpenTherm Gateway integration) fail to connect about 2 times in 5 (TASK-1999, beta.353; host test and bench: 10 of 10 connects on the fix, 3 of 5 failed before).
- **Fix: master mode sends the room setpoint of `BS=` to the boiler and puts it back after `TT=0`** (TASK-1751, beta.354; host test and bench).

### MQTT and memory

- **Fix: the 26 hourly statistics topics go out one per 100 ms instead of all at once**, which removes the brief free-memory dip at the full hour on boards without PSRAM (the known issue of beta.351) (TASK-2004, beta.355 and beta.356; host test and bench: no dip at the full hour with SAT on, 26 of 26 topics delivered).

### Diagnostics

- **Change: the diagnostic recording status reports how long one record takes to store** (`append_count`, `append_avg_us`, `append_max_us`) (TASK-1665, beta.352; bench on boards with and without PSRAM).

## What changed since beta.339

The 12 builds beta.340 to beta.351. Most of them carry today's SAT decisions by Sergeant D: every heating start in PWM first, no PWM block from the boiler's configuration message any more, a tolerance band around the room target, a quiet first minute after a reboot, the latest flow again during PWM, and a separate switch margin. How each change was checked is in brackets: host test (the real code compiled on a PC, red on the old code and green on the change), bench (on one of our bench units), or both.

### SAT

- **Change: every heating start runs in PWM first.** When the heating starts (after a pause, after SAT is switched on, or after a restart), SAT begins in PWM with the first phase ON, and only decides to stay in PWM or move to continuous once the flame has burned for 60 s. This gives the boiler a short warm-up and lets SAT pick up in PWM after a restart. It applies whatever control mode you chose, also with a manual "continuous"; during those 60 s none of the usual switches to continuous happen, not even at the maximum request, and after that the usual rules decide. A start where the flame was already burning when SAT took over is left as it was. Thermo-nova starts in continuous; this firmware differs on purpose. On a boiler without a return temperature the start-up handling (the flame-off hold and the 20 s flow flood) now always runs at the ignition of a PWM start (Sergeant D entries 385, 387, 389, 391, 394; TASK-1942, beta.351; host tests and a software simulation; not run on hardware or with a real boiler).
- **Change: the boiler's configuration message no longer blocks PWM.** SAT used to refuse PWM until the boiler had answered OpenTherm message 3, and to block it for good when that answer said "master low-off not allowed" (bit 4). Sergeant D decided this was wrong (entries 392 and 393): PWM is allowed from the start on every boiler. A boiler such as a Viessmann Vitodens that sets that bit can now run PWM (TASK-2000, beta.350; host test; PWM entry confirmed on a Viessmann Vitodens on beta.350 in a tester capture, real boiler).
- **Change: a tolerance band around the room target.** When the room reaches the target + 0.1 C, SAT stops asking for heat (CH off) while PWM keeps its rhythm, so the burner does not restart sooner than the PWM cycle when the room swings around the target; a burn in continuous mode still runs at least 180 s first. With zones, eco lets the main room decide and comfort keeps heating while any room is more than 0.1 C below its own target (Sergeant D entries 376, 378, 384; TASK-1887, beta.349; host test, plus a short run in the software simulator; not run with a real boiler).
- **Change: no SAT command in the first 60 s after a reboot,** then the heating curve alone until the first room temperature arrives; the gateway's own start-up reads of the boiler still run at once (Sergeant D entries 388 and 395; TASK-1874, TASK-1866, beta.348; host test, plus a bench smoke with SAT off that showed no change in the boot behaviour; the 60 s wait with SAT on rests on the host test).
- **Change: the PWM ON setpoint follows the latest flow again.** The 2 minute flow average of beta.330 and beta.333 is gone; SAT uses the latest flow, as thermo-nova does (Sergeant D entries 375 and 383; TASK-1990, beta.343; host test, plus a bench smoke without SAT control).
- **Change: a separate switch margin from continuous to PWM.** SAT switches to PWM when the flow stays at or above the request + 1.5 C for 30 s; the new setting "PWM switch margin" (0.5 to 5.0 C, default 1.5) is in the web UI, REST and MQTT; the 3.5 C / 30 s safety switch stays (Sergeant D entries 327, 359, 360, 379; ADR-249; TASK-1888, beta.345; host test, and the setting checked on the bench; the switching itself rests on the host test).
- **Change: no-ignition and heating-failure events.** SAT raises a diagnostic event when the burner does not light or the heating fails, and leaves such hours out of the heating curve self-tune; it takes no control action (Sergeant D entry 358; TASK-1882, beta.341; host test, plus a bench smoke).

### PIC (OTGW Classic)

- **Fix: the PIC settings fill in about a minute after a restart instead of four to five.** The PIC settings cycle runs from the main loop, so the Home Assistant discovery no longer holds it back (TASK-1971, beta.340; host test and bench: all 15 PIC fields after 55 s instead of 275 s on the OTGW Classic bench unit).

### OT-Direct (boards without a PIC)

- **Fix: `SR=` rewrites the boiler's answer as the PIC does,** instead of answering the thermostat locally (TASK-1804 A6, beta.342; host test only: the bench has no thermostat side).

### Diagnostics

- **Fix: a stopped recording stays readable until the next start** on boards with PSRAM, so it can still be downloaded after Stop (TASK-1960, beta.344; host test and bench).
- **Change: the gateway reports the full fingerprint of its firmware** (`appelfsha256` in `/api/v2/device/info` and next to a core dump), so the capture tool can say whether a stored core dump belongs to the running build or an earlier one (TASK-1983, beta.346 and beta.347; host test and bench).

## What changed since beta.323

The 16 builds beta.324 to beta.339, plus three changes to the capture tool. SAT gets a steadier setpoint during a PWM ON phase and no false pressure alarm after a pump stop; Home Assistant's thermostat card follows SAT; OT-Direct (boards without a PIC) moves closer to the PIC again.

### All boards

- **Fix: an image that fails its first boot goes back to the safeboot updater at once.** With one OTA slot, a new image that crashed on its first boot was started a second time instead of handing over to the updater; the gateway now removes the stale boot record right after the filesystem mounts (TASK-1919, beta.327).
- **Fix: a filesystem update keeps the newest lines of the reboot log.** During a filesystem update the RTC snapshot kept the reboot log whole or not at all, so next to a large `settings.ini` the log was lost; it now keeps the newest whole lines that fit (TASK-1931, ADR-246, beta.326).

### Home Assistant and MQTT

- **Change: the thermostat card follows SAT while SAT is on.** Its target sets SAT's target, it shows SAT's target and the room temperature SAT regulates on, and it offers the SAT presets (comfort, eco, away, sleep, activity, home). With SAT off it works as before (`TT=`). The entity and its id stay the same; the lowest target on the card is now 5 C. New MQTT topics: `sat/climate_target` and `sat/climate_current` (TASK-1979, ADR-248, beta.331).
- **Fix: the retained `<label>/override` topic is removed when the override ends**, so Home Assistant no longer keeps showing an old override value. An override counts as ended when no override frame came for 10 minutes, so the topic goes away up to about 11 minutes after the last one (TASK-1985, beta.336).

### SAT

- **Change: the PWM ON suppression setpoint uses the average flow of the last 2 minutes** minus the suppression offset, instead of the latest reading (Sergeant D, entry 364). Only readings with the flame on, since the current ignition and outside hot water, count. In the first 2 minutes after the ignition the setpoint follows the latest flow, so it keeps up with a rising flow (entries 151, 336). This deliberately differs from thermo-nova, which takes the latest flow (TASK-1990, beta.330 and beta.333).
- **Fix: no false pressure-drop alarm after a normal pump stop.** Pressure readings of the 600 s after a CH mode change are no longer used for the drop rate (Sergeant D, entry 362), and after such a change the drop rate waits for 600 s of fresh readings (Robert, entry 372). On gruijter's capture the old code raised two false alarms; the new code none. A real pressure loss right after a CH mode change is seen about 10 minutes later than before (TASK-1867, beta.332).
- **Fix: no `CS=10` without a flow temperature during PWM ON.** When the boiler flow temperature went stale during a PWM ON phase, the 3-second suppression refresh sent the minimum setpoint while heating stayed on; it now leaves the setpoint as it is, as thermo-nova does (TASK-1412.18.1, beta.334).
- **Change: a self-tune hour counts at 90 % clean ticks.** A 60-minute heating-curve self-tune hour counts when at least 108 of the expected 120 ticks are clean (Sergeant D, entries 340 and 343); ticks left out for hot water or sun no longer drop the whole hour, they only stay out of the hour's mean. On five of gruijter's runs this counts 10 hours instead of 8, with the same votes (TASK-1932, ADR-234 Proposed, beta.339).

### OT-Direct (boards without a PIC)

- **Change: master mode passes the thermostat's writes to the boiler.** A thermostat on the thermostat port that writes a value (any Data-ID except 1, 16 and 24, and 8 and 14 while SAT owns the setpoint) has that write sent to the boiler in the next free master slot and gets the boiler's own answer back. Reads stay answered from the boiler cache. The thermostat interface is on by default in master mode (TASK-1812, ADR-241, ADR-243, beta.329).
- **Change: the OpenTherm library carries the boiler line fix itself.** The library is pinned to a fork with the fix for the boiler line falling silent (submitted upstream as Phunkafizer/opentherm_library#1); the gateway's own workaround is gone. The fork also brings four newer upstream changes, among them the idle-level detection of the boiler line without Smart Power (TASK-1980, beta.328).
- **Fix: the flow temperature is read every 10 s again.** Once a minute MsgID 25 was read up to about 19 s apart, because the 10 s reads waited behind the 60 s slow-poll burst (since beta.307). The faster reads now go first; a `PM=25` goes out within about one second also during the burst. This closes the known issue of beta.323 (TASK-1981, beta.325).
- **Fix: a refused `CS=` or `SW=` is dropped, like the PIC.** When the boiler answers the control setpoint (MsgID 1) or the DHW setpoint (MsgID 56) with Data-Invalid or Unknown-DataID, the gateway drops that value: for `CS=` the thermostat's own setpoint goes to the boiler again, for `SW=` the stored value is cleared (TASK-1804, beta.335).
- **Fix: a command that does not fit the full queue gets no success answer.** `MH=`, `MW=`, `M2=`, `RR=`, `RS=` and `PM=` count as dropped, as `CS=` and `SW=` already did (TASK-1804, beta.337).
- **Fix: nothing queued during a simulation reaches the boiler afterwards.** After the SAT or OTGW simulation is turned off, no command queued while it ran goes out, and a cleared value (`CS=0`, `SW=0`, `MM=T`, `TT=0`) no longer follows its old write. Before, the boiler could get a stale control setpoint right after the simulation ended (TASK-1992, beta.338).
- **Fix: more commands answer as the PIC does** (TASK-1804, beta.324): a too short command or one without `=` gets `SE`; `PR=` with more than one letter is `SE`, an unknown letter `BV`, `PR=E` answers `E=-`; after `GW=R` the PIC's greeting line goes to port 25238; `MH=`, `MW=`, `M2=` read their value like the PIC; `VS=` above 100 is refused and `MI=` rounds to 5 ms; the thermostat's MsgID 16 is always acknowledged; a `TT=`/`TC=` override is cleared when the thermostat is lost; `SR=` on a thermostat write answers WRITE-ACK; a Data-Invalid answer resets the unsupported-ID count; a new boiler fault reads the fault code (MsgID 5) at once.

### PIC (OTGW Classic)

- **Fix: the PIC's `Error 01` counter counts again** (MQTT, web UI): the matcher looked for a line ending the serial reader had already removed (TASK-1804, beta.324).

### Capture tool

- **Change: the capture records only this gateway's MQTT topics.** It used to subscribe to everything on the broker (other devices included); it now records the gateway's own topics and its Home Assistant discovery configs. `--mqtt-all-topics` records everything, with a warning (TASK-1988).
- **Change: the upload fits one Discord attachment and always holds the summary and the crash log.** An upload over 24 MB is packed as `.tar.xz` (Windows 10 and later open it with `tar -xf` or 7-Zip); when that is still too big, the essentials go first and the rest into a second part (TASK-1986).
- **Change: a core dump is labelled latest or old** against the reboot log, so an earlier crash's dump is no longer mistaken for the newest one; the graph CSV matches the web UI export again (TASK-1972).

## What changed since beta.294

The 24 builds beta.295 to beta.323 (the numbers beta.302, beta.303, beta.318, beta.321 and beta.322 are not in this release). Most of them bring OT-Direct (boards without a PIC) closer to the PIC: commands now answer and act as the PIC 6.8 does.

### All boards

- **Change: a freshly flashed unit starts with the normal defaults** (MQTT on, the new topic names, the setup wizard) instead of looking like a 1.x upgrade. The filesystem image no longer contains a default `settings.ini`, so the first boot writes the firmware's own defaults. A filesystem update still puts your own settings back (TASK-1920).

### OT-Direct (boards without a PIC)

- **Fix: a command without a value answers like the PIC.** `CS=`, `TT=`, `SW=` and the other commands without a value answer `SE`, and `OT=`, `MM=` and `CL=` without a value clear (`XX: -`); they used to get no answer at all (TASK-1957, beta.296). This also closes the bare `CS=` known issue of beta.294.
- **Fix: `PM=` reads whole tables and strings, Data-IDs 0 to 255.** It took only IDs up to 127 and read only the first entry; now it reads a whole TSP or fault-history table after its size and every character of a string, as the PIC (TASK-1847, beta.297).
- **Fix: the thermostat's own MsgID 99 is handled like the PIC.** While an `MW=`, `MH=`, `M2=` or `HW=P` write is pending, the thermostat's read of MsgID 99 carries that write to the boiler, and a thermostat write of MsgID 99 sets the stored operating modes (TASK-1906, beta.298).
- **Fix: `OT=`, `SW=` and `SH=` reach the boiler also when the thermostat leaves no free slot.** They ride on the thermostat's own reads of MsgID 27, 56 and 57, as on the PIC (TASK-1803, beta.299).
- **Change: the MsgID 100 override flags and `TT=0` behave like the PIC.** During a `TT=` override the thermostat reads the PIC's flags `0x03`, and in master mode `TT=0` puts a `BS=` or MQTT room setpoint back on MsgID 16 instead of clearing it (TASK-1750, TASK-1751, beta.300).
- **Change: `OH=` is on by default, like the PIC.** Every OT-Direct board switches to `OH=1` once at its first boot on this version; an `OH=0` sent afterwards stays (TASK-1961, beta.301).
- **Fix: `PR=M` understands the OT-Direct mode letters.** The answers `S` (standalone) and `L` (loopback) set the gateway mode on and `P` (bypass) off, instead of logging `PR=M unexpected value` (TASK-1951, beta.304).
- **Fix: the scheduler keeps the 100 ms master wait time.** After the end of a conversation the gateway waits the OpenTherm master wait time of 100 ms before its next own frame; after a late boiler answer the next frame used to follow at once (OpenTherm spec v4.2; TASK-1954).
- **Change: a lost thermostat counts as lost after about 3 s instead of 30 s** (gateway mode), so the boiler gets the gateway's own frames within the spec's 5 s (TASK-1802).
- **Removed: the OT-Direct setback fields that no longer did anything.** See BREAKING changes; `SB=` and `FS=` stay, like the PIC (TASK-1950).
- **Fix: the boiler line no longer falls silent for 36 minutes.** When the boiler gave no valid answer for 36 minutes (no boiler connected, boiler off), a timer wrap in the OpenTherm library kept the line "busy": no frames went out, and web UI and REST commands were queued but not run (503 "queue full"). The gateway now keeps that timer current and restarts the line when a transmit stalls; `/api/v2/device/info` reports `otdrequestactive`, `otdlinestalefixes` and `otdlinetxrestarts` (TASK-1966).
- **Fix: `SC=` refuses a time or day the PIC refuses.** Hours above 23 or minutes above 59 answer `OR`, a missing `:` or `/` or extra characters `SE`, a day outside 1-7 `BV`, instead of being stored with the bits masked; `SC=12:30` without a day is no longer accepted (TASK-1908).

### PIC (OTGW Classic)

- **Fix: the PIC reports work during a SAT simulation.** With the simulation on (a bench without a boiler) the `PR=` report commands reach the PIC again, so the PIC settings, the gateway mode and the `PR=E` return-water reading fill; every other PIC command stays held back during the simulation (TASK-1943, beta.306).

### Web UI

- **Fix: a page load no longer gets 503 answers because of closing connections.** A web connection that is closing or still being set up and holds no data no longer counts towards the limit of 4 open connections; open connections still count, so the protection against request storms stays (TASK-1947, beta.305).
- **New: a DHW temperature line in the Monitor graph and its CSV** (MsgID 26), from the moment the page opens. The device history does not hold it, so after a reload the earlier part of the window has no DHW line (TASK-1776).
- **New: 7 days of graph history on boards with PSRAM.** The gateway keeps 7 days instead of 24 h (about 70 KB of PSRAM, RAM only) and the graph gets a 7 days window. `GET /api/v2/graph/history` serves 24 h by default as before; `?hours=N` (1 to 168) asks for another window, and a window of more than 1008 slots is thinned (7 days: 10 minute slots). Not yet verified: a full 7 day fetch, which needs 7 days of uptime (TASK-1963, ADR-245).
- **Fix: Clear for the cooling level sends `CL=-`.** It sent `CL=0`, which holds a cooling level of 0 instead of clearing it (TASK-1958).
- **Fix: the PIC panel shows a pending or clearing setpoint override as text.** The PIC prints `t`/`c` for an override the thermostat has not accepted yet and `n` while a cleared override is released; the panel showed the raw value such as `t20.50` (TASK-1939).
- **Change: the settings help is served gzipped with an ETag** (a reload answers 304), and the built-in replay fixture is stored compressed; the app image is about 22 KB smaller. No change in behaviour (TASK-1955, ADR-247).

### REST API

- **Change: a command SAT refuses gets 409, not 503.** With SAT enabled, a control command such as `CS=40` through `POST /api/v2/otgw/commands` answered 503 "queue full", which a client cannot tell from a full queue that clears. It now answers 409 "Command refused: SAT is in control of the boiler"; 503 stays for a full queue (TASK-1970).
- **New: `otgw/messages` and `otgw/label` say where the served value came from.** A `source` field (none, boiler, gateway, thermostat, summary, replay) next to the value, so a boiler status `data_invalid` next to a value from the PIC's own sensor is no longer confusing (TASK-1965).
- **Fix: `GET /api/v2/sat/weather/needs-setup` gives its own answer.** The route never matched and returned the full weather status (TASK-1969).

### Capture tool

- **New: the capture collects everything a report needs.** `capture-mqtt-debug` now also saves the device state (every read-only diagnostic REST route and the state files, at start and stop, hourly in a long run; secrets redacted, `settings.ini` excluded), the graph history (24 h, and 7 days where the firmware has it) as JSON and as the web UI CSV, and a diagnostic recording the device holds (it never starts or stops one). It only reads, one request at a time, and everything goes into the upload zip. `--skip-device-state`, `--skip-graph-history` and `--skip-diag-recording` turn the parts off (TASK-1964).

### SAT

- **Fix: the end of a simulation run restores the real SAT state.** The PID state and the heating-curve data are reloaded from the filesystem, so SAT does not go on with the values learned on the simulated room (TASK-1952, beta.295).

---

## What changed since beta.250

The builds beta.252 to beta.294 (the numbers beta.251 and beta.287 are not in this release). Each firmware change has host tests on the real code that fail on the old code and pass on the change, and each web UI change a test on the real web UI files. Every alpha build was bench-tested on a gateway before it was published; what was not checked on a bench is named per item.

Thank you to justink0k for GitHub PR #702 (the DHW flow rate unit in Home Assistant).

### Home Assistant and MQTT

- **Fix: the DHW flow rate sensor is created in Home Assistant again.** Home Assistant only accepts the unit "L/min" for a volume flow rate; the gateway sent "l/min", so Home Assistant rejected the sensor. Contributed by justink0k in PR #702 (TASK-1949, beta.257). The OT log, the web UI message table and the docs now write L/min too (beta.269).
- **Fix: MQTT commands keep working after a low-memory connect.** On a board without PSRAM the MQTT library could refuse the command subscription during the busy first seconds after connecting, and every MQTT command stayed dead until the next reconnect. A refused subscription is now retried until it is accepted (TASK-1778, beta.260).
- **Fix: a planned reboot leaves the broker cleanly.** Before a reboot the gateway now publishes its retained "offline" status first and then disconnects, so Home Assistant sees the gateway go offline at once instead of waiting for the broker's timeout (TASK-1780, beta.262).

### PIC (OTGW Classic)

- **New: the PIC temperature sensor readout from 1.x.** A new setting "PIC temperature sensor" (default off). With it on, a PIC Classic in gateway firmware reads the temperature sensor attached to the PIC every 3 minutes and publishes it to MQTT, Home Assistant and REST, as on 1.x. PIC Classic only; OT-Direct has no PIC (TASK-1940, beta.254 to beta.256).

### OT-Direct (boards without a PIC)

- **Change: PIC command replies match the PIC 6.8 text.** The PR= reports (P, R, O, C, B, L) and the SC=, SR= and FT= replies now use the same wording as the PIC firmware 6.8, so tools written for the PIC read OT-Direct the same way (TASK-1848, beta.252; TASK-1849, beta.253).
- **Change: thermostat lost and back are reported like the PIC.** OT-Direct now prints "Thermostat disconnected" and "Thermostat connected" like the PIC 6.8 does, also to port 25238 (TASK-1850, beta.259).
- **Fix: loopback mode no longer counts as your real boiler for SAT.** The synthetic boiler of loopback mode could set SAT's detected manufacturer, heat source and PWM permission. Those now only come from a real boiler (TASK-1857, TASK-1903, beta.270).
- **Fix: a gateway/monitor mode switch keeps your overrides.** Switching between gateway and monitor mode keeps HW, CS, SR and the other command settings, as the PIC does; only the command queue is emptied (TASK-1839, beta.272).
- **Fix: saved command settings survive a reboot without a thermostat.** HW, the unknown-ID list and the ventilation setpoint were lost on a unit without a thermostat after a reboot (TASK-1944, beta.275).
- **Fix: GW= with an invalid value changes nothing.** It answers BV (or SE for a value longer than one character), as the PIC (TASK-1946, beta.276).
- **Fix: faster recovery after a missed boiler answer.** Without a thermostat, the next status message now goes out within about 1 s instead of 5 s (TASK-1802, beta.277; benched at 1.04 to 1.13 s).
- **Fix: no old boiler values while the boiler is gone.** A thermostat behind the gateway in master mode no longer gets stale boiler values, and a mode switch or GW=R forgets old boiler data (TASK-1807, beta.278). The thermostat part is not checked on a bench.
- **Fix: no 16 °C flow setpoint when the thermostat disappears.** The gateway's own messages keep heating off unless CS= is set, as the PIC does (TASK-1806, beta.279). A real thermostat loss is not checked on a bench.
- **Fix: a boiler "data invalid" answer is never stored as a value** (TASK-1925, beta.280). Host tests only.
- **Fix: commands read their numbers exactly like the PIC 6.8.** CS=, C2=, BS=, CL= and RT= now parse a value as the PIC does: CS=49.72 sends 49.72 to the boiler (it used to be a hair lower) and RT=25.4 means 25.40 (it was read as 25.04). A wrong value answers SE or OR, as the PIC (TASK-1859, beta.288).
- **Fix: more PIC-style command checks.** TT=, TC= and SB= refuse a negative value with SE (beta.289). SW= and SH= answer BV outside the boiler's limits (20 to 80 and 10 to 90 until the boiler reports its own), and OT= of 64 or more, or OT=-, clears the outside temperature (beta.290). SB= keeps its 1 to 30 °C range (beta.291). All TASK-1859.
- **Fix: MM=- clears the maximum modulation** instead of setting it to 0 %, as the PIC (TASK-1799, beta.292).
- **Fix: the messages the PIC never gives up on stay on.** MsgIDs 1, 3, 14, 17 and 25 are never switched off after "unknown" answers (TASK-1801, beta.293). Host tests only; seeing it live needs a boiler that answers "unknown".
- **Fix: commands from the web page and the REST API run in order.** They now run on the main loop, so a GW=R followed by a CS= no longer loses the CS=, and a burst too big for the queue gets an HTTP 503 instead of a silent OK (TASK-1761, beta.294).

### SAT

- **Fix: the self-tune help text says how often it can change the curve.** "Heating curve self-tune" said "at most once a day". It decides once a day, but after a change it needs 3 new counted days, so two 0.1 steps are at least 3 counted days apart (a counted day has at least 6 clean heating hours). Help text only; SAT behaves the same (TASK-1792, beta.265).
- **Fix: settings show the real allowed range.** Ten SAT settings advertised a range that the gateway then cut silently (for example a flow offset of 0 was stored as 0.5). The ranges shown in the web UI and in GET /api/v2/settings now equal what is stored (TASK-1941, beta.264). Very small lower limits, such as the SAT thermal coefficient's 0.005, are now shown as they are instead of as 0 (beta.285).
- **Fix: SAT calibration markers no longer touch the filesystem from the web server.** Adding, listing or deleting a marker works on a copy in memory; the gateway writes it to flash in the background (TASK-1911, beta.284).
- **Fix: a simulation replay no longer changes your settings.** A replayed log (simulation) could overwrite your stored SAT target temperature through follow-thermostat, and on a unit without a PIC a replayed PIC banner could store a PIC board mode. Both are now ignored for replayed lines (TASK-1901, TASK-1902, beta.268). Not checked on a bench (host tests only).
- **Change: a heating start in continuous mode uses the PWM start sequence.** While the flame is off SAT asks for the return temperature + 18 °C, holds that for 20 s after the flame comes on, then follows the flow temperature minus 1.0 until 60 s have passed. This is a fail-safe decided by Sergeant D (TASK-1942, beta.273). Checked in the simulation; the case "boiler already burning when SAT is switched on" has host tests only.
- **Change: SAT leaves PWM for continuous mode sooner.** It switches when the flame is on and the request stays above the flow temperature for 60 s, instead of 3 °C below it for 3 minutes, as decided by Sergeant D (TASK-1933, beta.274). Host tests only; the simulation cannot reproduce this case.
- **Fix: a SAT simulation no longer overwrites your saved PID and heating-curve data** (TASK-1900, beta.281, checked on a bench).

### Web UI

- **New: the graph says what its marks mean, and shows a real restart.** The dashed red and green lines in Monitor > Graph are the live connection dropping and coming back (a sleeping tab or a Wi-Fi hiccup does that too). They now have a legend and a tooltip that say so. A real restart of the gateway (its boot counter went up) gets its own solid amber "gateway restarted" mark (TASK-1914, beta.271). The Greek strings are a first translation and need a check by a Greek speaker.
- **Fix: the 24 h graph no longer repeats an old weather value.** After the weather service stopped answering, the graph kept writing its last outside temperature. It now uses the weather value only while it is fresh (younger than 1 hour) and falls back to the boiler's outside temperature (MsgID 27) (TASK-1937, beta.258).
- **Fix: Mission control follows the light and dark theme.** It stayed a black box in the light theme (TASK-1863, beta.267).
- **Fix: on a phone the language button and the Light/Dark switch have the same height** (TASK-1873, beta.266).

### Diagnostics and tools

- **Change: the web connection limit line says why each connection counts.** When the web server refuses a page because too many connections are open, its log line now splits them by state, so a refused load can be explained. The line also says whether the connections it counted were closing or still opening (beta.286). The limit itself is unchanged (TASK-1947, beta.261).
- **Fix: a reset during a save no longer empties the support files.** The thermostat and boiler support files and the boot-detection log are now saved as a new copy that replaces the old file, so a reset in the middle keeps the old file (TASK-1916, beta.282).
- **Fix: GET /api/v2/device/info no longer adds a stray closing bracket** when it is queried very often (TASK-1910, beta.283).
- **New: device/info reports the IPC task stack low points** taken after hardware detection, for the boot crash investigation (TASK-1870.1, beta.263).
- **For developers: build.py writes its own build log** (build/logs/latest.log), the output of the compiler tools included (TASK-1894).

### Older changes

The changes from beta.77 to beta.250 are listed in the [beta.250 release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/releases/RELEASE_NOTES_2.0.0-beta.250.md).

---

## SAT is opt-in

SAT (Smart Autotune Thermostat) is an embedded heating controller. It is **off by default** and it stays off until you switch it on. From beta.390 the web UI no longer calls it experimental; the heat-pump type still is.

- SAT controls your boiler. When you switch it on, the wizard first shows a step "Before you enable SAT". You must tick a box to continue. Cancel leaves SAT off and changes nothing.
- While SAT is on, it owns central heating control: `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=` from MQTT, REST, the boot commands or the serial bridge are refused (TASK-1695; before beta.127 only `CS=` was). `HW=`, `SW=`, `SH=` and every other command still pass.
- Some SAT decisions are still open (see Known issues). SAT behaviour can change between builds.
- If you enable SAT, please share a capture log (see "Capture tool"). SAT is only as good as the field data we get.

Without SAT, the gateway behaves as a transparent OpenTherm gateway, as in 1.x.

---

## Before you flash

**1. Save your settings.** Since beta.238 a reset or panic during a filesystem update no longer resets the settings (TASK-1774), but a power cut during that write still can (see Known issues), so do not skip this step. Download `settings.ini` from the file explorer, or save the output of `GET /api/v2/settings`:

```
curl http://<ip>/api/v2/settings -o settings-backup.json
```

**2. Choose the route that fits your unit.**

- **A new S3 Mini, or a unit on 1.x or on an old partition layout, needs a USB first flash with `flash_otgw`.** For a unit that holds settings the tool saves them, writes the new layout, keeps your WiFi credentials and restores the settings. Windows: `flash_otgw.bat --migrate --host <ip>`. Linux/macOS: `./flash_otgw.sh --migrate --host <ip>`. See the README and the [WiFi firmware update guide](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/guides/WIFI_FIRMWARE_UPDATE.md).
- **A unit that already runs 2.0.0** (beta.1 or later, or a 2.0.0 alpha with the safeboot layout) **updates over WiFi with the flash zip.** Open `/update`, give it the release zip and press **Update**. The page runs the whole sequence: firmware through the safeboot updater, then the filesystem. Settings are kept. Let it finish: this build expects the new filesystem with the gzipped pages. Between the two steps it serves the plain page of the old filesystem.
- Coming from 1.x: this is a different firmware line. Settings and the web UI differ. Read the breaking changes below.

**3. Interrupted filesystem upload.** If the filesystem upload stops part-way, the settings are no longer lost. The gateway wipes the filesystem, writes your settings back from memory and asks you to upload the filesystem again. Since beta.238 the same holds when the unit resets or panics during the write: at the next boot it formats the filesystem and writes the settings, the reboot counter and the reboot log back (TASK-1774). Until you upload the filesystem, existing files such as the reboot log stay readable (TASK-1926) and other pages show the file explorer upload (`device/info` reports `fsupdateincomplete`).

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

### New from beta.374 to beta.382

- **MQTT and REST (SAT self-tune, TASK-1889): field names changed.** The MQTT topics `sat/curve_tune/days` and `sat/curve_tune/clean_hours_today` are replaced by `sat/curve_tune/window_hours`. In the REST SAT status, `curve_tune.window_hours` and `curve_tune.window_hours_needed` replace `days`, `days_needed`, `clean_hours_today`, `clean_hours_needed` and `hours_today`. No Home Assistant entity is affected (these topics have no discovery entity); update scripts or dashboards that read the old names.
- **BLE on boards without PSRAM** (TASK-2025): a stored "on" is switched off at the first start of this build, and switching it on is refused.
- **SAT self-tune:** the saved progress of an older build is dropped once and the tuner starts counting again (TASK-1889).

### New from beta.351 to beta.360

No MQTT topic is renamed or removed. Changes you may notice:

- **SAT (with SAT on): the room settles about 0.1 C higher.** The tolerance band restarts heating as soon as the room is below the target + 0.1 C (was target - 0.1 C), and after a band stop the boiler stays off for at least 10 minutes (TASK-1887).
- **REST:** the diagnostic recording status adds `append_count`, `append_avg_us`, `append_max_us` (TASK-1665).

### New from beta.339 to beta.351

No MQTT topic is renamed or removed. Changes you may notice, all with SAT on:

- **Every heating start now runs in PWM for its first 60 s of flame,** also with SAT set to manual continuous; the usual rules choose PWM or continuous after that (TASK-1942).
- **SAT can now use PWM on boilers it used to keep in continuous mode,** because the boiler's configuration message (OpenTherm message 3) no longer blocks PWM (TASK-2000). On such a boiler the burner pattern changes: shorter, more frequent burns instead of long continuous ones.
- **SAT stops asking for heat at the room target + 0.1 C** (the tolerance band, TASK-1887): CH goes off while PWM keeps running.
- **After a reboot SAT sends nothing for 60 s,** then controls on the heating curve alone until the first room temperature (TASK-1874).
- **New setting "PWM switch margin"** (REST `satpwmswitchmargin`, default 1.5 C); the existing overshoot margin keeps its own value (TASK-1888).
- **REST:** `/api/v2/device/info` adds `appelfsha256` (TASK-1983).

### New from beta.323 to beta.339

No MQTT topic is renamed or removed. Changes you may notice:

- **Home Assistant:** the thermostat card follows SAT while SAT is on: its target sets SAT's target instead of sending `TT=`, and its lowest target is now 5 C. Two new topics, `sat/climate_target` and `sat/climate_current` (TASK-1979). With SAT off the card works as before.
- **OT-Direct master mode:** a thermostat's writes now go to the boiler and the thermostat gets the boiler's answer, instead of a local acknowledgement; the thermostat interface is on by default in master mode (TASK-1812).
- **OT-Direct:** a `CS=` or `SW=` that the boiler refuses is dropped, so the thermostat's own setpoint goes to the boiler again (TASK-1804).
- **REST:** `/api/v2/device/info` no longer reports `otdlinestalefixes` and `otdlinetxrestarts` (nothing left to count after TASK-1980); `otdrequestactive` and `otdcmdringdropped` stay.
- **Capture tool:** it records only this gateway's MQTT topics; use `--mqtt-all-topics` for the old behaviour (TASK-1988). A large upload is a `.tar.xz` file, and a very large one comes in two parts (TASK-1986).

### New from beta.294 to beta.323

No MQTT topic is renamed or removed. Changes you may notice, all on OT-Direct (boards without a PIC):

- `OH=` is switched to 1 once at the first boot on this version, whatever was stored (a marker in `settings.ini`, `OTDohpicdefault`, makes it happen once). Send `OH=0` afterwards if you want it off (TASK-1961).
- A command without a value now gets an answer (`SE`, or the clear of `OT=`, `MM=`, `CL=`); a script that waited for no answer sees one now (TASK-1957).
- During a `TT=` override the thermostat reads MsgID 100 flags `0x03` instead of `0x02` (TASK-1750).
- **REST:** the setback fields that no longer did anything are removed: `setback_active` (device info `otdirect_status`, `state.otd.setback_active`, `otdsetback`), `setback_timeout` (`otdirect_settings`, `settings.otd.setback_timeout`), the `otdsetbacktimeout` setting, the POST form parameter `setbacktimeout`, and their web UI rows. MQTT is unchanged (`otgw-otdirect/setback_active` was no longer published and had no HA entity). `SB=` and `FS=` stay (TASK-1950).
- **REST:** a control command that SAT refuses answers 409 instead of 503 (TASK-1970). A client that retried on 503 now gets a lasting refusal it should not retry.

### New from beta.250 to beta.294

No MQTT topic, setting or REST field is renamed or removed. New: the setting `pictempsensor` (PIC temperature sensor, off by default) with the MQTT topic `otgw-pic/temperature_reading` (TASK-1940), and the IPC task stack low points in `/api/v2/device/info` (TASK-1870.1). Changes you may notice, all on OT-Direct (boards without a PIC):

- Commands parse their value as the PIC 6.8 does (TASK-1859). `CS=`, `C2=`, `BS=` and `CL=` answer `SE` for a negative or malformed value and `OR` above 100 (an empty `CS=` answers `SE` too since beta.296, TASK-1957); `TT=`, `TC=` and `SB=` answer `SE` for a negative value (a negative `TT=` used to set an override); `SW=` and `SH=` answer `BV` outside the boiler's limits; `OT=` clears on a value it cannot parse and on 64 or more; `RT=25.4` now means 25.40. Check a script that sends these commands.
- `MM=` with a value that is not a number clears the maximum modulation instead of setting 0 % (TASK-1799).
- A command sent over REST may now answer HTTP 503 when the command queue is full, instead of a silent OK (TASK-1761).

### New from beta.237 to beta.250

No MQTT topic, setting or REST field is renamed or removed. New fields: `simulation_active` and `simulation_source` in `/api/v2/health` (TASK-1915), and `resetraw`, `resetrawlabel` and `extwdreset` in `device/info` (TASK-1904). Changes you may notice:

- Three resets in a row, with the reset button or by pulling the plug, each within 10 s of the board coming back up, clear the stored WiFi credentials and open the setup portal (TASK-1930). A power supply that drops out three times that quickly does the same.
- The boot line of `reboot_log.txt` ends with the raw reset code (`[rst 0xNN/0xNN]`), a reset by the external 0x26 watchdog has its own line, and a line can be 160 characters instead of 140 (TASK-1904). Check a script that parses the reboot log.
- SAT sends `CS=` on a change and repeats it every 30 s (31 s after the last one), instead of every 5 s (TASK-1924). Expect about two `CS=` per minute in the telnet log and on the PIC instead of twelve. In continuous mode the setpoint follows the flow temperature minus 1.0 °C for 60 s after the burner lights for heating.
- The web UI no longer calls `/api/v2/simulate`; the SIMULATION badge comes from `/api/v2/health` (TASK-1915).
- On OT-Direct (boards without a PIC) in master mode, a thermostat that reads a value the gateway has not received yet gets Data-Invalid instead of Unknown-DataID (TASK-1808).

### New from beta.224 to beta.237

No MQTT topic, setting or REST field is renamed or removed. New: the object `curve_tune` in `/api/v2/sat/status` and, with "Heating curve self-tune" on, 17 flat `sat/curve_tune/*` MQTT topics (TASK-1881; no Home Assistant discovery, not retained). Changes you may notice:

- On OT-Direct (boards without a PIC): a `CS=` or `C2=` override now lasts 63.5 s instead of 60 s (TASK-1834). A `CS=` or `C2=` below 8 C no longer expires (TASK-1917); to end it, send `CS=0` or `C2=0`. In master mode a thermostat's `TSet` above 0 now expires after 63.5 s without a repeat, whatever its value, and a `TSet` of 0 ends the override at once (TASK-1811). The latest write wins, so a thermostat setpoint of 0 also ends a `CS=` that you set by hand. Before, the thermostat's setpoint stayed until a reboot, `CS=0` or a mode change.
- A settings save needs room for two copies of `settings.ini` for a moment. On an almost full filesystem the save now fails and keeps the old settings (TASK-1913). The temporary file `/settings.tmp` exists for a moment during a save and a leftover one is removed at boot.
- SAT accepts the outside temperature that a PIC puts on the bus (DS18B20 or `OT=`) as a source. A unit that showed `outside_temp_missing` with such a sensor can now start SAT (TASK-1896). An `OT=` value that an automation left behind keeps counting until `OT=-`.
- The first Dallas reading after boot is no longer published as 85.0, so the first sensor value arrives one poll later (TASK-1824).

### New from beta.218 to beta.224

No MQTT topic, setting or REST field is renamed or removed. New: the fields `status` and `last_seen_age_s` on `/api/v2/otgw/messages/<id>` and `/api/v2/otgw/label/<label>` (TASK-1871); `flow_temp`, `return_temp`, `flame_on`, `room_temp_age_s`, `outside_temp_age_s`, `pwm_phases` and the `pwm_phases_*` counters in `/api/v2/sat/status` (TASK-1885, TASK-1880); five `sat/pwm_*` MQTT topics with Home Assistant diagnostic sensors (TASK-1880). Changes you may notice:

- On the first boot of this build the gateway forgets which messages your thermostat sends (`/ot-thermo.json`) and learns them again from the live messages, so the thermostat part of the OpenTherm support view starts empty (TASK-1895). The boiler part keeps what it learned.
- Home Assistant discovers five more SAT diagnostic sensors (TASK-1880).
- The SAT control loop chart's flow line is now the measured boiler flow, not the heating curve (TASK-1885).
- The page footer no longer carries the copyright line; it is in Settings > About (TASK-1893).

### New from beta.203 to beta.218

No MQTT topic, setting or REST field is renamed or removed. `device/info` has new fields: `hd_ipc0_stack_min` and `hd_ipc1_stack_min` (TASK-1865), and `otdpiccompat` on OT-Direct (TASK-1860). Changes you may notice:

- Home no longer switches the appliance type (TASK-1837). Set it in Settings > SAT or in the setup wizard.
- On OT-Direct (boards without a PIC): the `PR=A` banner reports version 6.8 and the board name, where it reported 6.6 (TASK-1844); the MQTT `room_temp` command also replaces a thermostat's MsgID 24 write (TASK-1842); `OT=-` clears the outside temperature instead of writing 0 (TASK-1846); a write command sends one WRITE instead of two (TASK-1855).

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

- **Most settings help pop-ups are in English in Dutch, German and Greek** (TASK-2927). The rest of the web UI is translated, and every help text falls back to English, so no help is missing. The translation follows in 2.1.0 (Robert's decision, 2026-11-19).
- **After the gateway reconnects to your WiFi, its fallback access point can stay up for a few minutes, until its timer runs out** (TASK-2933). The gateway works normally on your network meanwhile. A fix is planned for 2.1.0.
- **The SAT rule of beta.391 (one PWM entry per flame, fail-safe 3.0 C) has not run with a real boiler yet** (TASK-2913). It rests on host tests on the real SAT code and a bench regression without a boiler. The field runs on this beta decide whether 2.0.0 stable goes out on Sunday 22 November.
- **The WiFi recovery of beta.385 (the dead-link count kept across a re-association while memory is nearly full) is proven in host tests only** (TASK-2026, ADR-261). The bench could not bring a unit into the needed state (very low memory and a WiFi drop at the same moment). Robert's decision: it ships, as a known limitation; the hardware test moves to 2.1.0.
- **Boards without PSRAM: on beta.374 or older, do not enable BLE.** It could use up the memory and make the gateway unreachable or hang until it was power-cycled. From beta.375 on BLE cannot be switched on without PSRAM (TASK-2025). Why such a gateway hung instead of restarting itself is still open (TASK-2026).
- **The continuous to PWM switch rule of beta.361 (flow at least 2 C above the request for 60 s) has not run with a real boiler** (TASK-2005). A replay of a tester's Vitodens capture through the real code shows the old rule switching 3 times and the new rule not at all.
- **On a bench unit running beta.373, the lowest free memory since boot dropped from 16.9 KB to 6.0 KB within one minute** at about 78 minutes uptime. The cause is not known yet; there was no crash and no restart. It is under investigation (TASK-2019).
- **The tolerance band of beta.359/360 and the PWM-first start of beta.351 have not run with a real boiler yet.** They rest on host tests on the real SAT code and on bench units without a boiler (TASK-1887, TASK-1942). The MsgID 3 change (TASK-2000) is confirmed on a tester's Viessmann Vitodens (beta.350, real boiler). Field reports with a capture are welcome.
- **`SR=` on OT-Direct (TASK-1804 A6) has not been seen with a thermostat;** it rests on a host test.
- **The Home Assistant card of TASK-1979 has no screenshot from a real Home Assistant yet, and the OTGW Classic was not benched with it** (TASK-1979). The MQTT side was checked on the OT-Direct bench unit. With SAT off and no thermostat connected, the card keeps showing SAT's last room temperature until a thermostat reports one.
- **The pressure-drop alarm fix has not run on a unit with a pressure sensor** (TASK-1867). The proof is a replay of gruijter's pressure series through the real code (two false alarms on the old code, none on the fix); the bench unit has no pressure sensor.
- **The first-boot rollback of TASK-1919 has no old-versus-fix bench run yet** (TASK-1919). The fault was found in the boot records of the OT-Direct bench unit; the fix rests on host tests, and the bench run that makes a new image fail its first boot is still open.
- **OT-Direct: going back to a build older than beta.301 and up again sets `OH=` to 1 once more** (TASK-1961, confirmed on the OT-Direct bench unit). The one-time switch to the PIC default runs again after such a round trip; set `OH=0` again if you had chosen that.
- **The thermostat write path of OT-Direct master mode has not been seen with a thermostat** (TASK-1811). The bench unit has no thermostat on the slave side, so the new expiry and the `TSet` of 0 rest on host tests on the real handler and expiry code, and on a bench run that shows the `CS=` timings did not change. If your thermostat in master mode behaves differently than before, please send a capture.
- **OT-Direct: the `CS=` timings are proven in master mode only** (TASK-1834, TASK-1917). The bench ran master mode without a thermostat; the substitution of a real thermostat `TSet` in gateway mode was not exercised there. The timer and release paths are measured (63.53 s, 63.84 s, `CS=5` stays, `CS=0` and `CS=0.001` release); the substitution code is unchanged.
- **A Data-Invalid answer to MsgID 99 on OT-Direct has not been seen** (TASK-1907). The loopback test never reaches that branch; the proof is a host test (old code 2 failures, fix passes). A boiler that answers MsgID 99 with Data-Invalid will show it.
- **A late PIC after a missed boot probe is not bench-tested** (TASK-1790). The ESP cannot hold the PIC silent at boot on the bench, so there is no red reference either; the host test on the real board resolution block is the proof (old code 2 of 5 fail, fix 5 of 5). A bare S3 module with PSRAM and without a PIC is the open residual case. Auto on the OT-Direct bench unit still closes the PIC port, as before.
- **A reset or power cut in the middle of a settings save is proven on a host model only** (TASK-1913). Every bench restart goes through the clean restart path, and a hard reset over USB did not reset the unit. The password guard on `/settings.tmp` is checked statically, not with an admin password set.
- **The first Dallas reading fix has not run on a DS18B20** (TASK-1824). No bench unit has one; the host test uses a sensor model (old code publishes 85.0 on boot, fix 4 of 4). The Dallas labels change (TASK-1912) was bench-tested with a simulated sensor.
- **The Dutch, German and Greek texts of the self-tune progress box are not reviewed twice** (TASK-1881). They were translated by hand against the glossary, without the usual two reviews; a review is open. If a text reads oddly, please tell us.
- **The AsyncTCP repair is proven by a host test** (TASK-1746.1). A connection storm rarely makes an allocation fail inside the connection accept, so the 20 restarts under storm on the bench show no regression but do not show that path being hit.
- **A filesystem update can, rarely, panic on the OTGW Classic** (TASK-1774). On our OTGW Classic bench unit a filesystem update panicked in an internal system task on beta.167 on 10 November, and in 1 of 9 filesystem writes on 12 November (beta.217). Both times the settings went back to their defaults and the unit came back in the safeboot updater; uploading the firmware there again brought it back. The likely cause is too small a stack for that system task. Since beta.219 that stack is larger (TASK-1870): on the same unit the free stack during a filesystem write went from 80-88 to 340 bytes, and 11 updates on beta.219 gave no panic. The cause is not proven, so this stays a known issue until more updates have run. Since beta.238 a panic or reset during the filesystem write no longer resets the settings: the next boot formats the filesystem, writes back the settings, the reboot counter and the reboot log, and asks for the filesystem upload again. A power cut during the write is not covered and can still leave the defaults. **Do:** save your settings before every update (see "Before you flash"), and if the unit comes back in safeboot, upload the firmware there and restore your settings if needed.
- **On one OTGW Classic bench unit the PIC's answers to `PR=` commands are not shown** (TASK-1943). On that unit, on beta.250 and on a later build, `/api/v2/pic/settings` stays empty and no `otgw-pic/settings/*` topic is published, while the PIC version is detected and its OpenTherm traffic is logged. That unit has no boiler connected. Whether this is the firmware or that bench setup is under investigation. If `/api/v2/pic/settings` is empty on your unit, please tell us.
- **The three-reset WiFi recovery has not been done on a bench** (TASK-1930). The bench can only restart over the web or USB, which never count. The counting of reset-button presses and power-ons rests on a host test on the real code. If the gesture does not open the portal on your unit, or the portal opens without it, please tell us.
- **The 60 s flow follow of SAT continuous mode after an ignition has not run on a boiler** (TASK-1924). On the bench the simulated flow stayed below the request in those 60 s, so the follow never came into play; the proof is a host test on the real control code. The new `CS=` timing was not checked during the 2 s start-up of a boiler without a return temperature either.
- **OT-Direct master mode: the Data-Invalid answer to a thermostat has not been seen** (TASK-1808). No bench unit has a thermostat or a boiler; the proof is a host test (old code fails, fix passes).
- **OT-Direct: a Data-Invalid answer from the boiler is kept as if it were a value** (TASK-1925, found during TASK-1808). Master mode then passes those data bytes to the thermostat as a normal answer. Not changed in this build.
- **A reset by the external 0x26 watchdog has not been seen in the new reboot log line** (TASK-1904). No bench unit has the chip; the proof is a host test on the real log code.
- **A firmware image that crashed once in its first 30 s may keep running without confirming itself** (TASK-1919, found during TASK-1774). On the bench such an image booted again after the first panic, and only the second panic rolled the unit back to the safeboot updater; by reading the code, any later reset does the same. If your unit lands in safeboot after an ordinary restart, upload the firmware there again.
- **The graph history kept on the device can hold the last weather value after the weather fetch stops** (TASK-1937). Found by reading the code, not seen in the field. The browser part of this was fixed in beta.247 (TASK-1875).
- **Watchdog restarts in the field may not be gone** (TASK-1865, TASK-1899). Beta.217 removed the filesystem read from the health check that a field crash pointed at, and beta.230 moves the same kind of read out of `/api/v2/simulate` and `device/info`, which the web UI polls (TASK-1899). On the bench, 125 minutes of polling both every 2 s with a filesystem write every minute gave no restart, but that bench never reproduced the field crash, and two of the three restarts of a tester's unit on beta.218 left no crash report, so their cause is not known. The field confirmation by gruijter is open. **Do:** close the gateway tabs you do not use (a hidden tab stops asking), and if your unit restarts unexpectedly, send `GET /api/v2/device/crashlog` and `reboot_log.txt`.

- **The new REST and SAT telemetry fields have not been checked against a real boiler** (TASK-1871, TASK-1880, TASK-1885). The bench has no boiler: the `status` of `/api/v2/otgw/messages/<id>` for an acknowledge, Unknown-DataID or Data-Invalid answer, the flow and flame fields of the SAT status, and the PWM phase record of a phase in which the burner never lights rest on host tests and the SAT simulation. If one of them disagrees with what your boiler does, please send a capture.
- **SAT's own requests for the flow and return temperature on a PIC board have not run on a real boiler with a thermostat** (TASK-1653 `PM=25`, TASK-1759 `PM=28`). They are covered by host tests on the real queue and frame code. The PIC sends such a request only in place of a thermostat request the boiler does not support; if your thermostat only asks supported values, SAT sends nothing and `sat/pm25_no_slot` says so. The REST SAT status counts `pm28_sent`, `pm28_answered` and `pm28_unanswered`.
- **OT-Direct: the boot change (TASK-1835) is not proven at frame level.** The bench unit has no boiler and no bus listener, and telnet starts after that part of the boot. On the bench, gateway mode came up about 1 s sooner than master mode in the same build, which matches the one boot status request it no longer sends; that is an indication only. A boot capture with a thermostat in gateway mode is welcome.
- **OT-Direct: the clock answer to a real thermostat (TASK-1840) has not been seen.** The bench checked that `SC=` puts nothing on the bus; the answer to the thermostat's MsgID 20 rests on host tests.
- **Used SAT in an alpha before 30 October 2026? Check the heating curve slope** (TASK-1787). The hourly heating curve auto-tune of the early 2.0.0 test builds (the `SAT Auto Tune` switch, removed by TASK-1413 on 30 October 2026) could lower the heating curve coefficient. 2.0.0 keeps a stored coefficient as it is, so a unit that ran that auto-tune can still carry a value far below the default of 2.0; one tester's unit was at 1.12. The daily self-tune moves the coefficient only by 0.1 per step, with at least 3 counted days between steps, so it does not correct such a value quickly. **Do:** after the upgrade, open Settings > SAT and check **Heating curve slope** (default 2.0).
- **Several open browser tabs still get a few "busy" answers** (TASK-1741, TASK-1747). On a board without PSRAM, two visible tabs on Monitor > Values got about 0.5 HTTP 503 answers per minute on the bench (0.4 % of requests), down from 15.6. The pages keep showing their last values, so a busy answer only makes a value one poll older. **Do:** close the gateway tabs you do not use; a hidden tab stops polling by itself.
- **The first periodic refresh of a page can come up to 15 s later than before beta.160** (TASK-1741). This is the price of spreading the requests of each tab.
- **A reboot can occasionally take about 25 s** (TASK-1763). In earlier betas a small internal task could run out of stack while the interrupts were set up at boot; the gateway then panicked, restarted by itself and ran normally. Since beta.219 that stack is larger (TASK-1870), with about 250 bytes more free on the bench, and 21 restarts gave no panic. The panic was rare before (1 in 10 reboots at worst), so these counts do not prove it gone. If you see a panic in the crash log after a reboot, please send it.
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

Thank you to everyone who tested the alphas and betas, to gruijter and maarten_41762 for the detailed feedback and the field captures, to thaghostnl and indigo_light for their reports, to tjfs for the OLED, OT-Direct, override, gateway `TT=`, pump symbol, pressure, home dial and update page reports and the captures, to Crash-Evans for pull request #699 and the macOS capture runs, to André Brait for the pull requests (#697 and #698 in beta.118), the code size analysis and the page-load report, and to Sergeant D for the SAT design decisions.
