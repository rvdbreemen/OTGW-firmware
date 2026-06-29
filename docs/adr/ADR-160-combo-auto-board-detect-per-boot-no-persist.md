# ADR-160 Combo board: AUTO hardware detection re-probes every boot and never persists its verdict

## Status

Proposed, 2026-06-29.

Amends ADR-127 (combo ESP32-S3 single binary, runtime PIC/OTDirect boot
detection). ADR-127 stays in force; this ADR corrects one specific behaviour of
its AUTO-detection path: the cache-back into `settings.iBoardMode`. Everything
else in ADR-127 (the board class, the build target, the pin re-mux, the
dual-path watchdog, the mutual-exclusion gates) stands unchanged.

Guideline-level (per ADR-080): no new `evaluate.py` gate is introduced. The
enforcement posture is identical to ADR-127 and ADR-158: the existing
ESP-abstraction boundary gate (`evaluate.py::check_esp_abstraction_boundary`)
enforces the flag-based layering. If AUTO-detection regressions reappear, a
`check_combo_runtime_selection` gate should be added and ADR-127, ADR-158 and
this ADR promoted to binding.

## Status History

status_history:
  - date: 2026-06-29
    status: Proposed
    changed_by: Agent
    reason: Capture the maintainer decision (TASK-947) that combo AUTO detection re-probes every boot and never writes its verdict into the manual-override field, closing the ADR-127 stranding bug.
    changed_via: adr-kit

## Context

