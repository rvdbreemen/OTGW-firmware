# ADR-135 ESP32 TWDT is the Primary Watchdog, External 0x26 Chip Demoted to Optional Secondary (Amends ADR-011)

## Status

Proposed, 2026-06-14.

This is the **watchdog half of Phase 4** of the 2.0.0 concurrency-model rollout
(ADR-123), landing after the ESP8266 source drop (ADR-128, TASK-865.2) and the
PIC-UART dedicated-task lift (ADR-130, TASK-865.6). It **amends ADR-011**
(External Hardware Watchdog): ADR-011 still documents the 0x26 chip's I2C
protocol mechanics verbatim, but its *architectural premise* — that an external
I2C watchdog is the device's primary recovery mechanism because the ESP8266's
internal/software watchdog "shares the same failure domain as the main firmware"
— no longer holds on the ESP32-S3. On acceptance, ADR-011's Status line becomes
"Amended by ADR-135, 2026-06-14" (only that line changes; ADR-011's body stays
immutable as the 0x26 protocol reference).

Field validation on ESP32-S3 hardware (an induced loop hang triggering a TWDT
panic-reset within the timeout on both `esp32` and `esp32-classic`; the 0x26 chip
not spuriously resetting an `esp32-classic` in normal operation; OTA and PIC-flash
completing without a watchdog reset) is still open and is tracked under epic
TASK-865, separate from this architectural acceptance.

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: Re-scope the watchdog (ADR-011) onto the ESP32 TWDT as primary and demote the external 0x26 I2C chip to optional secondary, after the ESP8266 drop (ADR-128) retired the single-failure-domain rationale; delivered by TASK-865.12; cross-refs ADR-123/127/128/130
    changed_via: adr-kit

## Context

### The premise that no longer holds

ADR-011 (2018) chose an external I2C hardware watchdog at address `0x26` on the
Nodoshop OTGW Classic PCB as the device's primary recovery mechanism. Its stated
reason for rejecting the alternative "ESP8266 internal/software watchdog" was a
**single-failure-domain** argument: *"Software watchdogs share the same failure
domain. If the system is completely hung, software watchdog may not function."*
On the ESP8266 that was a sound call — the SDK software watchdog could be starved
or disabled by buggy code, and there was no independent hardware timer the
application could not also wedge.

### What changed

Two 2.0.0 decisions invalidate the premise on the surviving platform:

- **ADR-128** dropped ESP8266 from 2.0.0. Every surviving build targets the
  ESP32-S3, which has an independent **Task Watchdog Timer (TWDT)** in hardware
  (the MWDT peripherals behind `esp_task_wdt_*`). The TWDT runs in the
  silicon-level timer group, fires an interrupt and (with `trigger_panic=true`) a
  panic-reset that the application code cannot disable from within a hung task —
  it is **not** in the same failure domain as the application loop. The original
  reason to prefer an external chip over the on-chip watchdog is gone.
- **ADR-130** moved the PIC UART onto a dedicated FreeRTOS task. The firmware is
  now multi-tasked (loop task + AsyncTCP service task + PIC-UART task), so "the
  watchdog" must be defined against a specific task, not "the loop".

### Current state (the bug this ADR's task fixes)

After the ESP8266 drop, the watchdog block in `OTGW-Core.ino` carried **three**
`#if` shapes inherited from the ADR-127 combo era, dispatched on
`HAS_PIC_WATCHDOG` / `HAS_RUNTIME_HW_DETECT` (`boards.h`):

| Board | `HAS_PIC_WATCHDOG` | `HAS_RUNTIME_HW_DETECT` | Shape hit | TWDT armed? |
|---|---|---|---|---|
| OTGW32 (`esp32`) | 0 | 0 | `#else` | yes |
| `esp32-classic` | 1 | 0 | `HAS_PIC_WATCHDOG && !HAS_RUNTIME_HW_DETECT` | **NO** |
| combo | 1 | 1 | `HAS_PIC_WATCHDOG && HAS_RUNTIME_HW_DETECT` | yes |

The first shape was the original ESP8266 external-0x26-only code (no
`esp_task_wdt_*` calls at all). With ESP8266 gone, that shape is now hit by the
**`esp32-classic`** build — an ESP32-S3 board that consequently ran with **no TWDT
armed**, relying solely on the external 0x26 chip. That is a single point of
failure: if the 0x26 chip, its I2C bus, or its solder joints fail, the
`esp32-classic` had no software-independent recovery at all.

