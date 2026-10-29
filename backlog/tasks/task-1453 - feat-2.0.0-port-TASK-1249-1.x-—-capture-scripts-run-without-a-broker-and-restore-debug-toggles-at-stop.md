---
id: TASK-1453
title: >-
  feat-2.0.0: port TASK-1249 (1.x) — capture scripts run without a broker and
  restore debug toggles at stop
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T06:25:00Z'
updated_date: '2026-10-29T22:18:00Z'
labels:
  - port-from-1.x
  - tooling
  - capture
  - telnet
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 613000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port two remaining parts of 1.x TASK-1249 (commit dc4e0341c on otgw-1.x.x) to the 2.0.0 capture scripts. The other parts of TASK-1249 (TCP keepalive, telnet gap report, half-open reconnect) came from dev TASK-1299 and are already on dev. Do not port them again.

## Gap 1: the broker is mandatory on dev
1.x behaviour: an empty BrokerHost no longer aborts the run. The telnet, probe and crashlog streams still run, and the summary/status says 'MQTT capture skipped: no BrokerHost given (telnet, probe and crash log still run).' A non-interactive run (stdin redirected) is not prompted for a broker.
- 1.x refs: scripts/capture-mqtt-debug.bat:2250 (prompt only when `-not [Console]::IsInputRedirected`), :2261 (username prompt only when a broker is set); scripts/capture-otgw.sh:377-384, :1527-1528.
- Dev locations to change:
  - origin/dev:scripts/capture-mqtt-debug.bat:2112-2121 (prompt, then `throw "BrokerHost is required."`), :2124 (username prompt), :2159 (Save-CaptureSettings), :2186 (summary line), :2325 (MQTT worker start).
  - origin/dev:scripts/capture-mqtt-debug.sh:1702-1703 (prompt), :1719-1720 (`fail "Broker host is required."`), :1776-1777 (summary), :1830 (MQTT_ARGS / mosquitto_sub start).
- Make an empty broker skip the MQTT worker and the mosquitto_sub resolve/install step. Write one status/summary line saying so, and keep every other stream running.
- Dev TASK-1078 (376a93ecf) already lets a missing mosquitto_sub degrade gracefully. Reuse that skip path where possible.
- An empty broker still gets the interactive prompt when stdin is a terminal and NoPrompt is not set (Enter = skip). It must not throw.

## Gap 2: debug toggles are not restored on dev
Dev enables every non-simulator toggle that shows [0] on each connect. It never switches them back:
- origin/dev:scripts/capture-mqtt-debug.bat:1226-1270 Enable-AllTelnetDebugIfNeeded
- origin/dev:scripts/capture-mqtt-debug.sh:879-903 enable_debug_toggles in the Python telnet worker

On 2.0.0 the toggles are runtime state (origin/dev:src/OTGW-firmware/handleDebug.ino:397-411, state.debug.*), so they stay on until the next reboot. That keeps heavy telnet output running after the capture. Dev TASK-1328 measured those bursts driving the heap watermark down to ~776 B.

1.x mechanism to port (scripts/capture-mqtt-debug.bat:1387-1426 Restore-TelnetDebugToggles, called at :2647; scripts/capture-otgw.sh:736-762 restore_debug_toggles, :802, :1773):
- Record the label -> state map from the FIRST banner that lists toggles.
- At stop (normal end, Ctrl+C/SIGTERM, duration reached), before the telnet socket closes, compare against the current states and send the key only for toggles whose state differs.
- 'Current states' means the states after the script's last actions (or a fresh banner/toggle readout). Comparing states instead of replaying the keys sent covers a reconnect without a reboot, where the banner already shows the switched-on toggles as on.
- Summary lines: 'Debug toggles: as found at capture start, nothing to restore.' / 'Debug toggles restored to their state at capture start: <list>' / 'Debug toggles restore: partial (k of n): ...' / 'Debug toggles restore failed (device likely gone): ...'.
- A failed restore must never change the capture's exit outcome.
- Never touch simulator toggles (SensorSim, OTGW-Sim; origin/dev:scripts/capture-mqtt-debug.bat:114).

