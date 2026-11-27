# OTGW-firmware v2.0.0-beta.NEXT Release Notes (draft skeleton)

Not a release yet. Skeleton for the beta after beta.458, built from the lane `lane-poc-b2` (tip 5205df0f5 when otgw-poc-dev sent his lines, base f7b6d0f4b) and three web UI lanes (3bb1245fd, 4cfbfadf7, e11cdbfd2). One line per user-visible change; every line needs its evidence checked again at the release commit. Not pushed. No Sergeant D entry numbers in public text.

## What is new in this beta, in plain words

### SAT and MQTT

- **A thermostat setpoint change that equals a preset selects that preset** (TASK-3032). When the thermostat's own setpoint moves to exactly the value of one of the SAT presets, SAT selects that preset; a value that matches no preset still clears the preset. Host tests on the real code, red on the old code. Not verified: SAT with a real thermostat, and the MQTT half (`sat/target` 18.0, 21.0, 15.0, 16.0 giving eco, comfort, away, sleep), which waits for the B2 bench.
- **Weather location over MQTT** (TASK-3016). Set `sat/weather_lat` (-90 to 90) and `sat/weather_lon` (-180 to 180); a value outside the range, or not one number, is ignored and not clamped. The state is published retained on the same topics with 4 decimals. The REST settings `satweatherlat` and `satweatherlon` are unchanged. Host tests, red on the old code. Not verified on a unit: the broker wire format and the value after a restart.
- **New retained topic `otgw-firmware/thermostat/override_refused`** (TASK-3033). "false" when the thermostat's own setpoint follows a `TT=` or `TC=` override within 0.25 C, "true" when it still differs 300 seconds after the command (some thermostats ignore the override). A command the PIC refused or never accepted is not counted as a refusal, and nothing is published until a verdict exists. Host tests only. Not verified: a real thermostat, and the PIC retry and refusal path on hardware.
- **Zone count defaults to 0 extra rooms, zone topics only for rooms with data** (TASK-3102). New and reset installations start with 0 extra rooms (the old default 1 counted the main room). An installation that already saved the zone count keeps its value, and an existing settings file written before this setting existed keeps 1. The zone topics `sat/zone/<n>/active|output|error` are published only for an extra room that has received data, `sat/zone_count` is retained, and `/api/v2/sat/status` has a `zones` array. SAT control is not changed. Host tests, red on the old code. Not verified: the B2 bench. The question about existing installations is not settled with Sergeant D yet: do not present that case as his decision.

### OTGW32

- **Sensible pin defaults on the OTGW32** (TASK-3120). Dallas sensors start on GPIO 4 (the 1-wire pad; the old default GPIO 10 is a board pin and was refused, so sensors never started). The S0 counter starts on GPIO 5, the DI pin of the terminal block (the old default GPIO 12 is the Ethernet clock). An installation that still holds an old default is moved once; a pin you chose yourself is never changed. The GPIO output has no pad on this board: it is hidden and refused (`has_output_pin` is false in `GET /api/v2/device/info`). The S0 counter does not start on the pin the enabled 1-wire sensors use, and says why. Host tests, red on the old code. **Not verified: counting S0 pulses on the DI pin** (needs an S0 meter or pulse generator on an OTGW32): say "untested", never "works".

### Web UI

- **The File System table fits a 375 px phone screen** (TASK-2926): the Advanced page no longer scrolls sideways. Checked in a real browser layout with synthetic data.
- **Help popover stays inside its card; chart axis label no longer touches the axis** (TASK-3058, TASK-3052). Checked in a real browser layout at 375 px and 1400 px with synthetic screenshots.
- **Clearer English settings help** (TASK-3118): `ethdns` names the router, and the PWM cycle help gives minutes like the dropdowns. Other languages follow the translation sync.
- **The page reconnects politely** (TASK-3124): the web socket retries after 5, 10, 20, 40 and then 60 seconds with some jitter, pauses after three sockets that opened and closed within a minute (a Reconnect button resumes), and no stale "page is reconnecting" hint shows while the page is paused. Tested in headless Chrome; not tried with several real browsers or a sleeping tab.

### Release files

- **No loose firmware files on the release page** (TASK-3116). From this beta the GitHub release has no loose `.ino.bin` or `.littlefs.bin` file, and `SHA256SUMS` lists none: an old 1.x flash tool matches those names and would write a 2.x image to an ESP8266. The app and filesystem images stay inside the `-flash.zip` under their own names. Betas up to 2.0.0-beta.452 still carry the loose files. Tests: the real 1.x `flash_esp.py` (tag v1.7.10) picks both images from the old list and nothing from the new one. Not verified: 1.x `flash_otgw.bat`, a real ESP8266, and the update page taking the zip.

### Manual

- Manual rows for the new MQTT topics, the zone changes and the OTGW32 pins (TASK-3016, TASK-3102, TASK-3120).

## Not verified yet, in plain words

- 3032, 3033, 3102, 3016: host tests only; no thermostat, boiler or broker run.
- 3120: no OTGW32 unit run with the new defaults, and no S0 pulses counted on the DI pin.
- 3124: headless browser only.
- 3116: real 1.x flash tools on Windows and a real ESP8266 not tried.

## Known issues

(carry over from beta.458; add what the next bench shows)

- **New in this beta, observation:** On a unit without extra memory, two web pages open at the same time plus other requests can bring the memory low for a moment; in our test the unit recovered by itself without a restart (one test run; the web UI reconnect backoff in this beta reduces repeated reconnects).

- The host test `tests/test_loadtest_restcap_sweep.py` failed once in a full run under load (passed 2 of 2 alone); a timing test, not a firmware finding. Internal, not for the public notes unless it repeats.
- For the 1.x line, not 2.x: the real 1.x `flash_otgw.sh` in an empty folder exits with code 2 before it downloads anything.
