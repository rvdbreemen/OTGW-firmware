# ADR-169 PSRAM-Aware BLE Default with a No-PSRAM Instability-Consent Gate

## Status

Accepted. Date: 2026-07-09.

This ADR documents an architectural decision that already shipped as code in
TASK-995 (commit 2f3bd23d3). Accepted by the maintainer on 2026-07-09. The
open PSRAM real-utility validation (TASK-988) does not block acceptance of the
consent-gate policy itself; it may lead to a later amending ADR if the
~64 KB / PSRAM-headroom premise is measured to differ materially.

## Status History

status_history:
  - date: 2026-07-09
    status: Proposed
    changed_by: Agent (adr-generator, on maintainer instruction)
    reason: Documents the code-only decision introduced by TASK-995 (commit 2f3bd23d3): BLE runs by default on PSRAM boards and stays dormant behind a persisted user-consent flag (bBleRiskAck) on no-PSRAM boards, with psramFound() as the single runtime discriminator across one combined firmware image. Status intentionally stays Proposed pending the maintainer's own accept-readiness review.
    changed_via: adr-kit
  - date: 2026-07-09
    status: Accepted
    changed_by: User: Robert van den Breemen
    reason: Maintainer accepted the PSRAM-aware BLE default + no-PSRAM consent-gate policy. All four verification gates pass (Completeness/Evidence/Clarity/Consistency); the decision matches shipped code (SATble.ino:629-631). TASK-988's PSRAM real-utility validation stays open as a possible later amendment, not a blocker on the policy.
    changed_via: adr-kit

## Context

