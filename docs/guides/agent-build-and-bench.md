# Agent guide: build, flash and bench

Split out of CLAUDE.md by TASK-1631. CLAUDE.md keeps the hard rules in short form; read this guide before you build, flash or run a bench or soak test.

## Build commands

**Builds on this machine (maintainer directive, 2026-10-29): one build at a time across ALL agents and sessions, never two in parallel, also not in different worktrees; at most 4 compiler jobs (`build.py` caps `pio_jobs()` at 4, TASK-1456); on `dev` build `esp32-combo` only (the plain wrapper), no separate esp32 / esp32-classic builds, also not for `HAS_*`/platform changes.** A session announces its build to the coordinating 2.0.0 session before it starts and when it ends. **Enforced (TASK-1488):** `build.py` takes a machine-wide lock (`%LOCALAPPDATA%/OTGW-build/build.lock`, shared by every worktree and session); a second build waits until the first ends (it prints the holder), a dead holder's lock is taken over, and `OTGW_BUILD_LOCK_WAIT=<minutes>` bounds the wait (0 = refuse, exit 3). A worktree only gets the lock once its `build.py` contains it: rebase onto origin/dev before building. Direct `pio run` bypasses the lock and is not allowed for that reason.

**Per-task cadence (maintainer directive, 2026-10-30): every finished firmware task is bench-tested before its zip is published.** Per task: host tests red on the old code and green on the fix, `python evaluate.py --quick`, its own prerelease bump (the pre-commit hook enforces one tag per firmware commit, so testers keep their A/B signal) and a commit. Then the developer builds esp32-combo from that HEAD. The bench-tester agent (`otgw-bench-tester`, maintainer directive 2026-11-11) flashes that build on a bench unit, checks that the device reports the githash, bench-tests that task's behaviour with the tests it chooses, records the evidence and ticks the bench ACs. The developer requests the run through the coordinator (githash, absolute artifact path, behaviour to check, and for a bug the old-code reproduction), never flashes itself, and does not rebuild in that worktree until the bench-tester confirms the flash. Only after the bench-tester's passing report: push, `discord-post-build.sh`, the #alpha-testing note and the ledger rows. A failed bench test publishes nothing: fix it, build again and request a new bench run. Tasks that are ready at the same moment may share one build, but each task's behaviour is bench-tested before the zip is posted, and the post's `-m` names every covered tag. At most 3 hours between two builds while work lands. A rebase before the push means a rebuild from the rebased HEAD, because the post script refuses a zip whose githash is not HEAD; the bench-tester then flashes the rebuilt zip and checks its githash and health before it is posted. Docs-only commits may still push at any time, except while a build for a push is running. Independent tasks may be implemented in parallel in their own worktrees (agents edit and test, the lead merges, bumps and commits one at a time).

The alpha-tag publishing protocol lives in `.claude/rules/discord.md` (always loaded).

**ALWAYS use the `.bat`/`.sh` wrappers — they are the only sanctioned CI/agent entrypoints.** On Windows use `build.bat`; on macOS/Linux use `./build.sh`. The wrapper bootstraps a correct Python venv **and downloads the full PlatformIO toolchain** if missing, so it works on any host. **NEVER invoke `python build.py` or `python flash_esp.py` directly** (maintainer directive, 2026-07-05): the host default Python is often 3.14, which the espressif32 platform rejects (needs 3.10–3.13), and `build.py` masks that failure with a lying exit 0 — leaving a stale `littlefs.bin` while claiming success. `build.bat`/`build.sh` are the entrypoints; `flash_otgw.bat` (not `flash_esp.py`) is the flash entrypoint. The `python build.py ...` forms below document what the wrapper runs underneath — invoke them directly only if the wrapper itself is genuinely unavailable, and then verify the output SUCCESS line + `firmware.bin`/`littlefs.bin` freshness by hand.

```bash
./build.sh                   # Preferred — firmware + filesystem (handles venv)
python build.py              # Build firmware + filesystem, esp32-combo only (default)
python build.py --target all             # Not used on dev: combo only (maintainer 2026-10-29)
python build.py --firmware   # Firmware only (also the push-policy gate)
python build.py --clean      # Clean build
python evaluate.py           # Code quality check (PROGMEM, unsafe patterns)
python evaluate.py --quick   # Fast check
```

Build targets on `dev` (`build.py --target` choices; ESP8266 exists only on `otgw-1.x.x`):

| Target | Hardware | Asset name token |
|---|---|---|
| `esp32` | OTGW32 / OT-Thing PCB (OTDirect) | `esp32-otgw32` |
| `esp32-classic` | OTGW Classic + LOLIN S3 Mini (PIC) | `esp32-classic` |
| `esp32-combo` | One image for both boards, PIC or OTDirect picked at boot (ADR-127); the default for S3 builds and what the bench runs | `esp32-combo` |

Asset naming: `OTGW-firmware-<token>-<semver>+<githash>-flash.zip` (plus
`.ino.bin`, `.littlefs.bin`, merged bins and `.elf`). Never run two builds
concurrently in one worktree — shared `.pio/build/` corrupts (bug-034 class);
wipe the affected `.pio/build/<env>` dir and rebuild solo to recover.

## Test automation (on-device / soak)

End-to-end loop for firmware changes that need on-device validation (e.g. the
heap-frag soak, TASK-934):

- **Build (canonical):** `build.bat --target esp32` / `./build.sh --target esp32`
  (a bare wrapper builds esp32-combo only) — builds firmware + filesystem and
  bootstraps its own venv, so it is immune to the host Python version.
