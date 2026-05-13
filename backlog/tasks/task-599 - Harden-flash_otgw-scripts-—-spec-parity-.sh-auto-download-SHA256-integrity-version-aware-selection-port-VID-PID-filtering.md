---
id: TASK-599
title: >-
  Harden flash_otgw scripts — spec parity (.sh auto-download), SHA256 integrity,
  version-aware selection, port VID/PID filtering
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-13 17:11'
updated_date: '2026-05-13 17:14'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation Plan

**Branch:** `claude/review-otgw-flash-scripts-XZI4w` (already on it). Tooling-only changes — exempt from version bump (CLAUDE.md §Versioning policy).

### Phase A — Pin esptool SHA256s (AC #2, #3)
Pin per-platform constants near top of both scripts, sourced from `https://github.com/espressif/esptool/releases/tag/v4.8.1` (fetched at impl time, not pre-baked into this plan).
- `.sh`: `ESPTOOL_SHA256_LINUX_AMD64`, `..._LINUX_ARM64`, `..._LINUX_ARM32`, `..._MACOS`.
- `.bat`: `ESPTOOL_SHA256_WIN64`.
Add `verify_sha256` helper in `.sh` (uses `sha256sum` on Linux, `shasum -a 256` on macOS). `.bat` uses `certutil -hashfile`.
Mismatch → red error citing expected vs actual + exit 1.

### Phase B — Spec parity: .sh auto-download (AC #1)
Port `.bat :download_release_binaries` logic into `.sh`:
- New function `download_release_binaries()` callable when `FW_FILE` / `FS_FILE` are empty after local scan.
- Use `curl` (already present in the script flow) with `User-Agent: OTGW-Flash-Tool` and `Accept: application/vnd.github+json` headers.
- Parse `releases/latest` JSON with `grep`+`sed` (no `jq` dependency).
- Filter assets to `\.ino\.bin$` and `\.littlefs\.bin$`. Download each via `browser_download_url`.
- Same error semantics as `.bat`: print actionable hint if 0 matching assets found.

### Phase C — Release SHA256SUMS asset + verification (AC #4)
Extend `.github/workflows/release-assets.yml` (existing — already runs on release publish):
- New step after the "Download release binaries" step: `sha256sum bundle_staging/*.ino.bin bundle_staging/*.littlefs.bin | sed "s| bundle_staging/|  |" > SHA256SUMS`.
- Attach `SHA256SUMS` via the existing `softprops/action-gh-release@v2` step (add to `files:` block).
- Include `SHA256SUMS` in the flash-bundle zip too.
Both scripts after auto-downloading bins:
- Fetch `SHA256SUMS` from the same release (asset filter).
- For each downloaded bin, look up its line by basename, compute its hash, compare.
- Mismatch → exit code **3** (distinct from network failure exit 1 and bad-arg exit 2).
Locally-present (not auto-downloaded) bins are flashed without verification — backwards-compat for offline use; documented in `--help`.

### Phase D — Version-aware file selection + display (AC #5, #6)
- `.sh`: `find_first_match` → `find_highest_version`. `ls "$dir"/OTGW-firmware-*.ino.bin 2>/dev/null | sort -V -r | head -n 1`. `sort -V` is GNU+BSD.
- `.bat`: replace single `for %%F` with a sort by parsed `<X>.<Y>.<Z>` segments. Pure cmd: `for /f` reading a `dir /b` then a sub-routine that picks the max via tokenised numeric compare.
- Both: extract version from chosen filename (regex `OTGW-firmware-(.+?)\.ino\.bin`) and print as `Version: <v>` in the "Ready to flash" summary block.

### Phase E — VID/PID port detection + --list-ports (AC #8, #9, #10)
VID/PID allowlist: CP210x `10c4:ea60`, CH340 `1a86:7523`, FTDI `0403:6001`.
- `.sh` Linux: walk `/sys/class/tty/ttyUSB*/device/../idVendor` and `idProduct` (3 levels up via `realpath`); match against allowlist. Optionally read `product`/`manufacturer` for display.
- `.sh` macOS: `system_profiler SPUSBDataType -json` parsed via `grep`/`sed` for VID/PID + bsd_name (saves us from depending on `ioreg` xpath). Falls back to glob.
- `.bat`: `Get-PnpDevice -Class Ports -PresentOnly` → filter on `InstanceId -match "VID_<H>&PID_<H>"`. Falls back to current `SERIALCOMM` enumeration.
- `--list-ports`: new arg parsed first (before esptool download), prints `<port>  <vid:pid>  <description>` lines, exits 0.

### Phase F — Confirmation prompt + opt-in sudo (AC #7, #14)
- `--yes` / `-y` flag in both scripts. Default: prompt `Continue? [y/N]: ` with 30s timeout (`.sh` `read -t 30 -r ans`, `.bat` `choice /c yn /t 30 /d n`). Anything not `y`/`Y` aborts cleanly.
- `.sh`: `maybe_sudo_relaunch` becomes opt-in via `--sudo`. Default path when device not writable: print "Add yourself to the dialout group: sudo usermod -aG dialout $USER, then log out/in" and exit non-zero. Keep dialout/uucp detection to refine the message.

### Phase G — .bat PowerShell env-var passing + GitHub API fallback (AC #11, #12)
- `.bat`: replace all `%DL_DIR%` interpolation inside the PS heredoc with `$env:OTGW_DL_DIR`. Set `OTGW_DL_DIR` via `set "OTGW_DL_DIR=%~1"` before invoking PowerShell. Manual smoke test path: `C:\tmp\rob's dir` (quote in path).
- Both scripts: on HTTP 403 or 5xx from `api.github.com/...releases/latest`, fall back to fetching `SHA256SUMS` from `https://github.com/<repo>/releases/latest/download/SHA256SUMS` (always at this URL once Phase C ships) and parse the filenames from there.

### Phase H — Help-text alignment + CI sync check (AC #13)
- Rewrite both `--help` blocks with identical section order: NAME / SYNOPSIS / DESCRIPTION / OPTIONS / FIRST-RUN / AFTER FLASHING. Same example lines.
- New file `.github/workflows/flash-scripts-lint.yml`: grep `ESPTOOL_VERSION="..."` (sh) and `ESPTOOL_VERSION=...` (bat) and fail if they differ. Runs on PR + push to `dev`.

### Phase I — Verification (AC #15, #16)
- `python evaluate.py --quick` → must be clean.
- Local unit-style checks: temp dir with mocked binaries `OTGW-firmware-1.5.0.ino.bin` + `OTGW-firmware-1.10.0.ino.bin`, stub `esptool` on PATH, run `flash_otgw.sh --yes --port /dev/null --list-ports`, assert version=1.10.0 picked and `--list-ports` exits 0 without flashing.
- AC #16 (Wemos D1 mini hardware smoke test) — **cannot self-verify** in this environment. Will leave that AC unchecked and flag it in Final Summary; task remains In Progress until field-validated. This is the documented exception case in CLAUDE.md §Autonomous task completion.

### Risks / scope notes
- `.bat` numeric semver sort is ~30 lines of cmd. If it gets too gnarly I will fall back to `sort -V`-equivalent via PowerShell one-liner inside the .bat (cmd calls PS, gets sorted output).
- `--list-ports` description field requires reading `/sys/class/tty/.../product` (Linux) and parsing system_profiler output (macOS). If macOS parsing proves fragile in the smoke test, ship `--list-ports` with VID/PID only on macOS and call it out in the help text.
- Backward compat: removing silent sudo escalation is a behaviour change. Mitigation: clear actionable error message so an existing user knows exactly what to do.
<!-- SECTION:PLAN:END -->