### Forces

- The external 0x26 chip still exists physically on the OTGW Classic PCB
  (`esp32-classic` always; combo only when booted into PIC mode). Removing its
  feed would let an *armed* chip reset a healthy board.
- The TWDT has no enable/disable (it is always running once configured); ADR-011's
  `WatchDogEnabled(0/1)` arm/disarm contract is meaningful only for the external
  chip. The OTA / PIC-flash long-operation handling must therefore be re-expressed
  in TWDT terms (reset the timer, do not "disable" it).
- The ESP-abstraction rule (CLAUDE.md): no raw `#ifdef ESP8266`/board-macro reads
  outside the allowlisted files; all dispatch routes through `HAS_*` flags.

## Decision

**On the 2.0.0 ESP32-S3 line, the ESP32 Task Watchdog Timer (TWDT) is the primary
watchdog on every build. The external 0x26 I2C chip is demoted to an optional
secondary layer, present only on the OTGW Classic PCB.**

Concretely:

1. **Collapse the three watchdog shapes to two.** The
   `HAS_PIC_WATCHDOG && !HAS_RUNTIME_HW_DETECT` (formerly ESP8266-only, now
   `esp32-classic`) external-only branch is removed and merged into the
   `HAS_PIC_WATCHDOG` branch that arms the TWDT. The surviving dispatch is:
   - `#if HAS_PIC_WATCHDOG` → `esp32-classic` + combo: **TWDT primary**, external
     0x26 **secondary**.
   - `#else` → OTGW32: **TWDT only**.

2. **TWDT is armed on every survivor.** `initWatchDog()` reconfigures/initializes
   the TWDT (30 s timeout, `trigger_panic=true`) and subscribes the loop task
   (`esp_task_wdt_add(NULL)`) in both surviving shapes. `feedWatchDog()` resets the
   TWDT (`esp_task_wdt_reset()`, gated on `s_twdtReady` so early-setup feeds before
   subscription do not spam "task not found"), rate-limited to 100 ms.

3. **The external 0x26 chip is a SECONDARY layer, fed UNCONDITIONALLY.** On every
   `HAS_PIC_WATCHDOG` build (esp32-classic AND combo) the 0x26 arm, feed and
   boot-status read all run unconditionally — never gated on `isPICEnabled()`.
   Rationale (maintainer directive 2026-06-14): 0x26 presence is **not reliably
   detectable** — later NodoShop Classic revisions dropped the chip, and an I2C
   ACK is not proof of board type — while arming/feeding an absent 0x26 is a
   harmless NACKed no-op on floating pins. The only correctness hazard is the
   converse: NOT feeding a chip that IS present resets a healthy board. An earlier
   draft of this ADR gated the combo feed on `isPICEnabled()` while leaving the arm
   unconditional; that asymmetry would let a combo on an old Classic PCB with a
   dead/undetected PIC **arm the chip but never feed it -> spurious reset loop**
   (which also blocks web-UI recovery). Symmetric unconditional arm+feed+boot-read
   avoids it and matches the original ESP8266 NodoShop firmware. The sole
   PIC-presence probe remains `detectPIC()` (reset -> ETX), never the 0x26 bus.

4. **`WatchDogEnabled()` keeps ADR-011 semantics for the secondary chip; no-op on
   OTGW32.** On `HAS_PIC_WATCHDOG` boards it arms/disarms the 0x26 chip (the
   boot-time disarm before the WiFi portal, and the OTA-flash disarm). On OTGW32 it
   is a no-op (`(void)stateWatchdog;`) because the TWDT has no enable/disable. The
   TWDT is never disabled for a long operation; instead the loop task keeps
   resetting it through `doBackgroundTasks()->feedWatchDog()`, which runs every
   loop iteration during both ESP and PIC flash.

5. **TWDT subscription set: loop task only (explicit decision).** Only the loop
   task is subscribed (`esp_task_wdt_add(NULL)` from the loop context;
   `idle_core_mask=0`). The dedicated PIC-UART task (ADR-130) is **deliberately not
   subscribed**. Rationale and the accepted residual gap are in §Consequences.

6. **OTA flash-feed re-pointed.** ADR-011 located the per-chunk watchdog feed in
   `OTGW-ModUpdateServer-impl.h` (the ESP8266 synchronous upload server), which
   ADR-128/TASK-865.2 deleted. The ESP32 per-chunk 0x26 feed now lives in the
   async OTA write handler (`OTGW-ModUpdateServer-esp32.h`, ADR-134 / seq11). The
   loop-task TWDT needs no reset in that handler: the async chunk write runs on the
   AsyncTCP task while the loop task keeps feeding the TWDT.