The `dev` (2.0.0) firmware ships as one combined image across the ESP32-S3
board family. There is no build-time board split for the BLE-sensor feature:
the PSRAM-init build flag `-DBOARD_HAS_PSRAM` is defined once in the GLOBAL
`[env]` of `platformio.ini` (line 109), not per board, precisely so that
`psramFound()` becomes the sole runtime discriminator (`platformio.ini`
lines 100-109: "Putting it in the GLOBAL `[env]` (not per-board) means PSRAM
is DETECTED AT RUNTIME on every S3 build"). A board without PSRAM still boots
gracefully because the sdkconfig variant leaves PSRAM BOOT_INIT off, so
`psramInit()` returns false rather than aborting.

The BLE-sensor scanner (NimBLE, ADR-092) carries roughly 64 KB of internal-DRAM
footprint at runtime. On a no-PSRAM board (e.g. the OTGW32) that internal DRAM
is the same scarce pool the concurrent web-serve path depends on. Two Accepted
ADRs establish that headroom as a hard platform constraint on the ESP32-S3:

- **ADR-147** ("ESP32-S3 platform limits for concurrent webui static-file
  serving", Accepted 2026-06-19) guards the file-serve path and adopts the
  AsyncTCP config block because concurrent static serving already pushes the
  internal-DRAM/heap budget.
- **ADR-149** ("Accept the LWIP TCP-pcb connection ceiling on the ESP32-S3",
  Accepted 2026-06-20) keeps app-level connection mitigations rather than
  raising the LWIP ceiling, again to stay within the internal-memory budget.

Adding BLE's ~64 KB internal-DRAM claim on top of that budget on a no-PSRAM
board risks heap exhaustion and instability. On a PSRAM board the 2 MB of
external RAM (verified on device this week on the S3 Mini Pro / combo:
`psram_found=1`, `psram_size=2097152`) frees the internal room the web stack
needs, so BLE and the web stack coexist safely.

TASK-995 therefore made BLE activation depend on the board's memory class at
runtime, while preserving user agency on no-PSRAM boards through an explicit,
persisted consent flag. TASK-988 (validate PSRAM real utility) remains the open
follow-up that must confirm the PSRAM assumption before this ADR is accepted.

## Decision

BLE runs by default only on boards where `psramFound()` is true; on no-PSRAM
boards it stays dormant behind a persisted user-consent flag (`bBleRiskAck`)
that acknowledges the internal-DRAM-headroom instability risk, with
`psramFound()` as the single runtime discriminator across one combined firmware
image.

Concretely, every BLE activation site funnels through one gate
(`SATble.ino:629-631`):

```cpp
static inline bool bleActive() {
  return settings.sat.bBleEnable && (psramFound() || settings.sat.bBleRiskAck);
}
```

`bBleEnable` defaults to `true` (`SATtypes.h:512`; BLE sensors are sensors, so
they scan by default). The new flag `bBleRiskAck` defaults to `false`
(`SATtypes.h:513`) and is persisted under the LittleFS key `SATbleriskack`
(written `settingStuff.ino:411`, parsed `settingStuff.ino:1121`) and exposed on
the REST settings surface as `satbleriskack` (`restAPI.ino:3751`). The BLE
status JSON also reports the live decision (`psram`, `ble_enable`, `risk_ack`,
`active`) at `SATble.ino:1089-1092` so the Web UI can surface the consent
dialog and reflect the runtime outcome.

The scope is deliberately narrow: this decision governs whether BLE scanning is
allowed to run, not how BLE decodes frames or manages its roster (ADR-092,
ADR-151, ADR-153, ADR-161). It introduces one persisted setting, one
user-consent contract, and one memory-headroom policy tied to the ADR-147 /
ADR-149 concurrent-serve budget.

## Alternatives Considered

### Alternative A: Enable BLE unconditionally on all boards

Drop the PSRAM check and run BLE wherever `bBleEnable` is true.

Rejected: no-PSRAM boards (OTGW32) lack the internal-DRAM headroom to carry
BLE's ~64 KB footprint alongside the ADR-147 / ADR-149 concurrent web-serve
path. The result is heap exhaustion and instability under nominal concurrent
web + BLE load. The whole reason ADR-147 and ADR-149 exist is that the internal
budget on the S3 is already tight; BLE would be the allocation that pushes it
over.

### Alternative B: Disable BLE entirely on no-PSRAM boards, with no consent path

Gate purely on `psramFound()` and give no-PSRAM users no way to turn BLE on.

Rejected: this removes user agency. Some users may knowingly accept the
instability risk to get the feature on a no-PSRAM board (e.g. a low-traffic
install that rarely serves the Web UI concurrently). A consent gate preserves
that choice without making it the default, so the safe behavior is automatic
and the risky behavior is deliberate and reversible.

### Alternative C: Separate firmware builds per board (PSRAM vs no-PSRAM)

Ship one firmware image with BLE enabled for PSRAM boards and a separate image
with BLE disabled for no-PSRAM boards.

Rejected: the project ships one combined image and uses runtime hardware
detection (`-DBOARD_HAS_PSRAM` in the global `[env]`; `psramFound()` as the sole
discriminator). A build split multiplies release artifacts, forces users to
match a firmware to their exact board, and contradicts the combined-single-image
approach (ADR-126). Runtime detection means a future OTGW32 revision that ships
with PSRAM auto-enables BLE with the same firmware, no rebuild.

## Consequences

**Benefits**

- Safe default per board class with zero user configuration on PSRAM boards:
  BLE just works when enabled, because the external RAM frees the internal
  headroom the web stack needs.
- Preserved user choice on no-PSRAM boards via an explicit, reversible consent
  flag rather than a hard lockout.
- One combined firmware image and one runtime discriminator (`psramFound()`);
  no board-specific builds, no per-board release artifacts.
- The runtime gate future-proofs the decision: a later no-PSRAM board revision
  that adds PSRAM auto-enables BLE with no code or build change.

**Trade-offs**

- A no-PSRAM user who wants BLE must find and accept the consent dialog in the
  Web UI. The feature is not discoverable by simply flipping `bBleEnable`.
- The instability risk on a no-PSRAM board is real, not theoretical: it is the
  same internal-DRAM budget that ADR-147 / ADR-149 already had to defend.
- One more persisted setting and one more user-facing contract to maintain.

**Risks and mitigations**

- *Risk*: a no-PSRAM user accepts the consent flag and still hits instability
  under heavy concurrent web + BLE load. *Mitigation*: the consent dialog states
  the risk explicitly, the setting is persisted and reversible (set
  `satbleriskack` back to false to return to the safe dormant state), and the
  BLE status JSON exposes the live `active` decision so the condition is
  observable.
- *Risk*: a genuinely PSRAM-less S3 fails to boot gracefully with
  `-DBOARD_HAS_PSRAM` set globally. *Mitigation*: BOOT_INIT is left off in the
  sdkconfig variant, so `psramInit()` returns false instead of aborting; the
  ship-gate (confirm graceful boot on a real PSRAM-less S3) is called out in
  `platformio.ini` and must pass before acceptance.
- *Risk*: the PSRAM "frees internal headroom" assumption is not validated in
  practice. *Mitigation*: TASK-988 is the open follow-up that must confirm PSRAM
  real utility before this ADR flips to Accepted.

## Related Decisions

- **ADR-147 (ESP32-S3 platform limits for concurrent webui static-file
  serving)**: establishes the internal-DRAM / heap budget on the concurrent
  web-serve path that BLE's ~64 KB footprint would compete with. This ADR's
  memory-headroom policy is a direct consequence.
- **ADR-149 (Accept the LWIP TCP-pcb connection ceiling on the ESP32-S3)**:
  the companion platform-limit ADR; both together define why a no-PSRAM board
  cannot safely afford BLE alongside the web stack.
- **ADR-092 (Adopt NimBLE-Arduino over Bluedroid for the SAT BLE scanner)**:
  the BLE stack whose runtime footprint this gate protects against.
- **ADR-126 (single combined firmware image / runtime hardware detection)**:
  the combined-image and runtime-detection approach that Alternative C
  contradicts and that this decision relies on.
- **ADR-161 (Dedicated REST endpoint for the SAT BLE sensor roster)**:
  complements this decision at the roster layer; unaffected by the activation
  gate.
- **TASK-995 (commit 2f3bd23d3)**: the implementing change documented here.
- **TASK-988 (validate PSRAM real utility)**: the open follow-up that must
  confirm the PSRAM assumption before acceptance.

## References

- `src/OTGW-firmware/SATble.ino:623-631` — the `bleActive()` gate and its
  explanatory comment.
- `src/OTGW-firmware/SATble.ino:636`, `:713`, `:931` — the activation sites that
  funnel through `bleActive()`.
- `src/OTGW-firmware/SATble.ino:1089-1092` — BLE status JSON (`psram`,
  `ble_enable`, `risk_ack`, `active`).
- `src/OTGW-firmware/SATtypes.h:512-513` — `bBleEnable` (default true) and
  `bBleRiskAck` (default false) storage.
- `src/OTGW-firmware/settingStuff.ino:411` (write), `:1121` (parse) — persisted
  key `SATbleriskack`.
- `src/OTGW-firmware/restAPI.ino:3751` — REST key `satbleriskack`.
- `platformio.ini:100-109` — global `-DBOARD_HAS_PSRAM` rationale and the
  runtime-detection / graceful-boot / ship-gate note.
- Commit `2f3bd23d3` (TASK-995).

## Enforcement

Declarative: the single-point activation gate must keep `psramFound()` and the
`bBleRiskAck` consent flag as the discriminators, so a diff that silently drops
either (re-enabling BLE unconditionally on no-PSRAM boards) is caught
deterministically.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {"pattern": "psramFound\\(\\) \\|\\| settings\\.sat\\.bBleRiskAck",
     "path_glob": "src/OTGW-firmware/SATble.ino",
     "message": "The BLE activation gate (bleActive) must keep psramFound() || bBleRiskAck as the no-PSRAM consent discriminator (ADR-169). Do not re-enable BLE unconditionally on no-PSRAM boards."}
  ],
  "llm_judge": false
}
```
