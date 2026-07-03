# BLE / web-stack memory — implementation proposal (TASK-994)

Date: 2026-07-03. Decision requested from maintainer (Robert). One yes/no per option.
Companion: `2026-07-03-ble-memory-hypotheses.md` (8 hypotheses) + 5-agent web research +
3-judge adversarial review (opus) + on-device measurements on the bench S3 (COM8, .88.64).

## The finding that changes everything

The bench S3-Mini **physically has 2MB PSRAM, but it is DORMANT**: the qio_qspi
prebuilt sdkconfig sets `CONFIG_SPIRAM=1` yet `CONFIG_SPIRAM_BOOT_INIT` is OFF and
platformio.ini defines no `BOARD_HAS_PSRAM`, so `psramInit()` is never called →
`psramFound()==0`, the 2MB unused. All the TASK-989 web-collapse pain traces to
running WiFi + async web + MQTT + BLE on ~90K of internal DRAM only.

### Measured (device/info probe, bench S3, esp32-combo)

| Build | BLE | psram | internal_free | internal_maxblk | web ramp (conc) | recovery |
|---|---|---|---|---|---|---|
| current (no PSRAM) | off | — | 82.8K | 40.9K | clean → 12 | instant |
| current (no PSRAM) | **on** | — | 18.8K | **9.7K** | **wedge @ 4** | reboot-only |
| **+BOARD_HAS_PSRAM** | off | 2MB | 45.1K | 31.7K | clean | instant |
| **+BOARD_HAS_PSRAM** | **on** | 2MB | **44.9K** | **31.7K** | **clean → 16** | **instant** |

With PSRAM active, BLE-on costs ~0 internal (host pools + WiFi/LWIP buffers land in
PSRAM); the web stack behaves BLE-on exactly like BLE-off. BLE functional (4 sensors,
live temps) throughout. **This is the fix for PSRAM-equipped boards.**

## Options

### OPTION A (RECOMMENDED) — Enable PSRAM on the S3 envs; keep BLE default as-is
`board_build.arduino.memory_type = qio_qspi` + `-DBOARD_HAS_PSRAM` in **esp32-classic
AND esp32-combo** (build_flags do NOT inherit via `extends` from env:esp32 — the judges
verified this; must be set per-env). Leave **esp32 (OTGW32) untouched** — no PSRAM there.

Pros:
- Solves the web-collapse root cause on the boards that have PSRAM (S3-Mini carriers).
  Ramp clean to conc=16 with BLE on, instant recovery (measured).
- Zero BLE behaviour change, zero app-code risk, zero controller-lifecycle risk.
- Unlocks the 2MB that is currently wasted; also gives headroom for future features.
Cons / risks (all from the field-risk judge, must be gated before field OTA):
- **Cache-coherency during flash writes**: with WiFi/LWIP buffers in PSRAM, the S3 cache
  is disabled during any flash write (OTA, LittleFS asset write, settings save, NVS
  commit). A non-IRAM-safe ISR (AsyncTCP/NimBLE) touching a PSRAM buffer in that window
  panics. NOT exercised by the ramp. **Ship-gate: a flash-write-under-load soak (hours,
  ≥2 boards) before field OTA.**
- **Combo on a PSRAM-less board**: field-risk judge's source read (cpu_start.c,
  esp32-hal-psram.c) says BOOT_INIT off → `psramInit()` returns false gracefully (no
  bootloop); technical judge feared a bootloop (no IGNORE_NOTFOUND). **Ship-gate: flash
  the combo image to a genuinely PSRAM-less S3 and confirm graceful boot** before release.
- Combo on OCTAL-PSRAM third-party S3: quad image → PSRAM init fails → runs internal-only
  (graceful, no benefit). Acceptable; document it.
- OTGW32 (esp32 env, no PSRAM) is NOT helped by A at all — see Option C.

