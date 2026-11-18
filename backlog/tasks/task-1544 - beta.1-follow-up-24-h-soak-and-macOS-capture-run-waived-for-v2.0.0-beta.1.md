---
id: TASK-1544
title: 'beta.1 follow-up: 24 h soak and macOS capture run (waived for v2.0.0-beta.1)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T15:47:00Z'
updated_date: '2026-11-18T09:21:00Z'
labels:
  - beta
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 700000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1504 AC2 (macOS capture run by a tester) and AC3 (24 h soak on Classic + S3 Mini) were waived by Robert for v2.0.0-beta.1 (2026-11-01). They are still needed before the next beta or stable. Longest run so far: 224 min (alpha.577, OTGW-direct board); .63 Classic ran alpha.597 6 h 24 min without reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 24 h soak of a beta build on the Classic + S3 Mini without an unexplained reboot; crashlog and reboot_log quoted
- [ ] #2 Capture run from the release zip on macOS by a named tester, with transcript
- [x] #3 Capture runs from the beta zip on Windows and Linux, each with a transcript (TASK-1504 AC2 asked for all three OSes)
- [x] #4 Update over WiFi with the release zip through the /update page (the path the release notes give users), then device/info shows the new version and settings are kept
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: verification only (24 h soak + macOS capture). Release gate for the next beta.

2026-11-04 Robert: keep as the 2.0.0 stable-release gate (24 h soak + macOS/Linux capture runs); no bench soak now.

2026-11-06 00:33: soak .63 at 9 h 31 min, boot 148 (no restart), freeheap drifting ~500 B/h (54,176 -> 51,572 B since 19:13); filed TASK-1669.

2026-11-06 15:00 .63 (Classic + S3 Mini, beta.95+1fbd5cf): uptime 1(d)-00:03, bootcount 148 (unchanged since the soak start), mqttconnected true, crashlog available:false, reboot_log newest entries '2026-11-05 14:56:34 - reboot cause: Software reset (3)' and '2026-11-05 14:56:01 - ... [OTA] Rebooting...' (the beta.95 flash), nothing after: 24 h without a reboot.

2026-11-11 (comms): capture-tool volunteer call (one volunteer per OS, beta.158 zip, 10-15 min run) posted in #alpha-testing on 2026-11-09 06:28 UTC, msg 1569237211789000751. No direct reply or volunteer to that post as of 2026-11-11 (checked the 68 later #alpha-testing messages via the bot REST API: no message references it, no reactions). Capture uploads that did arrive afterwards, not as replies to the call: tjfs 2026-11-09 (acknowledged in msg 1569333602137935873), thaghostnl 2026-11-10 two uploads on beta.170 (TASK-1786), gruijter 2026-11-10 20261110-100556-upload.zip on beta.167 (TASK-1787). The OS of those runs was not checked here, so this note does not close the per-OS check; macOS still has only the earlier run (#beta-testing 2026-11-08).

2026-11-12 (comms): long-run volunteer cmdr_langers (Discord 695051073886355508), #english-support https://discord.com/channels/812969634638725140/931267109726593116/1570569541678010461: Nodoshop OTGW, PIC 6.8, S3, OLED; Ideal Logic 35 boiler; Wiser HubR 1st Gen thermostat; MQTT to HA via a separate broker; one week on 2.x without issues seen (Nightly Restart on). Asked (msg 1570571315977527407) to update to beta.218, switch Nightly Restart off, run capture --long-run for a day or more and post the zip, and send crashlog + reboot_log.txt on an unexpected restart.

