# TASK-1475 evidence: one Python capture tool (ADR-202), with TASK-1396 and TASK-1453

Collected 2026-10-29/30 by session otgw-firmware-b1 in worktree `wt-1475`.

- **Bench:** `.63` (OTGW Classic + LOLIN S3, PIC 6.8, firmware `2.0.0-alpha.521+2425108`), read-only (telnet, REST GETs), 23:46-00:00.
- **Old scripts:** `origin/dev:scripts/capture-mqtt-debug.{bat,sh}` at `fc06acb6f`, run from a scratch copy.
- **New tool:** `scripts/otgw_capture.py` behind the launchers.
- **Redaction:** the LAN broker (`192.168.88.32`) was down, so every run used the test suite's minimal MQTT broker on `127.0.0.1:18830`; it publishes one message, `otgw-firmware/value/bench 42`. Full transcripts stay local, because they hold device dumps. The files below are extracts with local paths and the device MAC replaced (`<scratch>`, `%USERPROFILE%`, `<mac>`). They contain no passwords, PSKs or tokens: checked with `grep -i "passw|psk|token|secret"`, 0 hits.

## Bench runs, in order

| # | Run | Command (shortened) | Toggle readout after the run |
|---|---|---|---|
| 0 | readout as found | `toggles.py .63` | all non-simulator toggles on, SensorSim off (`bench-0-initial-toggles.txt`) |
| 1 | set a start state | REST API, MQTT, Sensors, NTP off | `bench-1-set-start-state.txt` |
| 2 | **new tool, Windows** (`capture-mqtt-debug.bat`, which picked `py -3` = Python 3.14.2) | `-DeviceHost 192.168.88.63 -BrokerHost 127.0.0.1 -BrokerPort 18830 -DurationSeconds 60 -SkipBrowserCapture -NoPrompt`, rc 0 | the four are **off again**: restored (`bench-2-after-new-windows.txt`) |
| 3 | **old .bat** | same arguments plus `-SkipToolInstall`, rc 0 | REST API, MQTT, Sensors, NTP **left on** (`bench-3-after-old-bat.txt`) |
| 3b | reset to the start state | | `bench-3b-reset.txt` |
| 4 | **old .sh** (Git Bash, mosquitto_sub on PATH) | `--device 192.168.88.63 --broker 127.0.0.1 --broker-port 18830 --duration 60 --skip-browser-capture --no-prompt`, rc 0 | the four **left on** (`bench-4-after-old-sh.txt`) |
| 4b | reset to the start state | | `bench-4b-reset.txt` |
| 5 | **new tool, Linux** (WSL2 Ubuntu, `capture-mqtt-debug.sh`, Python 3.12.3) | `--device 192.168.88.63 --broker "" --duration 45 --skip-browser-capture` | the four **off again**: restored (`bench-5-after-new-linux.txt`) |
| 6 | old .sh, no broker | `--device 192.168.88.63 --no-prompt` | rc 1, `Error: Broker host is required.` (`bench-6-old-sh-nobroker.txt`) |
| 7 | restore as found | all non-simulator toggles on | `bench-7-final-restore.txt`: identical to readout 0 |

`toggles.py` is a scratch helper. It reads the banner through `otgw_capture.parse_toggles` and sends only the keys of listed, non-simulator toggles.

## Results per acceptance criterion

**TASK-1475 AC#2 (same sources, compatible transcript).**
- `hdr-*.txt` hold the `=== ` section headers of the four transcripts.
- new Windows = new Linux = old .bat: 10 headers each, identical.
- old .sh has 12 headers. It adds two top-level copies of `MQTT STDERR` and `BROWSER STDERR`, which the new tool, like the .bat, keeps only inside `TOOL ERRORS (error.txt)`.
- `mqtt.log` content is the same line in all three runs that had a broker: `otgw-firmware/value/bench 42`.
- `summary-*.txt` are the full SUMMARY sections, for a line-by-line comparison.

**TASK-1475 AC#3 (launchers per OS).**
- **Windows:** run 2 went through the `.bat` launcher.
- **Windows without Python:** with `PATH=C:\Windows\System32;...\WindowsPowerShell\v1.0` and an empty `LOCALAPPDATA`, the `.bat`:
  - downloaded, SHA-256-checked and unpacked the embeddable CPython 3.12.10 in 11 s, rc 0;
  - started in 1.5 s on the second run (cached);
  - refused a tampered hash (`SHA-256 mismatch`, rc 1, nothing unpacked).

  That bootstrapped interpreter then ran `tests/test_capture_tool.py`: 16 ok, 1 skipped.
- **Linux:** run 5.
- **macOS:** not available here, not run.
- **Python versions:** `tests/test_capture_tool.py` passed on Windows 3.9.13 (embeddable), 3.12.9, 3.12.10 (bootstrap) and 3.14.2, and on Linux 3.12.3 (17/17, SIGTERM case included).

**TASK-1475 AC#4, TASK-1396 AC#1 (q/D only after setup).**
- `red-1396.txt`: fake device with uptime 0 and no `Setup finished!`.
- OLD worker, the heredoc extracted from the old .sh: keys `'13qD'`, so q/D are sent during setup (red).
- NEW: keys `'1313'`. No q/D are sent; the toggles go on at the start and back at the stop (green).
- Host test `test_i` covers the release once `Setup finished!` arrives.

**TASK-1396 AC#2 (heap fields in the device/info probe).** `hd_min_free_heap` appears in the HTTP PROBES section of 0 of the 2 old transcripts (the body is cut at 200 characters) and 2 of the 2 new ones. Host test `test_j` checks it too.

**TASK-1453 AC#1 / AC#2 (no broker).**
- Old .sh: run 6 fails with `Broker host is required.`
- Old .bat: with a saved `BrokerHost` in `capture-settings.json` it does not fail at all. It pre-fills the saved broker and starts a capture with no end; it was stopped by hand.
- New tool: run 5 has no broker and logs `MQTT capture skipped: no BrokerHost given (telnet, probe and crash log still run).`; stdin was redirected and no prompt appeared. Host test `test_k` covers this too.

**TASK-1453 AC#4 (toggles restored).**
- Red: runs 3 and 4.
- Green: runs 2 and 5, whose summaries say `Debug toggles restored to their state at capture start: REST API, MQTT, Sensors, NTP`.

**TASK-1453 AC#5 to AC#8.** Host tests on fake devices:
- `test_m`: reconnect without reboot; the restore compares states.
- `test_l`: simulators are never sent (key `8`).
- `test_n`: the device is gone at stop. The tool reports `restore failed (device likely gone)` within 20 s, and exit 0.

The red side is the old scripts on the bench: neither has a restore at all. Host-level red/green comes from `red-1453-restore.txt` (script `red_1453.py`). The extracted old worker and the new tool run against the same fake device:
- old: OTmsg and MQTT are left on (`restored: False`);
- new: `restored: True`;
- neither sends the simulator key.

**TASK-1453 risk: prefill after a run without a broker.** Host test `test_r`:
1. Seed the settings file with broker, user and password.
2. Run with `--broker ""`.
3. Every MQTT value must survive.

It is red on the first implementation, which wrote `BrokerHost: ''`, and green on the fix.

Helper scripts used above: `toggles.py`, `red_1396.py`, `red_1453.py` (they read the old `.sh` from a scratch copy of `origin/dev:scripts/capture-mqtt-debug.sh`).
