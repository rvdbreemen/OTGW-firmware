# SAT field runs for 2.0.0 stable (Wed 18 to Fri 20 Nov 2026)

Internal recipe for the testers' field runs that gate the 2.0.0 stable release (TASK-1740, target Sat 21 Nov).
The public post is the short English version at the end. Every log line below was found by grep in the source
on 2026-11-16 (origin/dev 17f1a7d79); every Sergeant D entry was opened that day and checked for a later entry
that changes it.

## 1. Before the run

1. **Firmware: the newest public beta.** Tonight that is 2.0.0-beta.351; from Tue 17 Nov 08:00 the next public
   beta (it carries entry 398, see item 5 below). Check it on the device: `GET http://<ip>/api/v2/device/info`,
   field `fwversion` (restAPI.ino:3238, the full version string such as `2.0.0-beta.351+3f58242`), or the version
   on the web UI. Quote the version in the upload message.
2. **SAT on, simulation OFF.** SAT is enabled; the SAT simulation is off. Check `GET /api/v2/sat/status`, field
   `simulation` (SATcontrol.ino:4973, `settings.sat.bSimulation`), which must read `false`. Quote that line in the
   upload message. A capture with the simulation on shows the simulator, not the boiler (TASK-2000 and TASK-1874,
   2026-11-16, were both misread that way).
3. **Capture tool:** `capture-mqtt-debug.bat` (Windows), `capture-mqtt-debug.sh` (Linux) or
   `capture-mqtt-debug-macos.sh` (macOS), all in the release zip next to `otgw_capture.py`. Run it as a long run
   (`--long-run`, or any `--duration` above 2 hours: one segment per wall-clock hour, packed into
   `hours/YYYYMMDD-HH.zip`, otgw_capture.py:13-14). Started without options (a double-click on Windows), the tool
   asks "How long?": choose `2) 24 hours` (TASK-2965). Keep it running for at least one full heating day (24 h)
   including a night and a morning start.
4. **Write down** the boiler model, radiators or floor heating, thermostat or room sensor used by SAT, and any
   moment you changed a setting or the target (time and what).

## 2. What we want to see (and where it shows)

All lines below appear in the telnet log that the capture records. "Entry" is the numbered Sergeant D decision in
`docs/sat/decisions/sergeant-d-corrections.md`.

| # | What | Decided in | How it shows in the capture |
|---|---|---|---|
| 1 | Every heating start runs in PWM first, also with SAT set to manual continuous; the start phase ends 60 s after the flame lights; only then do the normal rules choose PWM or continuous | entries 385, 389, 391, 394 (387 gives the reasons) | When SAT was in continuous before the start: `SAT: Control mode: PWM (heating start)` (SATcontrol.ino:4027, `satPwmFirstStart()`); a start that finds SAT already in PWM prints no such line. Then `Flame lit: flow ..., setpoint ...` (SATcycles.ino:1052). What we check: no `Control mode: continuous` line within 60 s of that `Flame lit:` line |
| 2 | After a reboot SAT sends no command for 60 s; after that it steers on the heating curve only until the first room temperature | entry 388, completed by 395 (the gateway's own MsgID 3/15/48 reads may run during the 60 s) | `SATBOOT phase=silence_end ms=...` (SATcontrol.ino:4407) about 60 s after boot, then `SATBOOT phase=decide ...` (:4443). Do one planned reboot during the run if you can |
| 3 | Tolerance band: at target + 0.1 C SAT sends CH off while PWM stays active; a continuous burn runs at least 180 s first; with zones, eco lets the main room decide, comfort keeps heating while any room is cold | entries 384, 376, 378 | `Heating paused (CH off, PWM stays active): the rooms are at least ... above their target` (SATcontrol.ino:8257), `Heating pause waits for the 180 s minimum burn` (:8261), `Heating resumes: a room is more than ... below its target` (:8256) |
| 4 | When heating resumes after a band stop | entry 399 questions the reading of entry 328; Q-399a is open | Same lines as item 3. Note the room temperature and target at the resume line; do not judge it yet, Sergeant D still has to confirm the threshold |
| 5 | Starts per hour: during a band pause a PWM ON phase does not count as a start toward the hourly limit | entry 398 (not in beta.351; in the next public beta) | Each start is a `Flame lit:` line and each stop a `Flame off: cycle ..., ...s on, peak flow ...` line (SATcycles.ino:1052, :1171); count them per hour |
| 6 | Mode ping-pong: PWM and continuous switching back and forth while the flame burns without a break (seen on a Vitodens, TASK-2005) | known issue under analysis | `Control mode: PWM (flow ... stayed above setpoint ... + ... for ... s)` (SATcycles.ino:1419) followed by `SAT: flame on and request ... above flow ... for ...s, switching to continuous mode` (SATcycles.ino:1460), repeating every few minutes |
| 7 | Hot-water burns counted as heating cycles (seen on a Vitodens, TASK-1998) | known issue under analysis | `PWM ON held: hot-water flame still burning` (SATcontrol.ino:1545), `PWM ON resumed after hot water` (:1516), `Hot water done: heating control resumes` (:8026); note when you drew hot water |

## 3. Upload

Stop the capture with `q`, then attach the `...-upload.zip` it writes (otgw_capture.py:147) to a GitHub issue, or post it in
#english-support or #nederlandse-ondersteuning (beta testers: #beta-testing), or send it by DM to Robert, with: firmware version, the `simulation: false` line, boiler model, heating type, and the notes
from step 1.4. Raw captures stay private; we only post summaries.

## 4. Public version (for #alpha-testing, posted by the comms session)

> **2.0.0 field runs, Wed to Fri:** please run the newest public beta for at least 24 h with SAT on and the SAT
> simulation off, and send us the capture.
> 1. Check the version: `http://<ip>/api/v2/device/info` -> `fwversion`.
> 2. Check that `http://<ip>/api/v2/sat/status` shows `"simulation": false`, and paste that line with your upload.
> 3. Start the capture tool from the release zip (`capture-mqtt-debug.bat`, `.sh` or `-macos.sh`) as a long run,
>    one night and one morning at least. One planned reboot during the run helps.
> 4. Note your boiler model, radiators or floor heating, and any setting you changed (with the time).
> 5. Stop with `q` and post the `-upload.zip` here or by DM.
> We look at the start sequence (PWM first), the pause at target + 0.1 C and the restart, starts per hour, and
> mode switches while the flame keeps burning. Thanks for testing!
