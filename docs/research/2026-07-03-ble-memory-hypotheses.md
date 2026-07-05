# BLE memory reduction — hypotheses & test plan (TASK-994)

Date: 2026-07-03. Context: BLE-on (NimBLE-Arduino 2.1.0, passive observer) costs
**63.8K free heap / 26.6K maxblock** on the ESP32-S3 (measured: 88.0K/40.9K → 24.2K/14.3K).
At 24K free the async web stack collapses under any ~4+ concurrent connections
(TASK-989 evidence). Research inputs: 5-agent web sweep (NimBLE internals, controller
mechanics, alternatives, PSRAM strategy, peer projects) + local source verification.

## Measured / verified facts

- F1: Controller (precompiled `libbt.a`) is heap-allocated at init (~348B static BSS
  in our ELF), est. 40–50K of the 64K. Host knobs cannot touch it.
- F2: `BT_CONTROLLER_INIT_CONFIG_DEFAULT` expands **in NimBLE-Arduino source** (compiled
  per-project); libbt honors the runtime struct → fields like `normal_adv_size`
  (scan-dup cache) are runtime-tunable (`NimBLEDevice::setScanDuplicateCacheSize`).
  `-DCONFIG_BT_CTRL_*` build flags do NOT work (prebuilt sdkconfig.h hard-defines, no #ifndef).
- F3: `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=1` as build flag is **inert** (sdkconfig.h
  hard-defines INTERNAL; exp_nimble_mem.c checks INTERNAL first) and the EXTERNAL branch
  is hard-SPIRAM-no-fallback (crashes PSRAM-less boards; upstream #317/#8020 wontfix).
- F4 **[CORRECTED 2026-07-03]**: the build DOES compile against a SPIRAM-enabled
  sdkconfig. pioarduino-build.py:533/540 selects `esp32s3/<memory_type>/include`,
  memory_type defaulting to `flash_mode + "_qspi"` = **qio_qspi** (our flash_mode=qio,
  no explicit memory_type). The qio_qspi variant's sdkconfig.h has `CONFIG_SPIRAM 1`,
  `SPIRAM_USE_MALLOC 1`, `SPIRAM_MALLOC_ALWAYSINTERNAL 4096`, `SPIRAM_TRY_ALLOCATE_WIFI_LWIP 1`,
  `SPIRAM_MODE_QUAD`, `SPIRAM_TYPE_AUTO`. So the compile-time config is PSRAM-on already.
  OPEN QUESTION (decides H1's entire value): is PSRAM actually INITIALIZED at runtime?
  The measured free heap (88K/24K) reads as INTERNAL-only. Either (a) PSRAM never inits
  (missing BOARD_HAS_PSRAM / boot-init / psramInit) → the 2MB is dormant and H1 = make it
  init; or (b) it IS active, WiFi/lwip buffers already external, and the residual internal
  pressure is BLE-controller + sub-4K allocs (2872B web send buffer < 4096 threshold stays
  internal) → H1 already in effect and won't help, pushing weight onto H2/H3/lib-patch.
  MUST resolve first (T1a: boot a probe build logging psramFound()/getPsramSize()/
  heap_caps internal-vs-spiram via telnet). No SPIRAM_IGNORE_NOTFOUND seen in qio_qspi →
  PSRAM-less-board boot safety of a BOARD_HAS_PSRAM image is an explicit risk to verify.
- F5: Disabling BLE via settings does NOT reclaim memory (no deinit path): measured
  22.9K free after disable vs 88K clean. `NimBLEDevice::deinit(true)` would return
  controller+host heap (~60K) and BLE can re-init in the same boot.
- F6: Host pool trims still available: ACL_FROM_LL_COUNT 12→2 (#ifndef-guarded, ~2.8K),
  MSYS1 12→5 (~2K), ROLE_CENTRAL_DISABLED (~1–2K RAM, ~26K flash; NimBLEScan guards on
  ROLE_OBSERVER, so scanning unaffected). HCI EVT pool (30×70B) is NOT flag-tunable
  (unguarded defines — lib patch required).
- F7: Peer projects: WLED/EMS-ESP ship no BLE at all ("prohibitive"); Tasmota isolates
  BLE in a separate build; ESPHome's first OOM mitigation is "disable web_server";
  OpenMQTTGateway duty-cycles scans. Nobody runs continuous scan + async web + MQTT
  comfortably on ~90K internal.
- F8: Matter-SDK measured trims: CONFIG_BT_CTRL_RUN_IN_FLASH_ONLY frees ~19K D/IRAM —
  but it is baked into the prebuilt libs → requires a custom framework rebuild.

## Hypotheses

### H1 — Enable PSRAM in the S3-Mini build envs (esp32-classic, esp32-combo)
`-DBOARD_HAS_PSRAM` + `board_build.arduino.memory_type = qio_qspi`.
Claim: WiFi buffers + ≥4K allocs move external → **30–60K internal freed**, BLE can
stay as-is; the async web stack gets its working room back.
+ Biggest single win; no BLE behaviour change; app code untouched.
+ Attacks the SYSTEM budget instead of squeezing BLE (research shows BLE floor ≈45K anyway).
− Only helps PSRAM boards (OTGW32 has none — but OTGW32 uses the `esp32` env, unchanged).
− Combo image can also land on PSRAM-less hardware (GPIO44-float class) → must verify
  graceful "PSRAM not found" boot.
− WiFi/BLE+PSRAM coexistence caveats (flash-write windows; task stacks stay internal).
Test: build variant → flash S3-Mini → heap/ramp/BLE-functional; boot-check on OTGW32 if available.

### H2 — Real deinit on BLE-disable (app code)
Call `NimBLEDevice::deinit(true)` when SATbleenable flips off (and on SAT-BLE idle?).
Claim: ~60K reclaimed at runtime; BLE-off boards stop paying entirely.
+ Pure app code; makes the setting honest (now it leaks, F5).
+ Combines with every other option.
− Doesn't help while scanning is ON (the normal field state for BLE users).
− Re-init cycle needs the known active-scan-crash caution (ADR-151: passive-continuous only).
Test: toggle on→off→on with heap samples + sensor pickup after re-enable.

### H3 — Host-pool trim round 2 (build flags + 1 API call)
ACL_FROM_LL_COUNT 12→2, MSYS1 12→5, ROLE_CENTRAL_DISABLED, `setScanDuplicateCacheSize(20)`.
Claim: **~6–9K internal** + ~26K flash.
+ Cheap, reversible, no lib patch (all #ifndef-guarded or runtime API).
− Small vs the 64K problem; scan-dup cache 100→20 may re-report duplicates more often
  (roster dedups by MAC, so functional risk low).
Test: build variant → heap delta + BLE-functional (4 sensors still seen) + msys depletion check.

### H4 — NimBLE host pools to PSRAM via library patch
Patch `exp_nimble_mem.c` to prefer-SPIRAM-with-internal-fallback (upstream-rejected fix).
Claim: 5–13K internal moved out on PSRAM boards.
+ Complements H1; fallback keeps PSRAM-less boards safe.
− **Library patch** (project rule: needs explicit maintainer approval; NimBLE-Arduino is
  lib_deps, so it means vendoring or a patch step); adv-report hot path lands in PSRAM.
− Small gain if H1 already lands.
Test only if H1+H3 insufficient. Proposal-level otherwise.

### H5 — Per-board BLE default policy
BLE default ON only where the budget allows (PSRAM boards after H1); default OFF on
OTGW32/no-PSRAM (toggle stays available, with H2 the toggle becomes honest).
+ Zero engineering risk; aligns with peer practice (Tasmota/ESPHome gate BLE).
− Product/policy call (TASK-975 was an explicit maintainer decision) — not Claude's to make.
No hardware test; decision item.

### H6 — Replace NimBLE host with raw-HCI minimal observer
esp_vhci_host API + ~300–500 line HCI parser (IDF `ble_adv_scan_combined` is the
canonical S3 example). Claim: host removed → ~20–30K RAM + 80–120K flash.
+ Biggest BLE-side ceiling; we need only adv reports (BTHome/ATC/MiBeacon).
− High effort; unproven under Arduino+prebuilt core (VHCI symbols present but untested
  here); we own an HCI parser forever; controller cost (~40–50K) remains anyway.
Proposal-level now; prototype only if H1 route is rejected.

### H7 — Duty-cycled / windowed scanning (ESPHome 30/320ms or OMG N-of-M pattern)
Claim: radio/CPU coexistence relief; heap unchanged (stack stays initialized).
+ Reduces WiFi contention during scans; may smooth web latency.
− Does NOT address the memory root cause; passive-continuous is our proven-stable mode
  (ADR-151); windowing risks missing slow-advertising sensors.
Optional complement; low test priority.

### H8 — Custom framework rebuild (RUN_IN_FLASH_ONLY etc.)
Claim: ~19K D/IRAM + controller trims (scan-dup cache, EVT pools at the source).
− Owning a custom prebuilt core = permanent maintenance burden; blocks framework updates.
Reject unless everything else fails. Proposal-level only.

## Proposed test order (bench S3-Mini, COM8)

T0 baseline re-measure (current build, BLE on/off) — done (F-numbers above).
T1 = H1 (PSRAM env flags) — heap + ramp conc 2–12 + BLE functional + OTGW32 boot check.
T2 = H1+H3 combined — same measurements (expected best all-round config).
T3 = H2 (deinit) — toggle cycle heap + re-init stability.
T4 = H7 (optional, only if time) — scan window experiment, web latency during scan.
Success criterion: with BLE ON, ≥60K internal free / ≥30K maxblock steady-state AND
ramp conc=8 serves without wedge; BLE sensors still received; OTGW32 image unaffected.