- **Build (direct pio) gotcha:** `python -m platformio run -e esp32` fails if the
  host Python is 3.14+ (the espressif32 platform requires 3.10–3.13: *"Python
  version must be between 3.10 and 3.13"*). Invoke pio via its own penv instead:
  `~/.platformio/penv/Scripts/python.exe -m platformio run -e esp32`
  (firmware) and `... -e esp32 -t buildfs` (LittleFS).
- **Fresh worktree:** run `git submodule update --init --recursive` before the
  first build — git worktrees do NOT populate submodule working trees, so
  `SimpleTelnet` (AsyncSimpleTelnet.h) and `OpenTherm` are missing otherwise and
  the sketch fails with *"No such file or directory"*.
- **Static gates:** `python evaluate.py` (full) / `--quick`. Exit 0 = pass, 1 =
  FAIL, 2 = WARN>5. NOT run by build.py — run it separately. Gate unit tests:
  `python tests/test_evaluate.py` (stdlib unittest, no pytest).
- **Full test sweep:** `python scripts/run_tests.py` runs every `tests/test_*.py` AND every
  `test/host/test_*.py` (the MSVC harnesses that compile sliced firmware sources; Windows only,
  `--no-host` leaves them out). Run it before a lane commit; a `test/host` harness that stops
  compiling after a firmware change is a stale stub, fix it in the same lane
  (`docs/guides/test-runner.md`).
- **Only the bench-tester flashes** (maintainer directive, 2026-11-11): `otgw-bench-tester` manages every
  hardware unit on the laptop, does every USB and OTA flash and chooses the bench tests. Other agents
  request a run through the coordinator. The rules below are the bench-tester's working rules.
- **Flash over WiFi OTA first** (maintainer directive, 2026-10-24): the app through
  `POST /safeboot` and the safeboot updater's `/update` upload, the filesystem through
  `POST /update?cmd=100&size=<n>` on the main firmware. The filesystem OTA keeps
  `settings.ini` and the other kept files by itself, and it is the route users take.
  Flash app and filesystem together when the web assets changed, so the hashes match.
- **Firmware OTA facts (TASK-1819, checked 2026-11-11 on .61 and .63):**
  - Every 4MB unit we ship has ONE OTA app slot. The old table has `app0` only. The ADR-191
    table has `safeboot` (factory, 0x10000) plus `app0` (ota_0, 0x100000). The running firmware
    therefore never writes a firmware image itself. IDF `esp_ota_get_next_update_partition()`
    returns the running slot when it is the only one (`esp_ota_ops.c`, `default_ota`).
    The TASK-959 guard in `_handleUploadStart` refuses the image before `Update.begin()`.
    That guard is the safety boundary: keep it.
  - `GET /api/v2/device/info` field `firmware_ota` names the path:
    - `safeboot`: firmware OTA works over WiFi through the updater. Both bench units report this.
    - `usb`: old table; migrate once with `flash_otgw.bat --migrate --host <ip>`.
    - `direct`: a spare OTA slot; no 4MB table has one.
  - `app_ota_available=false` does NOT mean "no firmware OTA". It only means the running
    firmware takes no image itself. Read `firmware_ota`, or `safeboot_available`.
  - Agent route for app + filesystem in one run:
    `python scripts/ota_flash.py --host <ip> --app <...ino.bin> --fs <...littlefs.bin>`.
    It saves the settings, does `POST /safeboot`, uploads the app to the updater and the
    filesystem to the main firmware, reports the new running version and exits 2 when the settings changed.
  - User route: the `/update` page, release zip, one action (same endpoints).
  - Never upload a merged image over WiFi.
- **USB only when OTA cannot work** (unit does not boot, no network, `firmware_ota` is `usb`), and
  then always `flash_otgw.bat --board esp32 --migrate --host <ip>`, which saves and
  restores `settings.ini` (auto-detects the S3 over USB VID/PID 303A:1001; `--port COMx`
  to force; esptool-only, works on any Python). Never `--update --fs` over USB.
- **Settings live on LittleFS:** any flash that writes a filesystem image (`--fs`,
  or bare `--update`, which auto-picks app + fs) resets every setting (mode reverts
  to `gateway`). `--update --app <bin>` writes the app only and keeps them. Save
  `curl http://<ip>/api/v2/settings` first when a filesystem write is involved.
  Check the downloaded size of a static file such as `/settings.ini` before
  trusting it: the device can serve it truncated.
- **Never `sed -i` a `.bat`:** it strips the CRLF line endings and cmd then fails
  with `'M' is not recognized`. Edit with the Edit tool and check the endings.
- **Load (fragmenting):** `python scripts/sat_boiler_emulator.py --host <ip>` for
  synthetic OT traffic; combine with concurrent Web UI polling + MQTT discovery
  republish to exercise heap pressure.
- **Capture / soak:** `scripts/capture-mqtt-debug.bat -DeviceHost <ip> -BrokerHost
  <ip> -DurationSeconds <N> -Topic "otgw-firmware/stats/#"` → one merged
  transcript (telnet debug + MQTT stream + browser devtools + crashlog + HTTP
  probes).
- **Heap soak (TASK-934):** watch `otgw-firmware/stats/min_max_block`,
  `/maxblock_lt2k…ge16k`, `/max_loop_gap_ms` and the gating counters
  (`/drip_slowmode`, `/mqtt_drops`, `/ws_drops`, `/enter_*`). Telnet `z` zeroes the
  watermark/histogram/counters for a fresh window (`min_free_heap` is the native
  ESP32 allocator watermark and is not resettable). Press `z` from a healthy heap:
  if reset mid-pressure, the tier-entry counters (`enter_*`) only resume counting
  after the heap recovers to HEALTHY and re-enters the tier (they share
  `getHeapHealth()`'s internal state, which the reset deliberately does not touch).
