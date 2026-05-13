---
id: TASK-599
title: >-
  Harden flash_otgw scripts — spec parity (.sh auto-download), SHA256 integrity,
  version-aware selection, port VID/PID filtering
status: To Do
assignee: []
created_date: '2026-05-13 17:11'
labels:
  - flash
  - tooling
  - security
dependencies: []
references:
  - flash_otgw.sh
  - flash_otgw.bat
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
`flash_otgw.sh` and `flash_otgw.bat` are the zero-install flash tools shipped with each OTGW-firmware release. Design constraints (re-affirmed 2026-05-13): **one file per OS** (`.bat` for Windows, `.sh` for macOS/Linux), **no Python dependency**, **auto-download missing release binaries** from GitHub.

A multi-perspective review on branch `claude/review-otgw-flash-scripts-XZI4w` identified three classes of issue that this task addresses:

**1. Spec-parity gap (.sh).** `flash_otgw.bat:104` auto-downloads release binaries from GitHub when missing in the script directory; `flash_otgw.sh:207-221` errors out instead. Violates the documented contract on macOS/Linux.

**2. Supply-chain integrity gap.** Neither script verifies SHA256 of the downloaded esptool zip (`.sh:165-176`, `.bat:53-87`), nor of the auto-downloaded `.ino.bin` / `.littlefs.bin` assets. Both are executed/flashed with HTTPS-chain trust only. Auto-download is the documented happy path on both platforms, so the surface is large.

**3. Correctness — version-aware file selection.** `flash_otgw.sh:195` (`sort | head -n 1`) and `flash_otgw.bat:93-98` (first `for %%F` match) select the lexicographically first match. With `OTGW-firmware-1.10.0.ino.bin` and `OTGW-firmware-1.5.0.ino.bin` side-by-side, `1.10.0` sorts before `1.5.0` → wrong file flashed (silent version downgrade). Likelier now that auto-download leaves yesterday's manual download alongside today's auto-fetched binary.

**Secondary items rolled in (single coherent PR):**
- Port detection picks first port (`.sh:251`, `.bat:166`) — should filter by USB VID/PID (CP210x `10c4:ea60`, CH340 `1a86:7523`, FTDI `0403:6001`) to avoid grabbing Bluetooth / motherboard serial.
- `--erase-all` is unconditional (`.sh:280`, `.bat:193`) — add `[y/N]` confirmation + `--yes` opt-out.
- No `--list-ports` flag for "which port is my device on" debugging.
- `.bat:228` PowerShell heredoc string-interpolates `%DL_DIR%` into the PS source — parse-fragile for paths containing `'` or `$`; pass via env var instead.
- GitHub API rate-limit fallback to the static `releases/latest/download/<asset>` redirect.
- Resolved firmware version not shown in pre-flash summary.

## Why one task (not split)
All changes live in two files and are coupled by shared arg/help surfaces. Splitting yields four sequential PRs that all re-touch the same blocks. The AC list is granular enough to review item-by-item.

## Out of scope (separate tasks if needed)
- Sharing code between `.sh` and `.bat` via a sibling file or generated source (excluded by design constraint).
- Replacing the pair with a Python tool (excluded by no-Python constraint).
- `flash_esp.py` changes (parallel path for Python-having users; unaffected here).
- Flash-size detection (`esptool flash_id` pre-flight).
- `--keep-config` / `--firmware-only` / `--fs-only` flags.
- ADR for the integrity-verification policy (file separately if reviewers want a durable record of the SHA256SUMS contract).

