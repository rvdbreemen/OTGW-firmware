# TASK-994: NimBLE internal-DRAM footprint on ESP32-S3 — hypotheses + proposal

Date: 2026-07-06. Baseline (STALE, flagged by adversarial review — see below):
BLE-on-by-default costs ~64K internal DRAM (measured under TASK-989/978:
~88KB free with BLE off vs ~24-33KB with BLE on at boot, before any web-serve
activity). **This number predates the NimBLE role/connection trim (hypothesis
1) and this session's ADR-165 gate-cap change — both shipped after the 64K
figure was measured. A fresh on-device boot-time measurement is needed before
treating 64K as the current baseline; it is very likely smaller now.** This
document uses 64K only as historical framing for why the investigation
started, not as a number to design against.

**Adversarially reviewed 2026-07-06** (see corrections inline below and the
"Confounds" subsection) — an independent review agent found two claims stated
with more confidence than was actually verified, and one test-plan confound
the self-review missed.

## Hypotheses (AC#1)

1. **NimBLE role/connection trim** (peripheral+broadcaster disabled,
   MAX_CONNECTIONS=1, MAX_BONDS=0, MAX_CCCDS=0, ATT_PREFERRED_MTU=23).
   Pros: already implemented and empirically measured (TASK-978,
   `platformio.ini` build_flags) — this is not a hypothesis to test, it is a
   shipped fix. Cons: none identified; this project only ever does passive
   BLE scanning (observer role, ADR-151), so disabling peripheral/broadcaster
   roles costs no functionality. **Status: DONE, already in production.**

2. **PSRAM-backed NimBLE host allocations.** NimBLE-Arduino's controller
   buffers must stay in internal DRAM (DMA-bound, hardware requirement — see
   TASK-978's "MALLOC_ALWAYSINTERNAL" finding), but HOST-side allocations
   (GATT service tables, connection bookkeeping, scan-result buffers) are not
   DMA-bound and could in principle be redirected to PSRAM via
   `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` if NimBLE-Arduino's build
   supports a PSRAM-preferred allocator hook.
   Pros: could reclaim a meaningful fraction of the DRAM footprint without
   touching controller behavior; PSRAM is present and idle on S3-Mini boards
   (2MB, confirmed via esptool).
   Cons (VERIFIED, not assumed — corrected after adversarial review flagged
   the original wording as an unverified claim stated as fact): checked the
   actual toolchain layout at
   `D:\DevData\platformio\packages\framework-arduinoespressif32`. NimBLE's
   controller AND host stack ship as a PRECOMPILED static library under
   `esp32-arduino-libs` for this target, not as source compiled by our
   build. The project's existing `-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`-style
   flags in `platformio.ini` work because they are read by the thin
   NimBLE-Arduino C++ WRAPPER source (`libraries/BLE`, which we DO compile
   fresh) — they do not reach into the prebuilt blob. A Kconfig-level
   allocator option like `MEM_ALLOC_MODE_EXTERNAL` (if it exists at all for
   this IDF/NimBLE version — not confirmed either way) would need to have
   been baked in when that static library was built; it is not overridable
   from our build via `-D` flags the way the connection-count macros are.
   Pursuing this would require either a custom `esp32-arduino-libs` variant
   (a much larger toolchain-fork undertaking, well beyond a vendored-library
   patch) or finding host-side allocation call sites inside the wrapper
   source itself that could be redirected to `heap_caps_malloc(...,
   MALLOC_CAP_SPIRAM)` — unconfirmed whether the thin wrapper does enough of
   its own allocation to matter, since the bulk of NimBLE's host-side state
   lives in the prebuilt blob, not the wrapper. **Highest theoretical
   payoff, but the prebuilt-library constraint makes the practical payoff
   likely much lower than hypothesis 5, for comparable or higher effort.**

3. **Scan window/interval widening.** NimBLE's passive-scan buffers are sized
   partly by how many concurrent scan results it must buffer between
   `ble_gap_disc()` callback drains. A wider scan interval with a narrower
   scan window reduces controller-side buffering pressure without reducing
   sensor discovery latency much (BTHome/MiBeacon sensors advertise every
   1-10s typically).
   Pros: pure config tuning, zero functional risk, testable in an afternoon.
   Cons: marginal — likely single-digit-KB savings, not a large fraction of
   the 64K gap.

4. **Reduce `CONFIG_BT_NIMBLE_MAX_CCCDS` / GATT attribute table size further.**
   Already zeroed by hypothesis 1 for the peripheral-role attribute table;
   verify the CLIENT-side (central/observer) attribute cache size is also at
   its floor — NimBLE keeps a small per-discovered-device cache even in
   pure-observer mode.
   Pros: incremental on top of hypothesis 1, same low risk.
   Cons: likely single-digit-KB; may already be at the practical floor after
   hypothesis 1's trim.

5. **Lazy BLE controller init/deinit** (only bring up the NimBLE controller
   when a scan is actually requested, deinit when idle).
   Precedent CORRECTED after adversarial review: `SATble.ino` has ZERO
   `deinit`/`NimBLEDevice::deinit` call sites (verified by grep). The existing
   "lazy-init" pattern (`satBLEInit()` called from `satBLELoop()` the first
   time BLE is runtime-enabled, per project memory) only implements the INIT
   half — once initialized, the controller runs continuously and is never
   torn down again for the life of the boot. This hypothesis's deinit half is
   therefore UNIMPLEMENTED FROM SCRATCH, not an extension of a proven
   pattern — the original wording understated the real risk by implying
   partial precedent that does not exist.
   Pros: reclaims the full BLE cost during any idle window (no scan
   requested), not just a fraction.
   Cons: controller init/deinit on an ESP32-S3 is not instantaneous (100s of
   ms) and NimBLE has a documented history of instability under repeated
   init/deinit churn on some ESP-IDF version combinations; the deinit path
   needs a soak test of its OWN (repeated init/deinit cycles, not just a
   single teardown) to rule out fragmentation or crash regression before
   this could ship, plus a policy for the idle threshold (too aggressive =
   missed sensor readings; too lazy = no savings). **Real payoff, but higher
   implementation risk than originally stated — treat as a from-scratch
   feature, not a small extension.**

6. **Do nothing / accept the 64K cost, rely on the already-shipped mitigations**
   (hypothesis 1's trim + TASK-978's frontend retry loader + ADR-165's N=2
   gate cap, all already shipped and re-verified this session). The chronic
   fragmentation symptom (unstyled UI under concurrent load) is already fixed
   at the SYMPTOM layer; further BLE-DRAM reduction is a ROOT-CAUSE
   improvement with diminishing marginal value now that the symptom no longer
   manifests.
   Pros: zero further engineering risk; the actual field-reported bug
   (unstyled UI) is already resolved.
   Cons: leaves genuine headroom on the table for future features that might
   need more internal DRAM (e.g. deeper heap-frag-soak-proven gating removal,
   TASK-956 — currently running as a soak on the shared bench device as of
   this writing).

## Test-plan (as executed / as proposed) — AC#2 adversarial review

**Hypothesis 1** was already executed and evidenced in TASK-978's notes
(measured concurrent-serve pass rate before/after the trim: 4/9→2/9→1/9
collapsing baseline vs 6/9→5/9→8/9 holding after the trim). Adversarial
self-review of that test: the measurement used raw parallel curl (no browser
retry logic), isolating the firmware-side effect from the frontend mitigation
that shipped alongside it — a fair, confound-free comparison.

**Hypotheses 3-5** are NOT YET EXECUTED. Proposed test plan, self-reviewed for
gaps before running:
- Baseline: telnet `z` reset, record `hd_min_max_block`/`freeheap` at boot
  with BLE enabled (current config).
- Per-hypothesis: apply the config/code change, rebuild, reflash, reboot,
  record the same counters at boot and after a 10-minute idle window (no web
  traffic) to isolate the BLE-specific delta from serve-path fragmentation.
- Gap identified in self-review: boot-time-only measurement would miss
  hypothesis 5's real payoff (idle-triggered deinit reclaiming memory AFTER
  boot, not instead of the boot-time allocation) — the test plan for
  hypothesis 5 specifically needs a "measure at boot, then measure again
  after N minutes idle with no scan activity" pair, not a single boot-time
  reading like hypotheses 3/4.
