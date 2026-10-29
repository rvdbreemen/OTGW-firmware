---
id: TASK-1475
title: >-
  Capture tool: one Python implementation replaces capture-mqtt-debug .bat/.sh
  (local first)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T14:05:00Z'
updated_date: '2026-10-29T22:18:00Z'
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
- [ ] #1 ADR (Proposed via adr-kit) for the Python capture tool, its bootstrap and the paho-mqtt dependency
- [ ] #2 One Python implementation collects every source today's scripts collect; transcript format compatible (diff of section headers against a current .bat and .sh capture)
- [ ] #3 Launchers run on stock Windows 11 without a preinstalled Python, and on macOS and Linux; evidence = a capture run on each available OS
- [ ] #4 TASK-1396 and TASK-1453 behaviour fixed in the new tool (tests red on the old scripts' behaviour, green)
- [ ] #5 Old .bat/.sh payloads removed; docs/guides and the flash-zip README updated
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
<!-- SECTION:NOTES:END -->