## Test surface
No automated tests for the shell scripts today (`test_flash_automation.py` covers the Python wrapper only). Verification path: `python evaluate.py --quick`, manual Wemos D1 mini smoke test, and targeted shell-level checks (mocked binaries in a temp dir, stubbed `esptool` on PATH, asserted exit codes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 .sh auto-downloads OTGW-firmware-*.ino.bin and OTGW-firmware*.littlefs.bin from the GitHub releases/latest endpoint when not present in the script directory, then proceeds to flash. Behaviour mirrors existing .bat :download_release_binaries (User-Agent header, asset filtering, error message).
- [ ] #2 Both scripts compute SHA256 of the downloaded esptool zip and compare against a pinned constant (ESPTOOL_SHA256_<platform>) before extracting. Mismatch aborts with a clear error citing expected vs actual hash.
- [ ] #3 Pinned esptool SHA256 constants match the upstream esptool ${ESPTOOL_VERSION} release artefacts on github.com/espressif/esptool; values are documented inline with a comment pointing to the upstream release page.
- [ ] #4 Release workflow publishes a SHA256SUMS asset alongside .ino.bin and .littlefs.bin in each GitHub release. Both scripts fetch SHA256SUMS, verify each release binary's hash, and abort on mismatch with an exit code distinct from network failure.
- [ ] #5 When multiple OTGW-firmware-*.ino.bin (or *.littlefs.bin) files exist in the script dir, both scripts select the highest semantic version (1.10.0 over 1.5.0), not the lexicographically first. .sh uses 'sort -V'; .bat parses the version segment and numerically compares. Test: place both versions in a temp dir, assert 1.10.0 is picked.
- [ ] #6 Both scripts display the resolved firmware version string in the 'Ready to flash' summary, parsed from the filename OTGW-firmware-<version>.ino.bin.
- [ ] #7 Both scripts prompt '[y/N]' confirmation before invoking esptool. Bypassed with --yes (or -y). Default (no input, EOF, or 30s timeout) aborts safely without flashing.
- [ ] #8 Both scripts accept --list-ports: prints each detected serial port (one per line) with VID/PID and product description when available, then exits 0 without flashing. --list-ports does not trigger esptool download.
- [ ] #9 .sh serial-port auto-detection filters by USB VID/PID via /sys/class/tty/*/device/{idVendor,idProduct} on Linux and 'ioreg -p IOUSB -l' on macOS, preferring CP210x (10c4:ea60), CH340 (1a86:7523), FTDI (0403:6001). Falls back to the current glob behaviour when no VID/PID match is found.
- [ ] #10 .bat serial-port auto-detection uses 'Get-PnpDevice -Class Ports' filtered by the same VID/PID list. Falls back to the current HKLM\HARDWARE\DEVICEMAP\SERIALCOMM enumeration if no VID/PID match.
- [ ] #11 .bat PowerShell download payload no longer string-substitutes %DL_DIR% into the heredoc body. The download directory is passed via the OTGW_DL_DIR environment variable and read inside PS as $env:OTGW_DL_DIR. Verified by running with a script path that contains a single-quote character.
- [ ] #12 Both scripts fall back from https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases/latest to the static redirect https://github.com/rvdbreemen/OTGW-firmware/releases/latest/download/<asset> on HTTP 403 (rate-limit) or 5xx; a single [WARN] line is printed before fallback.
- [ ] #13 Both scripts' --help output documents the new flags (--yes, --list-ports) and the integrity-verification behaviour. Help-text structure (section order, option order, example invocations) is aligned between .sh and .bat. A grep-based CI check fails the build if ESPTOOL_VERSION differs between the two files.
- [ ] #14 .sh Linux sudo auto-escalation becomes opt-in via --sudo flag. Default when port is not writable: print actionable instructions ('sudo usermod -aG dialout $USER' then log out/in) and exit non-zero. No silent privilege escalation.
- [ ] #15 python evaluate.py --quick shows no new failures attributable to this change.
- [ ] #16 Manual smoke test on a Wemos D1 mini in an empty directory: fresh USB plug → run .sh and .bat separately → each auto-downloads bins + esptool, verifies hashes, detects port via VID/PID, shows summary with resolved firmware version, prompts y/N, flashes on y, device boots and serves OTGW-<MAC> AP. Output recorded in Implementation Notes.
<!-- AC:END -->