### OPTION B — Enable PSRAM (A) + real deinit on BLE-off (H2)
As A, plus `NimBLEDevice::deinit(true)` when SATbleenable flips off (today it leaks —
disabling BLE frees nothing, measured).
Pros: makes the toggle honest; frees ~60K when BLE off on any board incl. OTGW32.
Cons / risks (field-risk judge, HIGH):
- deinit(true) may call `esp_bt_controller_mem_release` (ONCE-only) → **same-boot re-init
  may crash/fail**; contradicts the "re-enable without reboot" premise. Judges CONFLICT on
  whether mem_release is called.
- Same ADR-151 controller-lifecycle-mutation crash family that already crash-loops the S3
  to USB-reflash-only recovery.
- **Ship-gate: ≥50 off→on cycles on ≥2 boards proving same-boot re-init survives + reclaims
  heap + sensors resume.** If it fails once → only viable as reboot-on-disable, not live toggle.
Recommendation: do NOT bundle with A. Separate task, own prerelease tag, gated hard.

### OPTION C — No-PSRAM track for OTGW32 (H9 + H3, optionally H5)
For the esp32 env (OTGW32, no PSRAM), PSRAM cannot help. Levers that free INTERNAL DMA RAM:
- **H9**: reduce static WiFi buffers (`ESP_WIFI_STATIC_RX_BUFFER_NUM` 8, `STATIC_TX` 8,
  `RX_BA_WIN` 16) — these stay internal even with PSRAM, so this helps ALL boards. (New
  hypothesis from the technical judge; untested — needs its own build + ramp + WiFi
  throughput regression check.)
- **H3**: host-pool trim (ACL_FROM_LL 12→2 ~2.8K, MSYS1 12→5 ~2K, ROLE_CENTRAL_DISABLED
  ~1-2K RAM + ~26K flash). Cheap, but small vs the 64K BLE cost; won't make BLE-on
  comfortable on OTGW32 alone.
- **H5**: BLE default OFF on no-PSRAM boards (toggle stays). Product decision (TASK-975 was
  yours). With Option B's honest deinit, "off" would actually free the memory.
Recommendation: OTGW32 realistically = BLE default OFF (H5) until H9+H3 are proven to make
BLE-on fit, which is unlikely given the ~48K controller floor is internal-only.

### Rejected / deferred (judge consensus)
- H4 (NimBLE pools → PSRAM via lib patch): needs maintainer lib approval AND puts the
  adv-report RX hot path into the cache-during-flash-write crash path. Dominated by A. Defer.
- H6 (raw-HCI observer rewrite): even full success leaves the ~48K controller cost; high
  effort + permanent HCI-parser maintenance. Prototype only if A is rejected.
- H7 (duty-cycled scanning): orthogonal to memory (heap unchanged). Drop from this matrix.
- H8 (custom framework rebuild, RUN_IN_FLASH_ONLY): permanent maintenance burden, blocks
  framework security updates. Reject unless everything else fails.

## Recommendation

**Option A**, staged and gated:
1. Set PSRAM flags per-env on esp32-classic + esp32-combo (NOT esp32/OTGW32).
2. Ship under its own prerelease tag; run the **flash-write-under-load soak** + **PSRAM-less
   boot test** before any field/beta exposure (these are the two real risks; the ramp win is
   already proven).
3. Separately, decide OTGW32 policy (Option C): default BLE off + (Option B) honest deinit,
   as its own task/tag with the 50-cycle gate.

The heap-headroom the current 16384 AsyncTCP stack and the TASK-989 serialized loader / font
routes already bought stays in; PSRAM is the lever that makes BLE-on actually fit.

## Cleanup owed regardless of decision
Temporary probes to remove before any release: `psram_*`/`internal_*` fields in
restAPI.ino device/info, the PSRAM-PROBE line in OTGW-firmware.ino setup(), and (if A is
NOT chosen) the BOARD_HAS_PSRAM/memory_type test flags in platformio.ini.
