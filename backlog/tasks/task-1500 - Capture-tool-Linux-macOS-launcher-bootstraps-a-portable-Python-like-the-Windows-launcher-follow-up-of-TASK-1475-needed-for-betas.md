---
id: TASK-1500
title: >-
  Capture tool: Linux/macOS launcher bootstraps a portable Python like the
  Windows launcher (follow-up of TASK-1475, needed for betas)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T05:55:00Z'
updated_date: '2026-10-30T08:54:00Z'
labels:
  - tooling capture
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 656000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-30: on Linux the capture launcher must bootstrap and fetch a portable Python, as capture-mqtt-debug.bat does on Windows (portable Python 3.12.10 from python.org, SHA-256 checked, unpacked into %LOCALAPPDATA%\OTGW-capture). Today scripts/capture-mqtt-debug.sh only looks for python3/python and prints install advice. Needed before betas (Robert: priority right after TASK-1495).

Open-source research (2026-10-30, GitHub API): python-build-standalone (github.com/astral-sh/python-build-standalone): latest release 20261026 (published 2026-10-26, pushed 2026-10-29, ~4.5k stars, MPL-2.0 build scripts, CPython under PSF), install_only tarballs for x86_64/aarch64/armv7(gnueabihf) Linux and x86_64/aarch64 macOS, e.g. cpython-3.12.16+20261026-x86_64-unknown-linux-gnu-install_only.tar.gz, plus SHA256SUMS; the same builds uv/rye/Hatch use. Rejected: niess/python-appimage (GPL-3.0, no latest release, few archs, FUSE), Miniconda as build.sh uses (unpinned latest, >100 MB, Anaconda terms), uv as an extra layer. Choice: python-build-standalone, pinned tag + per-asset SHA-256 in the launcher.

Scope: capture-mqtt-debug.sh (used by capture-mqtt-debug-macos.sh too). Order: python3/python >= 3.9 on PATH first; otherwise download the pinned install_only tarball for uname -s/-m (curl or wget), verify SHA-256 (sha256sum or shasum -a 256), unpack once into \/otgw-capture/python-<ver> (macOS: ~/Library/Application Support/otgw-capture), reuse on later runs. Unknown arch / musl (Alpine) / no curl+wget: clear message with the manual alternative. Keep the launcher POSIX/bash-portable (macOS bash 3.2).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Launcher uses python3/python >= 3.9 when present, otherwise downloads the pinned python-build-standalone install_only tarball for the host (Linux x86_64, aarch64, armv7; macOS x86_64, aarch64), checks its SHA-256 and unpacks it once into a per-user directory; later runs reuse it without network
- [x] #2 A wrong checksum, a failed download, an unsupported arch/libc or missing curl+wget stop with a clear message and leave no half-unpacked runtime behind; tests or scripted runs show each path
- [x] #3 Evidence: a real capture run started from the launcher on a Linux host without python3 on PATH (container or WSL with python hidden) bootstraps and captures; plus a run with system Python. macOS: run if a Mac is available, else stated as not verified
- [x] #4 Docs (docs/guides capture guide, flash-zip README) and ADR-202 note updated; the flash zip ships the new launcher
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Real release zip OTGW-firmware-esp32-combo-2.0.0-alpha.549+6e02216-flash.zip (pushed 6e02216bf, posted msg 1565649919828824198), WSL Ubuntu Info-ZIP: capture-mqtt-debug.sh/-macos.sh/flash_otgw.sh -rwxr-xr-x, otgw_capture.py and vendor wheel -rw-r--r--; launcher with no python on PATH and a fresh XDG_DATA_HOME: download, SHA-256 verified, unpack, tool --help, rc=0, runtime Python 3.12.16. evaluate --quick 79/0/0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
capture-mqtt-debug.sh bootstraps a portable Python like the .bat: PATH python >= 3.9, then a private cached runtime, then a pinned python-build-standalone 20261026 / CPython 3.12.16 install_only tarball (5 SHA-256 pins from SHA256SUMS: Linux x86_64/aarch64/armv7, macOS x86_64/arm64). Security review fix: no /tmp fallback, the cache is used only when every folder is owned by the user and not group/other writable, a non-private parent is refused before download (a bypass test caught a bug in the first check, find -perm -022). Evidence: tests/test_capture_launcher.py 6 red -> 7/7 green; WSL runs a-e (system python, no python download+verify+unpack, cached rerun, corrupted pin refused with no partial dir, no curl/wget refused) plus cache-security tests 1-4; the real alpha.549+6e02216 zip runs end to end on Linux without Python. Not verified: macOS (asked a tester in #alpha-testing, msg 1565649970412126320), aarch64/armv7 downloads, the wget path.
<!-- SECTION:FINAL_SUMMARY:END -->