This ADR **amends ADR-011**: the 0x26 I2C protocol (feed `0xA5`, arm/disarm
register 7, status register 17) documented there is unchanged and still
authoritative for the secondary layer; only the architectural role (primary →
secondary) and the single-failure-domain rationale (retired) change.

## Alternatives Considered

### Alternative 1 — Keep the external 0x26 chip primary, add TWDT as secondary
Leave the 0x26 chip as the named primary on Classic boards and treat the TWDT as a
backstop.
**Pros:** smallest narrative change from ADR-011.
**Cons:** inverts the actual reliability ordering on the ESP32-S3 — the on-chip
TWDT is software-independent and present on *every* build, while the 0x26 chip is
absent on OTGW32 and can fail independently (bus/solder). It would also leave
OTGW32 (no 0x26) described as having "no primary watchdog".
**Rejected:** the TWDT is the layer present and software-independent everywhere; it
is the primary by construction.

### Alternative 2 — Remove the external 0x26 chip entirely
Drop the 0x26 feed/arm/disarm and rely on the TWDT alone on all boards.
**Pros:** one watchdog, simplest code; no I2C traffic to 0x26.
**Cons:** the chip is physically on every shipped OTGW Classic PCB and is *armed*
at boot; stopping the feed would let it reset a healthy `esp32-classic` every few
seconds. Defusing it would require an unconditional disarm on a board where it is
wanted as defence-in-depth (a true-hardware reset path the TWDT's panic-reset
cannot fully replicate if the SoC itself wedges below the task scheduler).
**Rejected:** throws away a working independent hardware reset on the boards that
have it, for no real simplification (the secondary path is ~3 lines gated on a
compile-time-foldable flag).

### Alternative 3 — Subscribe the PIC-UART task to the TWDT as well
Add `esp_task_wdt_add()` for the dedicated PIC task so a wedged PIC task also trips
a panic-reset.
**Pros:** the safety-critical serial task gets watchdog coverage.
**Cons:** two real hazards. (a) The PIC task **parks** (sleeps in
`platformTaskDelay(20)` indefinitely in OTDirect mode on the combo, and during
every flash); a subscribed-but-parked task would trip the 30 s panic unless its
entry is reset on the parked path too — extra surface on the most timing-sensitive
task. (b) `feedWatchDog()` uses a function-local `static DECLARE_TIMER_MS`; calling
it from two tasks shares that static (a data race) and the 100 ms rate-limit lets
one task's call starve the other's reset. Correct multi-task subscription needs a
dedicated per-task reset on every path (active + parked) with subscription inside
`startPICSerialTask()` — note `initWatchDog()` runs *before* `startPICSerialTask()`
in `setup()`, so the loop-side init cannot subscribe the not-yet-created handle.
**Rejected for now (loop-only chosen):** loop-only is zero added code and satisfies
the induced-hang field requirement. The residual gap (a wedged PIC task does not
self-trip the TWDT) is accepted and recorded in §Consequences; it surfaces as a
stalled OT frame/TX queue, observable loop-side, and is revisitable as a separate
task if field evidence shows the PIC task hanging in isolation.

## Consequences

### Positive
- **No single point of failure on any build.** Every survivor now arms the TWDT;
  `esp32-classic` is no longer external-0x26-only. A 0x26 chip/bus/solder failure
  no longer leaves a board with no software-independent recovery.
- **Defence in depth on the Classic PCB.** `esp32-classic` (and combo-in-PIC-mode)
  keep both layers: the on-chip TWDT *and* the truly-external 0x26 hardware reset.
- **Simpler dispatch.** Three watchdog shapes collapse to two; the ESP8266-era
  external-only branch is gone. All dispatch stays behind `HAS_*` flags (no raw
  board-macro reads), per the ESP-abstraction rule.
- **Consistent long-operation handling.** OTA and PIC flash keep feeding the TWDT
  via the unconditional `doBackgroundTasks()->feedWatchDog()` every loop iteration;
  the external chip is disarmed/fed exactly as ADR-011 prescribed.

### Negative / accepted residual
- **PIC task not watchdog-covered.** A hang *inside* the dedicated PIC-UART task
  (not the loop) does not directly trip the TWDT (loop-only subscription). It
  surfaces indirectly as a stalled TX/OT-frame queue, observable from the loop
  side. Accepted per Alternative 3; revisit only on field evidence.