On the .sh, the telnet socket belongs to the Python worker. Implement the restore in the worker, triggered by the existing stop file/signal, not in the bash cleanup() at origin/dev:scripts/capture-mqtt-debug.sh:632.

## What NOT to port
- The 1.x -TelnetIdleSeconds 120 s HTTP-gated watchdog. Dev already has the stronger health-probe watchdog (TelnetIdleCheckSeconds, origin/dev:scripts/capture-mqtt-debug.bat:76/:1417/:2460; .sh:1136-1140).
- Keepalive and gap report. Dev has them (TASK-1299).
- The 1.x -QuietDebugToggles/-KeepDebugToggles policy (a separate 1.x feature).
- Anything in capture-otgw.sh, which does not exist on dev.
- The capture-mqtt-debug-macos.sh wrapper only needs to keep working.

## Risks
- The restore runs while the device may be gone. Bound it with a short timeout so stop never hangs.
- Dev TASK-1396 orders the q/D dump after setup. Do not send toggle keys while the q/D dump is in flight.
- Keep saved-settings prefill (dev TASK-833) working with an empty broker. Saving an empty BrokerHost must not break the next run's prefill.

## Evidence that dev lacks it

- PRESENT on dev (dev originated it, TASK-1299): origin/dev:scripts/capture-mqtt-debug.bat:1060-1069 Set-TelnetKeepAlive (SO_KEEPALIVE)
- PRESENT: origin/dev:scripts/capture-mqtt-debug.bat:1393,1405,1407 telnet gap report lines
- PRESENT: origin/dev:scripts/capture-mqtt-debug.bat:76,1417,2460 TelnetIdleCheckSeconds=60 silence watchdog gated by GET /api/v2/health
- PRESENT: origin/dev:scripts/capture-mqtt-debug.sh:26,84-90,928,1136-1140 Python worker keepalive (10/5/3), health-gated idle check, gap report
- PRESENT: origin/dev:backlog/tasks/task-1299 (root cause + fix), task-1308 (port to 1.x = dca99273f)
- GAP broker mandatory: origin/dev:scripts/capture-mqtt-debug.bat:2112-2121 prompts unconditionally and throws 'BrokerHost is required.'
- GAP broker mandatory: origin/dev:scripts/capture-mqtt-debug.sh:1702-1703,1719-1720 fail 'Broker host is required.'
- GAP no toggle restore: origin/dev:scripts/capture-mqtt-debug.bat:1226-1270 Enable-AllTelnetDebugIfNeeded only flips [0]->on; git grep -i 'restore' in both dev capture scripts finds no toggle restore
- GAP no toggle restore: origin/dev:scripts/capture-mqtt-debug.sh:879-903 enable_debug_toggles, cleanup() at :632 does not restore
- Relevance on 2.0.0: toggles are runtime state, so they stay on until a reboot (origin/dev:src/OTGW-firmware/handleDebug.ino:397-411 state.debug.*, debugStuff.h:50)
- Relevance: origin/dev:backlog/tasks/task-1328 shows telnet debug bursts with all toggles on push the 2.0.0 heap watermark down to ~776 B
- Dev has no capture-otgw.sh (git ls-tree origin/dev scripts/); its equivalents are capture-mqtt-debug.sh plus the 8-line capture-mqtt-debug-macos.sh wrapper
- Searched dev backlog for 'without a broker', 'broker optional', 'restore.*toggle': no matching task

## Platform notes

