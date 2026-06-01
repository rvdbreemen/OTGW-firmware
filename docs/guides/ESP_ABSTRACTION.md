# ESP8266 / ESP32 Platform Abstraction — Consolidated Reference

> One map for the whole abstraction. This guide does **not** introduce new
> policy; it points at the authoritative pieces (ADRs, the CI gate, the audit,
> the allowlist) so a contributor can find everything from one place. When this
> guide and an ADR disagree, the ADR wins.

## TL;DR (the one rule)

Application code (`.ino` files and most `.h`) **never** uses raw
`#if(def) ESP8266` / `ESP32` / `ARDUINO_ARCH_ESP*` / `BOARD_NODOSHOP_*`.
Instead it:

1. **Calls `platformXxx()` shims** for any per-platform API divergence, and
2. **Gates optional features with `HAS_*` capability flags** from `boards.h`.

If a divergence has no shim yet, you add the shim (in *both* platform headers,
real body on one, inline stub on the other) **first**, then call it unguarded.
If a feature has no `HAS_*` flag yet, you add the flag to `boards.h` first.

## The decision (ADRs)

| ADR | What it decides |
|---|---|
| **ADR-061** | **The primary one.** Unified ESP8266/ESP32 platform abstraction: the 3-header design (`platform.h` dispatcher + `platform_esp8266.h` / `platform_esp32.h` shims), `boards.h` pin/flag selection, inline zero-cost shims, `platformXxx()` naming, `HAS_*` + type-alias flags. Supersedes ADR-001. Read this first. |
| **ADR-115** | Per-board **numeric constants + typedefs** live in `boards.h` (where the TASK-743 SAT tuning constants + ring-index typedef went). |
| **ADR-113** | Hardware-type codepath selection contract. |
| **ADR-114** | OLED runtime detection decoupled from the board flag. |
| **ADR-072** | SAT platform-compatibility layer. |
| **ADR-063** | OTGW32 hardware support decision. |
| **ADR-082** | ESP8266 Arduino Core 2.7.4 LTS pin. |

## The files (the abstraction layer itself)

These are the **only** places raw platform/board conditionals belong:

- `src/OTGW-firmware/platform.h` — dispatcher (`#ifdef ESP8266` → includes the right header).
- `src/OTGW-firmware/platform_esp8266.h` — ESP8266 includes, shims, type aliases (~35+ shims).
- `src/OTGW-firmware/platform_esp32.h` — ESP32 includes, shims, type aliases (parity set).
- `src/OTGW-firmware/boards.h` — pin maps + `HAS_*` capability flags; reads the raw `BOARD_NODOSHOP_*` macros so nothing else has to.
- `src/OTGW-firmware/OTGW-ModUpdateServer{.h,-esp32.h,-impl.h}` — parallel mini-abstraction for the firmware-update server.

**Out of scope entirely:** vendored libraries under `src/libraries/**`
(e.g. `OTGWSerial`, `SimpleTelnet`). They manage their own platform support and
are listed in `evaluate.py::ESP_ABSTRACTION_EXCLUDED_LIB_DIRS`.

## Capability flags (`boards.h`)

Gate optional features on the *capability*, never the platform:
`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`,
`HAS_SAT_BLE`, `HAS_WEATHER_FORECAST`, `HAS_BYPASS_RELAY`, … The raw board
macros decide which `HAS_*` are set; the rest of the firmware sees only `HAS_*`.

## The enforcement (CI gate)

ADR-061 states the rule; **`evaluate.py::check_esp_abstraction_boundary()`**
enforces it, per ADR-080 (binding rules need a CI gate). It is a
**baseline-ratchet**:

- `ESP_ABSTRACTION_BASELINE` in `evaluate.py` is the maximum tolerated count of
  platform-conditional sites outside the allowlisted files (currently **16**).
- A **new** violation above the baseline → **FAIL**.
- Any pre-existing violation remaining → **WARN** (target is 0).
- **Each remediation tier task must lower the baseline** as part of its DoD.

So the number only ever goes down. To see the current live count:
`python evaluate.py --quick` → the "ESP Abstraction Boundary" line.

## The roadmap (audit + tier tasks)

- **Audit:** `docs/audits/2026-05-28-esp-abstraction-leak-audit.md` (TASK-739) —
  the original leak inventory + remediation plan.
- **Tier tasks:** TASK-740..746 — each removes a class of leak and lowers the
  baseline. TASK-743 (Tier 3) moved tuning constants to `boards.h` and added
  the platform shim set; later chunks took the baseline to 16.
- **Known remaining leaks** (documented, deferred — see the tier-task notes):
  the `jsonStuff.ino` REST-TX coalescing buffer (highest-risk chunk), the
  `MQTTstuff.ino` heap-pressure pair (couples to app-side `getHeapHealth`),
  `SATmqttPublish.h` `os_random`/`esp_random` (include-order: the header loads
  before `platform.h`), and the `helperStuff.ino` min-free-heap watermark
  global (a one-definition-rule guard that is structurally necessary, not a
  removable leak).

## Adding a platform-divergent feature — the checklist

1. Need a divergent API? Add `platformXxx()` to **both** `platform_esp8266.h`
   and `platform_esp32.h` (real body where the feature exists, inline no-op stub
   on the other). Call it **unguarded** from application code.
2. Need an optional feature? Add a `HAS_*` flag to `boards.h`, gate with
   `#if HAS_*` (not `#ifdef ESP32`).
3. Never call `ESP.getXxx()` directly when a `platformXxx()` shim exists
   (`platformFreeHeap`, `platformRestart`, `platformChipId`, …) — skipping the
   shim is a quieter form of the same leak.
4. Build **both** targets (`python build.py`, check per-env SUCCESS lines) and
   run `python evaluate.py --quick` — the gate must not regress.

## Related

- ADR-061 (primary), ADR-072/113/114/115, ADR-063, ADR-082, ADR-080 (gate meta-rule).
- `evaluate.py::check_esp_abstraction_boundary()` + `ESP_ABSTRACTION_BASELINE`.
- `docs/audits/2026-05-28-esp-abstraction-leak-audit.md` + TASK-739..746.
- The CLAUDE.md "ESP Platform Abstraction" section (the operational quick-rules).