- Gap identified in self-review: none of these tests should run on a device
  mid-soak for a DIFFERENT task (TASK-956) — heap-frag soak data would be
  contaminated by a mid-soak reboot/reflash. Deferred until the bench device
  is free.
- **Gap identified by ADVERSARIAL review (missed in self-review): no control
  for WiFi/MQTT activity or ambient BLE RF density.** WiFi and MQTT draw from
  the same internal-DRAM pool and can produce single-digit-to-double-digit-KB
  swings that would dwarf hypothesis 3/4's expected marginal savings —
  comparing a hypothesis-3 run against a baseline run taken at a different
  WiFi/MQTT connection state (e.g. one during an MQTT reconnect storm, one
  fully idle) would misattribute noise to the BLE change. Ambient BLE
  advertiser density near the bench also affects hypothesis 3's scan-buffer
  pressure directly (the hypothesis's own rationale). Fix: hold WiFi
  connected + MQTT in the SAME state (either both connected or both
  disconnected, not mixed) across all runs, and log ambient BLE
  advertisement count (`GET /api/v2/sat/ble/roster` `count` field, or a raw
  scan-result count) alongside each measurement so a density shift is
  visible rather than silently absorbed into the "BLE hypothesis" delta.

## Implementation proposal (AC#4) — for maintainer decision (AC#5)

Given the above, three options with genuinely different risk/reward:

- **Option A (minimal, recommended default): ship hypothesis 1 (already
  done) + hypothesis 3/4 (scan tuning, low-risk, same afternoon).** Modest
  additional single-digit-KB gains on top of the already-shipped 64K→~40K-ish
  reduction (exact post-trim number needs a fresh on-device measurement once
  the bench device is free — TASK-978's before/after numbers were maxblock
  under load, not the boot-time internal-DRAM figure this task's baseline
  quotes).
- **Option B (moderate risk/reward): also pursue hypothesis 5 (lazy
  controller init/deinit).** Reclaims the full BLE cost during idle windows,
  but needs its own soak test to rule out init/deinit churn instability
  before shipping, and a policy decision on the idle threshold.
- **Option C (highest risk/reward, likely out of scope for this task):
  pursue hypothesis 2 (PSRAM-backed NimBLE host allocations).** Needs a
  feasibility spike first (does NimBLE-Arduino's build even expose a PSRAM
  allocator hook at the version pioarduino ships) before any implementation
  work is justified — could turn out to require forking a vendored framework
  package, which this project's CLAUDE.md explicitly scopes OUT of the
  vendored-library exception list (only `SimpleTelnet` and `OTGWSerial` are
  Claude-editable vendored libs).

**No option has been implemented in this pass beyond hypothesis 1 (already
shipped before this task existed).** AC#5 (maintainer decision) is open —
this is exactly the kind of design-scope decision (how much further
engineering risk to accept for a symptom that's already fixed) that needs the
maintainer's call, not an autonomous one.
