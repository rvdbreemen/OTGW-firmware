---
id: "ADR-202"
title: "Replace the two capture scripts with one Python capture tool behind thin launchers"
status: "Proposed"
date: "2026-10-29"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "diagnostic-capture"
  - "host-tooling"
  - "python-bootstrap"
  - "mqtt-client-dependency"
aliases:
  - "capture-mqtt-debug"
  - "otgw_capture.py"
  - "capture script"
  - "paho-mqtt"
  - "embeddable python"
components:
  - "Diagnostic capture tool"
symbols: []
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-202 Replace the two capture scripts with one Python capture tool behind thin launchers

## Status

Proposed, 2026-10-29.

**Decision Maker:** User: Robert van den Breemen (maintainer) chose "Unify on Python" in TASK-1472 on 2026-10-29. The bootstrap and dependency details below are the agent's proposal under TASK-1475 and wait for his review.

## Status History

```yaml
status_history:
  - date: 2026-10-29
    status: Proposed
    changed_by: Agent (Claude, TASK-1475)
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

Testers and the maintainer collect bug evidence with a capture script. It records five sources into one merged transcript:

- the telnet debug stream on port 23;
- MQTT;
- the browser console over the Chrome DevTools Protocol (CDP);
- the crash log and reboot log;
- REST (Representational State Transfer) probes.

The script exists twice. `scripts/capture-mqtt-debug.bat` is a cmd launcher with a 2,570-line embedded PowerShell payload. `scripts/capture-mqtt-debug.sh` is 1,884 lines of bash with embedded Python heredoc workers. The two copies have drifted. Every reliability fix had to be written two or three times: TASK-1299 (telnet keepalive and gap report), TASK-1320, TASK-1396 (q/D sent during setup, probe bodies cut at 200 characters) and TASK-1453 (broker mandatory, debug toggles never restored). The TASK-1472 research found the problem is the duplication, not the language. Robert chose to unify on one Python implementation, local only; cloud upload is parked as TASK-1473.

Constraints that shape the decision:

1. **Delivery.** `build.py:1194-1243` puts the capture launchers into every flash zip, and testers run them from the unzipped folder. A tester on stock Windows 11 has no Python. Typing `python` there either fails or starts the Microsoft Store "App Execution Alias".
2. **MQTT.** Both scripts shell out to `mosquitto_sub`. When it is missing, the `.bat` installs it with winget and the `.sh` points the user at brew or apt. That install step is the most common first-run failure.
3. **Python packaging.** The Windows embeddable CPython has no pip, and its `._pth` file fixes `sys.path`. PEP 668 blocks `pip install` into the system python3 on Debian/Ubuntu and Homebrew. On macOS, `/usr/bin/python3` can be the Xcode Command Line Tools stub.
4. **Standard library.** `telnetlib` was removed in Python 3.13 (PEP 594). The `.sh` telnet worker already uses a raw socket.
5. **Callers.** Scripts and skills call the `.bat` with PowerShell-style arguments (`-DeviceHost`, `-BrokerHost`, `-DurationSeconds`, `-Topic`, `-SaveSecrets`); see `scripts/otgw-test.py:137` and `scripts/loadtest_arm_runner.py:202`. The `.sh` callers use `--device`, `--broker` and `--duration` (`scripts/README.md:89-93`).

## Decision

One standard-library Python program, `scripts/otgw_capture.py`, replaces both payloads. The thin launchers `capture-mqtt-debug.bat`, `capture-mqtt-debug.sh` and `capture-mqtt-debug-macos.sh` find or bootstrap a Python 3.9+ interpreter and run it. MQTT uses paho-mqtt 2.1.0, vendored as its pinned pure-Python wheel and imported straight from the wheel file.

How the pieces fit:

```text
capture-mqtt-debug.bat ---> py -3 | python | python3 (>= 3.9, not the Store alias)
                       \--> else: %LOCALAPPDATA%\OTGW-capture\python-3.12.10\python.exe
                                  (embeddable zip from python.org, SHA-256 checked, no pip)
capture-mqtt-debug.sh  ---> python3 (>= 3.9)
        |
        v