The combo ESP32-S3 binary (board macro `BOARD_NODOSHOP_ESP32_COMBO`, capability
flag `HAS_RUNTIME_HW_DETECT=1`) links both OpenTherm (OT) engines and decides at
boot whether it is running on an OTGW Classic PCB driving a PIC microcontroller
(the `PIN_CLASSIC_*` pin map) or on an OTGW32 / OT-Thing PCB driving OpenTherm
directly (OT-Direct, the `PIN_*` map). ADR-127 §3 made that AUTO decision and
then **cached the verdict back into `settings.iBoardMode`** ("so later boots
skip probing"): a no-PIC boot persisted `iBoardMode = 2` (OT-Direct) the moment
`initOTDirect()` succeeded, and ADR-127's `iBoardMode != 0` fast path then
applied that cached value on every subsequent boot without re-probing.

An aggressive code review of that path found it strands hardware. The PIC
discriminator is `detectPIC()`, a single ETX (End-of-Text, 0x03) handshake
check on UART1. That probe can transiently fail: a busy PIC, a missed ETX, or a
marginal UART line all read as "no PIC" on one boot. The failure chain is:

1. A real Classic+PIC board boots; `detectPIC()` transiently returns false.
2. The AUTO path falls through to `initOTDirect()`, which sets
   `HW_MODE_OT_DIRECT` **unconditionally** — there is no OT-bus confirmation
   that a boiler is actually present on the direct path.
3. ADR-127 §3 caches `iBoardMode = 2` and persists it.
4. On every later boot the `iBoardMode == 2` fast path skips the PIC probe
   entirely, so the now-healthy PIC is never seen again. The board is
   permanently stranded in OT-Direct; only manual user intervention (set
   `boardmode` back to auto, or force PIC) recovers it.

The root fault is semantic, not just a probe-reliability issue: an AUTO *guess*
was written into the field that is supposed to hold a *user decision*.
`settings.iBoardMode` is the manual override selector. Giving a transient guess
the permanence of a deliberate override is the bug. ADR-127 §7 (banner-recovery
re-detect) only papered over the Classic case where a PIC banner later arrives;
it did not address the underlying conflation, and it does nothing for a board
that never re-runs the probe.

## Decision

On the combo board (`HAS_RUNTIME_HW_DETECT`), AUTO hardware detection
(`settings.iBoardMode == 0`) re-probes the PIC on **every** boot and **never**
persists its verdict. `settings.iBoardMode` is redefined as the manual-override
field only: `0` = auto, `1` = force PIC on S3 Mini, `2` = force OT-Direct, `3` =
force PIC on S3 Mini Pro. When AUTO finds no live PIC and no live OT bus, the
firmware assumes OTGW32 / OT-Direct as a deliberate "bench" (non-production)
default, re-evaluated from scratch on the next boot.

As built (TASK-947, maintainer decision 2026-06-29):

- The AUTO branch (`iBoardMode == 0`) opens UART1, binds the live PIC
  reset/LED pins per ADR-158 (`activePicRst`), and runs `detectPIC()` on every
  boot.
- A live PIC resolves to Classic, expressed only in the transient
  `state.hw.eMode` (per ADR-051, this is live state, not persisted config).
- No live PIC triggers `OTGWSerial.end()` (the ADR-125 floating-pin RX-storm
  guard, so the released UART RX pin cannot flood the input) followed by
  `initOTDirect()`, which sets `HW_MODE_OT_DIRECT`. This is the OTGW32 /
  OT-Direct bench default.
- The AUTO verdict is **never** written to `settings.iBoardMode`. The
  ADR-127 §3 cache-back lines are removed from the AUTO branch.
- `comboActivePinMap()` already resolves the AUTO pin map from the transient
  `state.hw.bClassicPro` (the S3 Mini Pro IMU (Inertial Measurement Unit) probe,
  per ADR-158) and `state.hw.eMode`, so dropping the cache needs **no** change
  to the pin resolver. The decision self-corrects: a PIC that appears on a
  later boot is detected and wins automatically, with no manual intervention.
- The manual force paths (`iBoardMode == 1 / 2 / 3`) are unchanged. The
  `iBoardMode == 2` behaviour ("forced OT-Direct, skip the PIC probe") is now
  reachable **only** by an explicit user force, never by an AUTO guess.

The scope is narrow: this changes what the AUTO branch does after it decides
(stop persisting, always re-probe), not how it decides (the discriminator is
still `detectPIC()` on UART1).

## Alternatives Considered

### Alternative A: cache `iBoardMode = 2` only when the OT bus was confirmed live

Persist the OT-Direct verdict only when `probeOTBus()` actually saw a live OT
bus, so a no-boiler bench board would not get cached.

Rejected. It still persists an AUTO verdict into the manual-override field,
keeping the semantic conflation that is the root fault. It also fails a
legitimate case: an OTGW32 in OT-Direct mode with no boiler attached (a bench
unit, exactly the bench default this ADR embraces) would then never cache and
never settle, while a Classic board with a momentarily live-looking line could
still mis-cache. It treats a symptom, not the cause.

### Alternative B: keep caching, but re-probe the PIC every boot even when cached `== 2`

Leave the persistence in place but always re-run `detectPIC()` so a stranded
board recovers.

Rejected. If the probe runs every boot regardless, the cache buys nothing — it
no longer "skips probing", which was its only stated purpose in ADR-127 §3.
Worse, it keeps an AUTO-derived value sitting in the manual-override field, so
the UI and REST (Representational State Transfer) API would report a user
override the user never set. It is the
cost of caching with none of the benefit.

### Alternative C: do not persist AUTO at all; re-detect every boot; OTGW32 is the no-PIC bench default (chosen)

Drop the cache-back entirely, re-probe on every AUTO boot, and treat "no PIC,
no OT bus" as the OTGW32 / OT-Direct bench default. The pin resolver already
reads transient state, so no resolver change is needed and the system
self-corrects.

Chosen. It removes the conflation at its source (AUTO never touches the
override field), is self-healing against transient probe misses, and keeps
`iBoardMode` meaning exactly one thing: a user override.

### Alternative D: do nothing

Leave ADR-127 §3 as-is.

Rejected. That leaves the stranding bug in the field: one glitchy boot on a
real Classic+PIC board permanently mis-caches it as OT-Direct with manual
recovery only.

## Consequences

**Benefits**

- No sticky mis-cache: a transient `detectPIC()` miss self-corrects on the very
  next boot, because the next boot re-probes from scratch.
- Cleaner semantics: `settings.iBoardMode` now means exactly "user override"
  and nothing else. The UI and REST API never display an AUTO guess as if it
  were a user decision.
- No resolver change and no new state: `comboActivePinMap()` already derives the
  AUTO map from transient `state.hw.bClassicPro` and `state.hw.eMode`, so the
  fix is a deletion (the cache-back at `OTGW-firmware.ino:314, 320`), which
  lowers complexity rather than adding it. It is a net code reduction, so it does
  not grow the combo image, which ADR-127 measured at 98.4% of the 1.875 MB app
  slot (roughly 30 KB headroom).
- Manual force is still fully honoured for users who deliberately want to pin a
  mode (`iBoardMode == 1 / 2 / 3`).

**Trade-offs**

- The AUTO path pays a small per-boot cost: one `detectPIC()` ETX probe on
  every boot instead of one-and-then-cached. The maintainer explicitly chose
  re-probe over cache (TASK-947); the probe is a single short UART handshake
  during `setup()`, well inside the existing boot budget, and only the combo
  AUTO path pays it (fixed boards have `HAS_RUNTIME_HW_DETECT=0` and never
  probe).
- The bench default is a guess by construction: a no-PIC, no-OT-bus board is
  assumed OTGW32 / OT-Direct. This is acceptable because it is re-evaluated
  every boot and a user who needs a different mode sets the manual override.

**Risks and mitigations**

- *Risk*: a Classic board whose PIC is genuinely dead reads as OT-Direct on
  every boot. *Mitigation*: that is the correct AUTO answer for a board with no
  live PIC; the user can force PIC mode (`iBoardMode == 1` or `3`) if the PIC is
  expected to come back, and a repaired PIC is picked up automatically on the
  next boot.
- *Risk*: removing the cache could regress boot time if `detectPIC()` were slow.
  *Mitigation*: the probe is a single ETX handshake with a bounded timeout, and
  the maintainer accepted the per-boot cost as part of TASK-947.

## Related Decisions

- **ADR-127** (combo ESP32-S3 single binary, runtime PIC/OTDirect boot
  detection): amended. This ADR removes the ADR-127 §3 cache-back into
  `settings.iBoardMode` and the `iBoardMode == 2` AUTO fast path; ADR-127
  §7 (banner-recovery) becomes redundant for the AUTO case because every boot
  re-probes, but is left in place as a harmless second net.
- **ADR-158** (combo S3 Mini Pro as a third boot-detected Classic variant):
  the third pin map and `activePicRst` / `state.hw.bClassicPro` machinery this
  AUTO path resolves against; `iBoardMode == 3` ("force PIC S3 Mini Pro") is its
  manual-force value.
- **ADR-125** (combo board boot detection, floating-pin RX-storm guard): the
  `OTGWSerial.end()` call on the no-PIC path that this decision relies on.
- **ADR-126** (fixed esp32-classic build): the fixed targets that keep
  `HAS_RUNTIME_HW_DETECT=0` and therefore never run this AUTO path.
- **ADR-051** (settings vs transient state): the rule this ADR restores —
  `settings.iBoardMode` is persisted user config, `state.hw.eMode` and
  `state.hw.bClassicPro` are transient live state. The bug was a transient
  guess leaking into persisted config.
- **ADR-080** (gate policy; this ADR is guideline-level, see Status).

## References

- TASK-947 (implementation; follow-up to the combo PR #667 / ADR-127),
  commit `c3afa5f4`.
- `src/OTGW-firmware/OTGW-firmware.ino` — `setup()` hardware-mode resolution
  block: the AUTO branch (`iBoardMode == 0`) no longer writes
  `settings.iBoardMode`.
- `src/OTGW-firmware/OTGW-firmware.h` — `comboActivePinMap()`: the AUTO branch
  resolves the pin map from `state.hw.bClassicPro` and `state.hw.eMode`, so no
  resolver change is needed.
- `src/OTGW-firmware/OTDirect.ino` — `initOTDirect()`: sets `HW_MODE_OT_DIRECT`
  (the bench default reached when no live PIC is found).
- GitHub PR #667: https://github.com/rvdbreemen/OTGW-firmware/pull/667

## Enforcement

No declarative or LLM-judgeable enforcement block. The decision is a behavioural
contract on a single boot-time code path (the AUTO branch must not write
`settings.iBoardMode` and must re-probe each boot). It has no stable
regex/import surface that a diff-level judge could check without false positives,
and ADR-080 keeps it guideline-level: correctness is verified by code review and
on-device validation on both the Classic+PIC PCB and the OTGW32, not by an
automated gate. If AUTO-selection regressions reappear, the
`check_combo_runtime_selection` gate proposed in the Status section should be
added and this ADR promoted to binding.