- **OTGW32 has one watchdog layer.** By hardware design (no 0x26 chip) the OTGW32
  relies on the TWDT alone. This is unchanged from prior OTGW32 behaviour and is
  acceptable: the TWDT is software-independent.

### Risks & Mitigation
- **Risk:** the TWDT 30 s timeout is long enough that a slow stall delays recovery.
  **Mitigation:** 30 s is intentionally generous to avoid false positives during
  legitimate long operations (flash, WiFi bring-up); the field AC validates an
  induced hang triggers a panic-reset within the timeout.
- **Risk:** a combo on an old Classic PCB (0x26 present) whose PIC is dead or
  undetected could be reset-looped if the chip were armed but not fed.
  **Mitigation:** the 0x26 arm, feed and boot-read all run unconditionally on every
  `HAS_PIC_WATCHDOG` build (no `isPICEnabled()` gate), so a present chip is always
  fed and an absent one only NACKs; arm and feed are symmetric. The field AC
  validates no spurious 0x26 reset in normal combo/classic operation.

## Related Decisions

- **Amends:** ADR-011 (External Hardware Watchdog) — its 0x26 I2C protocol stays
  authoritative; its primary-watchdog role and single-failure-domain rationale are
  re-scoped here.
- **Depends on:** ADR-128 (drop ESP8266 — retires the single-failure-domain
  premise), ADR-130 (PIC-UART dedicated task — defines the multi-task context).
- **Coexists with:** ADR-127 (combo ESP32-S3 single binary — runtime PIC/OTDirect
  boot detection; the 0x26 feed is intentionally NOT keyed off it, see Decision §3),
  ADR-134 (async OTA handler — where the per-chunk 0x26 feed now lives).
- **Related:** ADR-029 (XHR OTA flash — watchdog coordination), ADR-030 (heap
  recovery — complementary software-level recovery), ADR-036 (boot sequence —
  watchdog enable/disable ordering), ADR-080 (binding ADRs need a CI gate).

## References

### Implementation files
- `src/OTGW-firmware/OTGW-Core.ino` — watchdog block (`#if HAS_PIC_WATCHDOG` /
  `#else`): `initWatchDog()`, `WatchDogEnabled()`, `feedWatchDog()` (0x26
  arm/feed/boot-read all unconditional), `EXT_WD_I2C_ADDRESS`.
- `src/libraries/Platform/src/boards.h` — `HAS_PIC_WATCHDOG` /
  `HAS_RUNTIME_HW_DETECT` per board (OTGW32 / esp32-classic / combo).
- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h` — per-chunk 0x26 secondary feed
  in the async OTA write handler (re-pointed from the deleted
  `OTGW-ModUpdateServer-impl.h`).
- `src/OTGW-firmware/OTGW-firmware.ino` — boot-time `WatchDogEnabled(0)` disarm and
  `WatchDogEnabled(1)` arm; `initWatchDog()` call site; `doBackgroundTasks()`
  unconditional `feedWatchDog()`.

### External
- ESP-IDF Task Watchdog Timer API (`esp_task_wdt_init` / `_reconfigure` / `_add` /
  `_reset` / `_status`): <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html>

## Enforcement

Per ADR-080 a binding pattern-level decision wants a CI gate, and ADR-123 promised
each rollout phase adds its own enforceable boundary. This is the watchdog boundary
of Phase 4. The load-bearing invariant — **no surviving watchdog shape lacks an
`esp_task_wdt_*` path** — is verified by a forbidden-pattern gate scoped to the
watchdog source file plus a grep check in the task's Definition of Done. The
external-only (no-TWDT) branch is forbidden by name so it cannot be reintroduced;
the per-board `HAS_PIC_WATCHDOG` flags continue to dispatch the two surviving
shapes. The patterns were grep-verified to have zero matches on the re-scoped
file. Declarative and deterministic; the maintainer ratifies (or downgrades) this
block at the Proposed checkpoint.

```json
{
  "forbid_pattern": [
    {"pattern": "HAS_PIC_WATCHDOG && !HAS_RUNTIME_HW_DETECT", "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
     "message": "The external-0x26-only (no-TWDT) watchdog shape is retired (ADR-135). esp32-classic must arm the ESP32 TWDT as primary; the 0x26 chip is an optional secondary under #if HAS_PIC_WATCHDOG."}
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
