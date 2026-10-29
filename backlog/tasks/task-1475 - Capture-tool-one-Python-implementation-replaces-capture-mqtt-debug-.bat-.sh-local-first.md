---
id: TASK-1475
title: >-
  Capture tool: one Python implementation replaces capture-mqtt-debug .bat/.sh
  (local first)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T14:05:00Z'
updated_date: '2026-10-29T23:03:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 635000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29 (TASK-1472, interactive choice 'Unify on Python'): rebuild capture-mqtt-debug.bat (PowerShell payload) and capture-mqtt-debug.sh (bash + python3 workers) as ONE cross-platform Python script, so each fix lands once. Keep the thin .bat/.sh/.command launchers as entrypoints that bootstrap Python where it is missing (Windows: like build.bat's venv/toolchain bootstrap, e.g. the embeddable CPython zip; macOS/Linux: system python3). Same collected sources as today: telnet :23 debug (reconnect, keepalive, toggles, q/D), MQTT (paho-mqtt replaces mosquitto_sub, so no winget/brew install), headless Edge/Chrome CDP devtools, crashlog + reboot_log poll, REST metadata, curl probes; same merged transcript format. Fold in TASK-1396 (q/D during setup, probe bodies cut at 200 chars) and TASK-1453 (broker optional on dev). Local only: no upload anywhere (TASK-1473 parked). Needs an ADR (tooling change, new dependency paho-mqtt) and an open-source check recorded per project rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR (Proposed via adr-kit) for the Python capture tool, its bootstrap and the paho-mqtt dependency
- [x] #2 One Python implementation collects every source today's scripts collect; transcript format compatible (diff of section headers against a current .bat and .sh capture)
- [x] #3 Launchers run on stock Windows 11 without a preinstalled Python, and on macOS and Linux; evidence = a capture run on each available OS
- [x] #4 TASK-1396 and TASK-1453 behaviour fixed in the new tool (tests red on the old scripts' behaviour, green)
- [x] #5 Old .bat/.sh payloads removed; docs/guides and the flash-zip README updated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) ADR-202 Proposed via adr-kit (tool, bootstrap, paho dependency, OSS check). 2) scripts/otgw_capture.py: port the .sh Python workers (telnet, CDP, crashlog, probes, metadata, merge) to threads in one process; add .bat-only features (saved settings TASK-833/879, stop key, tool error log, console echo); paho-mqtt from a vendored wheel replaces mosquitto_sub. 3) Fold in TASK-1396 (q/D after Setup finished!, full device/info body) and TASK-1453 (broker optional, toggle restore by state comparison). 4) Thin launchers (.bat finds py/python or bootstraps the SHA-256-checked embeddable CPython 3.12.10; .sh runs python3); build.py packs otgw_capture.py + vendor wheel; test_build red/green. 5) Port the heredoc tests to tests/test_capture_tool.py against the real tool; run on Windows and Linux (WSL). 6) Baseline captures with the old .bat/.sh on .61 (scratch copies of origin/dev versions) and the new tool, diff section headers (AC#2); capture runs per OS (AC#3).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 b1 progress:
- Parity table .bat vs .sh (Explore agent, 29 divergences) decided: B's 7 transcript sections (the 9 .sh sections minus the duplicate stderr sections, which stay inside error.txt); redaction adopted from .sh; MQTT missing/unreachable degrades (TASK-1078) and paho reconnects instead of ending the capture; settings file kept (TASK-833/879), POSIX path ~/.config/otgw-capture, OTGW_CAPTURE_SETTINGS honoured; Chromium quiet flags from .bat; browser discovery = union of both lists plus chromium-browser/google-chrome-stable; debug port checked by binding (works everywhere); error.txt removed after merge (its content is in the transcript).
- Behaviour change on purpose: an exception after the telnet link is up (banner/toggles/dump) now counts as a read failure, not a failed connect (both old scripts mislabelled it); found by the ported test a.
- Tests: tests/test_capture_tool.py 17 cases. Windows 11 / Python 3.12.9: 16 ok + 1 skipped (SIGTERM is POSIX). WSL Ubuntu / Python 3.12.3: 17/17 ok. One earlier full-suite Windows run had case h fail once (gap length bound under CPU load); 8 solo reruns and the next full run were green. tests/test_build.py: red on old build.py (KeyError: no item named 'otgw_capture.py'), green on new (13/13).
- Z/Y hang keys on the .61 exploratory build (wt-1479 handleDebug.ino:484-488) are not toggles; toggle keys are 1-7,g,n,d,s/S,b, so neither old nor new tool can send them.
- Not yet done: bench baselines (AC#2), per-OS capture runs (AC#3), stock Windows without Python bootstrap run, shellcheck.

2026-10-30 bench (.63, read-only) + evidence in docs/evidence/task1475/README.md.
- Side finding (old .bat, now replaced): with a saved BrokerHost in capture-settings.json and stdin redirected, the old .bat does not stop with 'BrokerHost is required.'; it pre-fills the saved broker and starts a capture with no end (observed 23:55, stopped by hand). The new tool never prompts with stdin redirected and treats an empty broker as 'skip MQTT'.
- AC#3 scope: Windows (launcher run on bench; no-Python bootstrap with stripped PATH + empty LOCALAPPDATA, tamper refusal) and Linux (WSL2 bench run). macOS not available on this machine: not run.
- AC#5: payloads removed (launchers only); flash-zip README (build.py EN/NL) and scripts/README.md updated. docs/guides has no page about the capture scripts, so nothing to change there.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
One Python capture tool, scripts/otgw_capture.py, replaces the PowerShell payload of capture-mqtt-debug.bat and the bash + heredoc workers of capture-mqtt-debug.sh (ADR-202, Proposed). Launchers only find or bootstrap Python 3.9+ (Windows: SHA-256-checked embeddable CPython 3.12.10 in %LOCALAPPDATA%\OTGW-capture, plain .NET calls so it also works from a pwsh 7 terminal). MQTT uses vendored paho-mqtt 2.1.0 via zipimport, no mosquitto_sub. TASK-1396 and TASK-1453 folded in. build.py ships otgw_capture.py + vendor/ in the flash zip.
Evidence per AC (docs/evidence/task1475/README.md):
- AC#1: docs/adr/ADR-202-...md Proposed via adr-kit, adr-lint 0 FAIL; OSS table (paho-mqtt, gmqtt, aiomqtt, amqtt) with PyPI/GitHub data and hashes.
- AC#2: bench .63 section headers: new (Windows, Linux) identical to old .bat (10); old .sh has 2 extra duplicate stderr sections; mqtt.log identical in all broker runs.
- AC#3: Windows launcher bench run (picked py -3 = 3.14.2); no-Python bootstrap 11 s / cached 1.5 s / tampered hash refused; bootstrapped 3.12.10 runs the suite; Linux WSL2 bench run. macOS not available, not run.
- AC#4: red-1396.txt (old worker sends q/D during setup, new does not); toggle restore red on old .bat/.sh, green on new Windows/Linux; no-broker red on old .sh, green on new; tests/test_capture_tool.py 17 cases (Windows 16 ok + 1 POSIX-only skip on 3.9.13/3.12/3.14; Linux 17/17).
- AC#5: payloads gone, launchers only; tests/test_build.py red on old build.py (KeyError otgw_capture.py) and green (13/13); README EN/NL + scripts/README.md updated.
Also: python evaluate.py --quick 79 passed, 0 failed; shellcheck 0.11.0 and bash -n clean on both .sh files.
Not verified: macOS; a physically clean Windows 11 install (simulated with stripped PATH). Open question in ADR-202 for Robert: port to otgw-1.x.x or dev-only.
<!-- SECTION:FINAL_SUMMARY:END -->