otgw_capture.py  -- sys.path.insert(0, "vendor/paho_mqtt-2.1.0-py3-none-any.whl")  (zipimport)
   threads: telnet | mqtt (paho) | browser CDP | crashlog poll | REST probes
   output : logs/mqtt-diagnostics/<run>/ + one merged transcript, same section headers as today
```

- The tool starts from the existing Python heredoc workers of the `.sh` (telnet, CDP websocket, crashlog, probes, metadata), which `tests/test_capture_telnet_worker.py` already covers. It adds the `.bat`-only features: saved settings (TASK-833), the console stop key and the tool error log.
- The command line accepts both spellings. `-DeviceHost`, `-BrokerHost`, `-DurationSeconds` and the other `.bat` names map to the same options as `--device`, `--broker` and `--duration`.
- Saved settings keep the JSON shape of `capture-settings.json`. On Windows the file stays at `%LOCALAPPDATA%\OTGW-capture\`; on macOS and Linux it lives at `~/.config/otgw-capture/`. `scripts/_secrets.py` keeps reading the Windows path unchanged.
- If the wheel cannot be imported, MQTT capture is skipped with one summary line and every other source keeps running. The same applies when no broker is given (TASK-1453).
- The launcher downloads the embeddable CPython only when no usable interpreter exists. It checks the download against a SHA-256 hash pinned in the launcher before unpacking.

## Decision Contract

### Must

- Keep one implementation of capture behaviour in `scripts/otgw_capture.py`. Launchers only locate or bootstrap Python and forward arguments.
- Keep the merged transcript's section headers compatible with the old scripts, so existing readers and skills keep working.
- Pin every downloaded or vendored artefact by version and SHA-256: the embeddable CPython zip per architecture and the paho-mqtt wheel.
- Degrade per source. A missing broker, wheel, browser or crash-log endpoint skips that source with a summary line; it never aborts the capture.
- Stay local: write only to the local output folder and the saved-settings file. No upload anywhere (TASK-1473 is parked).

### Must Not

- Must not call `pip install` at run time, or require network access, when a Python 3.9+ interpreter is already present.
- Must not use `telnetlib`, or any other module removed in Python 3.13.
- Must not reintroduce `mosquitto_sub`, winget or brew installs into the capture path.
- Must not send uppercase debug keys that are not toggles listed in the banner (some bench builds bind test hang keys).

### Exceptions

- None.

### Verification

- `python tests/test_capture_tool.py` runs the real tool against an in-test fake device (telnet, HTTP and an MQTT stub). It covers the TASK-1299 cases a–h, TASK-1396 and TASK-1453.
- `tests/test_build.py` asserts that the flash zip contains the launchers, `otgw_capture.py` and the vendored wheel.
- Capture runs on Windows 11 without Python on PATH, on macOS and on Linux, recorded in TASK-1475.

## Alternatives Considered

- **Keep both scripts and fix each bug twice.** Rejected. This is the drift that produced TASK-1299, 1320, 1396 and 1453, with each fix written two or three times (TASK-1472 research).
- **Rewrite in JavaScript as a Node, Deno or Bun single binary (TASK-1472 route B).** Rejected. A binary is 62–150 MB per OS and breaks the small flash-zip delivery.
- **Browser-only capture from the device page (TASK-1472 route A).** Rejected for now. The debug text is telnet-only (`networkStuff.ino:33`, 1 client), and background tabs get throttled over long runs.
- **Hand-rolled MQTT 3.1.1 subscriber (about 150 lines, no dependency).** Rejected under the reuse-open-source rule. paho-mqtt already does CONNECT/SUBSCRIBE/PINGREQ, authentication, MQTT 5 and reconnects, and it is actively maintained.
- **Install paho-mqtt with pip into a per-user venv.** Rejected. The embeddable CPython has no pip, PEP 668 blocks system installs, and the step would need network access on every fresh machine.
- **Download the paho wheel at first run instead of vendoring it.** Rejected. It adds a network dependency and a second trust point for a 67 KB file. A vendored wheel also works offline on a LAN-only bench.
- **gmqtt 0.8.0 (MIT, released 2026-10-08) or amqtt 0.12.1 (MIT).** Rejected. gmqtt is asyncio-only, which does not fit the thread-per-source design inherited from the `.sh` workers. amqtt pulls in psutil, pyyaml, websockets and four more packages, which is too heavy to vendor.

Open-source check (2026-10-29, primary sources: the PyPI JSON API and the GitHub API):

| Candidate | Version / date | Licence | Size | Maintenance | Advisories |
|---|---|---|---|---|---|
| paho-mqtt | 2.1.0 / 2024-04-29 | EPL-2.0 OR BSD-3-Clause | 67,219 B wheel, no runtime dependencies | repo pushed 2026-10-24 (PR #961, OpenSSL 4 TLS fix) | none in the GitHub advisory database for pip/paho-mqtt |
| gmqtt | 0.8.0 / 2026-10-08 | MIT | 37,679 B | active | not checked further (rejected on fit) |
| aiomqtt | 2.5.1 / 2026-03-05 | BSD-3-Clause | 16,051 B | active | wraps paho-mqtt, so adds a layer without removing the dependency |
| amqtt | 0.12.1 / 2026-09-16 | MIT | 129,862 B plus 7 dependencies | active | not checked further (rejected on weight) |

The paho-mqtt wheel SHA-256 is `6db9ba9b34ed5bc6b6e3812718c7e06e2fd7444540df2455d2c51bd58808feee`, the same value PyPI publishes. The embeddable CPython 3.12.10 SHA-256 values were computed from python.org downloads. Their MD5 sums match the python.org release API:

- amd64: `4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3`
- arm64: `3065efc3d382d1cda66757ac71ade11904fa6e350f5a97eb74811acd71ba5532`
- win32: `084b9eb24cb848605c895d05b738fbc2572efc8b4c18c415a824065864a2b853`

## Consequences

**Positive:**

- Each capture fix lands once, in one file, with one test suite. About 4,500 lines across two languages become one Python program plus three launchers of a few dozen lines each.
- No mosquitto_sub install step. MQTT works as soon as Python runs.
- A stock Windows 11 machine works after a one-time 11.1 MB download (amd64), cached per user. Machines that already have Python 3.9+ need no download at all.

**Negative:**

- The flash zip grows by two files: `otgw_capture.py` and the 67 KB wheel. The `.bat` is no longer self-contained. Mitigation: `build.py` packs all of them and `tests/test_build.py` fails when one is missing.
- A first run on Windows without Python needs internet access to python.org. Mitigation: the launcher reports this clearly and names the cache folder, and a tester can install Python 3.9+ instead.
- The vendored wheel has to be bumped by hand for security fixes. Mitigation: the version and hash live in one constant, and the advisory check is repeated whenever this ADR is reviewed.
- PowerShell-only behaviour, such as reading DPAPI-protected values, is gone. The saved-settings file was already plain JSON (`capture-mqtt-debug.bat` Save-CaptureSettings), so nothing changes there.

## Open Questions

- [ ] Port to the 1.x line (`otgw-1.x.x`, which has its own capture scripts) or keep this dev-only? The default in TASK-1475 is dev-only.

## Related Decisions

- None. Earlier ADRs cover the firmware build (ADR-014, ADR-083) and the LittleFS staging step (ADR-190). This ADR covers host diagnostic tooling only.

## References

- TASK-1472 (research and Robert's choice), TASK-1475 (implementation), TASK-1396, TASK-1453, TASK-1299, TASK-833, TASK-1078, TASK-1473 (parked cloud upload).
- `build.py:1194-1243` (flash-zip packing of the capture launchers), `build.bat:130-200` (the existing embeddable-CPython bootstrap, which this tool's launcher mirrors).
- `scripts/capture-mqtt-debug.sh:750-1169` (telnet worker), `:1173-1476` (CDP worker), `:1480-1620` (crash-log worker).
- PEP 594 (removal of `telnetlib` in Python 3.13): <https://peps.python.org/pep-0594/>. PEP 668 (externally managed environments): <https://peps.python.org/pep-0668/>.
- paho-mqtt: <https://pypi.org/project/paho-mqtt/2.1.0/>, <https://github.com/eclipse-paho/paho.mqtt.python>.
- Python embeddable package: <https://docs.python.org/3/using/windows.html#the-embeddable-package>.