This is host tooling (PowerShell inside the .bat, bash with an embedded Python worker in the .sh), so no ESP8266/ESP32 firmware difference applies. The script layout does differ. Dev has no capture-otgw.sh; the Linux/macOS path is capture-mqtt-debug.sh, with telnet handled in a Python worker (enable_debug_toggles at :879). The restore must therefore go into the Python worker, which owns the socket, and not into the bash cleanup(). Dev banners carry more toggles than 1.x (OTmsg, REST API, MQTT, MQTTGate, Sensors, NTP, SAT, OTDirect, SATBLE) plus simulator toggles (SensorSim, OTGW-Sim) that are never touched (bat:114). The restore must skip simulators and parse the banner dynamically. Dev's watchdog (60 s, health-probe gated) already replaces the 1.x 120 s HTTP-gated watchdog, so do not port the 1.x watchdog. The 1.x -QuietDebugToggles/-KeepDebugToggles policy is a separate 1.x feature and not in scope.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

I could not refute the verdict. Both remaining gaps are real on origin/dev: the broker is still mandatory in both scripts, and neither script restores the debug toggles. The keepalive and gap-report parts already exist on dev because they came from dev TASK-1299. Small caveat: I did not re-open the analyst's handleDebug.ino or TASK-1328 heap figure. They only support the relevance argument, not the gap itself.

None needed. One optional addition: on the .sh, the username prompt at origin/dev:scripts/capture-mqtt-debug.sh:1706-1708 runs whenever USERNAME is empty. Gate it on a non-empty broker, as 1.x does at bat:2261. The .bat already gates its username prompt on whether the parameters were bound (:2124).

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 capture-mqtt-debug.bat and capture-mqtt-debug.sh on dev start and capture telnet, device probe and crashlog with no broker given: the old scripts stop with 'BrokerHost is required.' / 'Broker host is required.' (red); the new scripts run and write one summary/status line saying the MQTT capture was skipped (green). Both runs are recorded as evidence.
- [ ] #2 A non-interactive run (stdin redirected, or --no-prompt on .sh) with no broker is not prompted for a broker or MQTT username and does not hang.
- [ ] #3 With a broker given, MQTT capture behaves as before: same mqtt.log content and same summary lines apart from the new toggle-restore line.
- [ ] #4 After a capture that switched toggles on, every non-simulator toggle is back in the state shown in the first banner of that capture. Shown on the bench (.61 OTGW32 or .63 Classic-S3) by the telnet banner after stop: old script leaves e.g. REST API/MQTT/Sensors on (red), new script restores them (green).
- [ ] #5 The restore also works after a mid-capture reconnect without a reboot. Test with a forced telnet drop, keepalive or watchdog path: the restore compares states and does not replay the keys of the last connect.
- [ ] #6 Simulator toggles (SensorSim, OTGW-Sim) are never sent by the restore.
- [ ] #7 When the device is unreachable at stop, the restore logs 'Debug toggles restore failed (device likely gone)' within a bounded time, and the capture still finishes and merges the transcript with the same exit code as without the restore.
- [ ] #8 tests/test_capture_telnet_worker.py (or a sibling test) runs the real extracted Python worker against the fake device and covers the restore: red on the current worker, green on the fix.
- [ ] #9 The new tool (scripts/otgw_capture.py, TASK-1475) compiles on Python 3.9+, the .sh launcher passes bash -n and shellcheck, the .bat launcher runs on cmd (replaces the PowerShell-parser AC: the payloads are gone, ADR-202)
- [ ] #10 python evaluate.py --quick green (host tooling only: no firmware change, so no firmware build; replaces the esp32/classic/combo build AC, combo-only directive 2026-10-29)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 (b1, TASK-1475 lane): AC#9 and AC#10 rewritten. The old ACs named the PowerShell payload parser check and a three-target firmware build; the payloads are replaced by one Python tool (ADR-202) and this is host tooling, so neither applies. Lead otgw-firmware-da agreed to the rewrite on 2026-10-29. Implementation lives in TASK-1475 (scripts/otgw_capture.py TelnetCapture.restore_debug_toggles, broker optional in main/resolve_inputs).
<!-- SECTION:NOTES:END -->
