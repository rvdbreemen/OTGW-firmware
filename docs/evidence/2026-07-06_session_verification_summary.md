# 2026-07-06 session verification summary

Consolidated evidence for four completed items this session that are not
screenshot-able (hardware data / backlog-doc work), per the standing
verification goal. Each item's underlying raw data already lives in the
referenced files/commits; this document is the "or otherwise" evidence
record tying them together.

## 1. TASK-956 — 10-hour ESP32-S3 heap-fragmentation soak

**Device:** esp32-classic bench unit, 192.168.88.64, alpha.331+a82bfda.
**Driver:** `scripts/heap_soak_driver.py`, representative load (API polling
every 20s + occasional asset reload + onboard OTGW serial-simulation replay).
**Raw data:** `build/heap_soak_snapshots_long.ndjson` (1612 lines),
`build/heap_soak_stdout_long.log`, `build/heap_soak_stderr_long.log`.

**Result:** 9.998h wall-clock span, one 127.7-minute gap (external cause —
board physically unplugged for unrelated hardware testing, not a heap fault).
`hd_min_max_block` never dropped below 10,740 bytes across the entire run,
both sides of the gap. `enter_low/warning/critical`, `drip_slowmode`,
`ws_drops`, `mqtt_drops`: all zero for the full 10 hours. One loop-gap spike
(1027ms) coincided with unrelated concurrent PIC-flash testing on the same
unit, not load-induced.

**Verdict recorded:** `backlog/tasks/task-956 - heap-frag-soak-run-analyze-prove-or-disprove-esp32s3-gating-then-phase3-removal.md`
(Implementation Notes, "FULL 10-HOUR VERDICT 2026-07-06").

## 2. TASK-972 — PIC firmware flash investigation

**Device:** same esp32-classic bench unit, live telnet monitoring during 3
real flash attempts (`gateway.hex`, action=upgrade).

**Findings:**
- Compared `otgw-1.x.x` vs `dev` OTGWSerial/upgrade-task code: only one
  platform conditional in the whole library (UART/pin binding), correctly
  parameterized for this board (`PIN_PIC_RX=44`/`PIN_PIC_TX=43` match
  `boards.h`). No functional divergence to port.
- Verified `dev`'s dedicated-FreeRTOS-task park handoff (`waitForPICTaskParked()`)
  is correctly ordered before the flash state machine drives the UART.
- Tested and **refuted** the BLE/core-1-starvation hypothesis: disabled
  `satbleenable`, retried the flash live (user-authorized), got the identical
  failure (`Result code: 9, "Too many retries", 10 retries, 0 errors`).
  Re-enabled `satbleenable` afterward, confirmed restored.

**Full investigation log:** `backlog/tasks/task-972 - v2-parity-PIC-firmware-flash-UI-pic-only-port-the-Classic-hex-upload-flash-flow.md`
(Implementation Notes, 2026-07-06 entries).

**Status:** still open — root cause narrowed to either ESP32 UART1 timing or
a hardware-specific issue on this bench unit, neither confirmed. No further
live flash attempts recommended without a scope/logic-analyzer or a second
PIC-equipped board.

## 3. TASK-1029 — ADR-167 authored (supersedes ADR-089/121)

**File:** `docs/adr/ADR-167-esp32s3-heap-tier-gating-removal-supersedes-adr089-adr121.md`
**Commit:** `a51db2135` (pushed to `origin/dev`).
**Status:** Proposed (not self-accepted — explicitly left to the maintainer).

Cites both TASK-956 soak data points (2026-06-30 extreme-synthetic run,
2026-07-06 10-hour representative run above) as Evidence. Honestly flags
the evidence's limits (10h vs the originally-scoped 24-72h window, missing
MQTT-discovery-republish traffic).

**Verification:** file exists at the path above, Status block confirmed
`Proposed. Date: 2026-07-06`, `docs/adr/README.md` updated with the ADR-167
entry and ADR-089/121 annotated "(Proposed for supersession by ADR-167)".

## 4. TASK-1030 — MIT to GNU GPLv3 relicensing

**Commit:** `d3c6209d8` (pushed to `origin/dev`); SimpleTelnet submodule
commit `123106f` (local only, not pushed — no standing push permission for
that separate repo).

**Verification performed:**
- Root `LICENSE` confirmed to match the official GPLv3 text fetched from
  `https://www.gnu.org/licenses/gpl-3.0.txt` (674 lines, header/footer spot-checked).
- `scripts/relicense_mit_to_gplv3.py` dry-run + applied run: 52 files
  converted in the main pass, correctly SKIPPED `s0PulseCount.ino` (©Rob
  Roos) and `safeTimers.h` (©Willem Aandewiel) via its per-file copyright
  safety check.
- Follow-up sweep found and hand-fixed 5 files the script's regex patterns
  missed due to phrasing variants (`MQTTHaDiscovery.cpp`, `MQTTstuff.h`,
  `mqtt_discovery_verify.{cpp,h}`, `SimpleTelnet/README.md`).
- Repo-wide `grep -rl "MIT License"` re-run after all fixes: zero remaining
  hits outside the deliberately-excluded third-party files.
- Build verified clean post-conversion: `./build.bat --target esp32 --firmware`
  → "Build completed successfully!" (comment-only changes, no compile
  regressions).
- `python evaluate.py --quick`: 68 checks, 0 failed, 1 warning (pre-existing,
  unrelated), Health Score 98.7%.

**Verification log:** `backlog/tasks/task-1030 - docslicense-convert-project-license-from-MIT-to-GPLv3.md`
(final summary).