2026-11-16 11:43-11:49 AC3 Windows half, bench 09: the published v2.0.0-beta.339 zip (OTGW-firmware-esp32-combo-2.0.0-beta.339+bd7a133-flash.zip, the release build soaked on .63 this morning) unpacked into a fresh folder and its launcher capture-mqtt-debug.bat run on Windows 11 the way a user runs it (bundled otgw_capture.py; args --device 192.168.88.63 --broker 192.168.88.32 --duration 300 --no-prompt --skip-browser-capture; browser part skipped for low laptop memory) against .63 (Classic, running beta.347). Result: stop reason 'duration elapsed', telnet connected with keepalive, 0 telnet gaps, debug toggles found on and left as found, 799 MQTT messages, 301 OT frames (otframes.log + otgw_simulation.log), crash-log capture normal, post-capture HTTP probes, coredump fetched and labelled OLD (app sha cd720dc0b), device state and 24 h graph history at start and stop, one merged transcript and one upload zip. Transcript: 09 scratchpad t1544/runs/20261116-114354/transcript-20261116-114354-2.0.0-beta.347+2cb8d28-...txt and runs/20261116-114354-upload.zip (local, not for posting: contains the unit's settings). Linux half not run yet: WSL needs 1-2 GB and the laptop was at 2-4 GB free with background processes being reaped. AC4 not run: .63 is on beta.347 as the 20:00 soak candidate (coordinator: skip unless .63 is on 339). AC2 still needs a named macOS tester.

2026-11-17 AC3 (95): capture runs with the tools from the PUBLISHED v2.0.0-beta.360 release zip (OTGW-firmware-esp32-combo-2.0.0-beta.360+9665467-flash.zip, downloaded with gh, sha256 6f1b6543a904424590b46230ed8b083a17448790b7d68941f9471e7acb57b259 = SHA256SUMS), read-only against .63 (Classic + S3 Mini, PIC 6.8; the unit ran 2.0.0-beta.365+cc60bfd at the time, 09's 2007 rerun, not 360), broker 192.168.88.32:1883, --duration 300 --skip-browser-capture. WINDOWS: capture-mqtt-debug.bat (launcher found Python 3.14.2 on PATH), 10:31:32-10:36:39: exit 0, stop reason duration elapsed, telnet gaps 0, MQTT 581 messages (merged into the transcript, 'MQTT BROKER STREAM' section), 302 OT frames, device state 36 answered / 5 not on this firmware (404) / 3 failed, core dump fetched (31268 B, an OLD dump of an earlier build, labelled so), upload zip 74,960 B. LINUX: capture-mqtt-debug.sh in WSL Ubuntu (Python 3.12.3, no paho in site-packages: the vendored paho wheel was used), 10:37:40-10:42:42: exit 0, duration elapsed, telnet gaps 0, MQTT 588 messages, 298 frames, same device-state counts, upload zip 74,767 B. Evidence (transcripts in the upload zips + launcher consoles + SHA256SUMS): RvdB/OTGW-field-data/bench-63/20261117-task1544/. Bugs/gaps: none blocking. One cosmetic finding: the 3 'failed' device-state reads are /api/v2/otdirect/status, /settings and /overrides, which answer 503 'No OT-Direct hardware' on a PIC board by design; the summary counts them as failed rather than not applicable (could be counted with the 404s). Not covered: WSL is Linux but not a native Linux desktop (network through WSL NAT); macOS stays AC2 (tester).

Status 2026-11-17 (95, ALL STOP): AC3 done and pushed (42e26c101). AC4 request (351 -> 360 via the /update page on .63, settings diff) is with 09 via the coordinator; AC2 macOS draft is with 92. Open follow-up NOT yet filed (task create timed out at the stop): the capture summary counts the three /api/v2/otdirect/* 503 'No OT-Direct hardware' answers on a PIC board as 'failed' instead of not applicable (otgw_capture.py device-state counting); coordinator asked for a small tools-only task with a red/green test.

AC4 bench STATUS by 09 (2026-11-17 ~10:50, stopped at the coordinator's ALL STOP, to be continued). Pre-state is done: .63 (Classic + ESP32-S3, PIC 6.8) OTA'd to the public release 2.0.0-beta.351+3f58242 (ino 0593e580..., the release image) at 10:48:06, 'changed: none'. settings-before.json, S-before.ini and device/info saved (fwversion 2.0.0-beta.351+3f58242, bootcount 199); satsimulation false. Release asset OTGW-firmware-esp32-combo-2.0.0-beta.360+9665467-flash.zip downloaded with gh from release v2.0.0-beta.360: sha256 6f1b6543a904424590b46230ed8b083a17448790b7d68941f9471e7acb57b259 = the expected value. NOT done yet: the browser upload through http://192.168.88.63/update with screenshots, because the Claude-in-Chrome extension was not connected (tabs_context: 'Browser extension is not connected'). Next step: upload the zip through /update in a browser (extension or Playwright), screenshots before/during/after, then compare fwversion 2.0.0-beta.360+9665467, bootcount and settings-after against settings-before. .63 is left stable on 351. Evidence: 09 scratchpad t1544ac4/ (settings-before.json, S-before.ini, info-before.txt, rel/*.zip, flash-to-351.log).

AC4 PASS by 09 (2026-11-18), .63 (Classic + ESP32-S3, PIC 6.8). Pre-state: public release 2.0.0-beta.351+3f58242, bootcount 199, settings-before.json/S-before.ini saved 2026-11-17 10:48. Release asset OTGW-firmware-esp32-combo-2.0.0-beta.360+9665467-flash.zip from release v2.0.0-beta.360 (gh), sha256 6f1b6543a904424590b46230ed8b083a17448790b7d68941f9471e7acb57b259 = expected. Update the way a user does it: real Chrome (headless, driven over CDP because the browser MCPs were not connected; 09 tool otaredirect.mjs), the real http://<ip>/update page, the zip put in its one-file field (#oneFiles), Update clicked 07:16:34. The page's own JS unpacked the zip, went through safeboot and the filesystem step, and at t+93 s navigated to /?_ota=...; at t+96 s the web UI was back by itself, with no further clicks. device/info after: fwversion 2.0.0-beta.360+9665467, bootcount 201 (+2 = the app and filesystem steps), MQTT connected. Settings: 152 keys before and after, none added or removed, 1 value changed: SATthermalcoeff 0.0420 -> 0.0646 (settings.ini: the same single line). That change is NOT from the update: no commit between 3f58242 and 9665467 touches the thermal coefficient (git log -G), and its only runtime writer is the thermal learner (SATcontrol.ino:7371, an hourly save while the flame is off with valid room + outdoor temperatures). .63 has a BLE room sensor and weather, and ran about 20 h on 351 between the pre-state snapshot and the upload; on 360 the uptime was 6 min, under the first hourly save. This cause is inferred from the code, without a log line; a snapshot taken right before the click would have shown it directly. Screenshots: 4 (before, update x2, web UI after) in 09 scratchpad t1544ac4/run/ (redirect.log, shot-01..04). Chrome processes after: 0. Not covered: a desktop Chrome with a human hand; the same JS in headless Chrome.

2026-11-18 AC2 check (otgw-docs), NOT ticked: indigo_light's macOS capture 20261118-081844-upload-clean.zip (752,500 B, #beta-testing msg 1572521874263515248; facts from 92's read of summary.txt and the transcript): Platform darwin, Python 3.9.6 (/Library/Developer/CommandLineTools/usr/bin/python3), --long-run, device beta.360+9665467, 08:18:44-08:53:21 +01:00 (35 min), stop 'Q key', Finished line present, crash-log/frame recorder/browser capture completed normally, telnet gaps 0, MQTT 3525 messages, OT frames 6492. Two gaps against the AC: (1) the tool ran from a beta.350 zip folder, not the release zip the AC names; (2) script.error.log shows the graph-history CSV failing on Python 3.9 (TypeError write_text newline), fixed in TASK-2016 (tools, not yet in a release zip). macOS version asked (msg 1572527731558580235), not answered yet. Next: once TASK-2016 is in a published beta zip, ask indigo_light once for a 10 min rerun from that zip; tick AC2 on that transcript with the macOS version.

2026-11-18 (from 92): indigo_light's Mac is Apple silicon M4 Pro, system Python 3.9.6 (CommandLineTools); macOS version asked again, not answered yet. 92's behaviour analysis of that run (#beta-testing 1572541155206500372, chart in 92's scratchpad indigomac/charts/indigo-mac-sat.png): the tool ran clean on macOS except the 3.9 CSV bug (TASK-2016, on origin/dev, in the beta.369 alpha zip); heating side one 24.8 min run, PWM/continuous/PWM, CS 43-46 C, flow within about 1 C, room 20.07 -> 20.74 C. AC2 stays open until a rerun from a published release zip with the fix.
<!-- SECTION:NOTES:END -->
