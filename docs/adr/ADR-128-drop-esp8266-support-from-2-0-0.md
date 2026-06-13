# ADR-128: Drop ESP8266 Support from the 2.0.0 Line (Supersedes ADR-082)

## Status

Accepted, 2026-06-12. Decision by the maintainer (Robert). On acceptance, ADR-082's
Status line becomes "Superseded by ADR-128, 2026-06-12" (only that line changes).

## Context

### Problem

The ESP8266 has been the project's platform for ~12 years and has reached its memory
ceiling: with ~40 KB usable RAM, every new 2.0.0 feature is a balancing act against
heap (see the long string of memory ADRs — ADR-004 static buffers, ADR-042 streaming
JSON, ADR-053 large-feature buffers, ADR-079/107 emergency heap recovery, ADR-121
per-consumer heap gating). The maintainer's announcement (Discord, June 2026) states
it plainly: *"we are at the end of what is possible with this 12 years old platform"*,
and on 2026-06-12 decided to **stop ESP8266 support to simplify the codebase**.

### Background — current state

2.0.0 today is dual-target. ADR-126 defines three fixed compile-time builds:
`esp8266` (OTGW Classic + Wemos D1 mini + PIC), `esp32` (OTGW32 board, ESP32-S3,
OTDirect), and `esp32-classic` (LOLIN S3 mini in the Classic socket). ESP8266 is held
in place by ADR-082 (Arduino Core 2.7.4 LTS pin), the unified platform abstraction
(ADR-061), the SAT compatibility layer (ADR-072), and the dual-target PlatformIO
build (ADR-083). `README.md` and `docs/MANUAL.md` describe ESP8266 as supported
"alongside" ESP32.

### Forces

- **Memory.** TLS, Bluetooth/BLE, the AsyncTCP stack and FreeRTOS multitasking all
  need headroom the ESP8266 does not have. The ESP32-S3 has ~320 KB+ RAM.
- **Maintenance.** Dual-target means platform-conditional code, two build envs and two
  regression surfaces. Concretely, ADR-123 (the 2.0.0 async concurrency model) would
  otherwise have to carry a *permanent* platform-divergent cooperative loop for
  ESP8266 — the single biggest complexity it now sheds.
- **A migration path already exists.** The LOLIN/Wemos **S3 mini** and **S3 mini pro**
  (ESP32-S3) are pin-compatible with the Wemos D1 mini footprint, a drop-in board swap
  on existing Nodoshop OTGW hardware (the `esp32-classic` build, ADR-126; mapping in
  `docs/hardware/PINOUT.md`). BLE is already adopted via NimBLE (ADR-092).
- **Breaking change by design.** This breaks 1.x.x compatibility; the 1.x ESP8266
  release line remains the migrate-from baseline for users who do not move.

### Constraints

- ADR-082/061/072/083/126 are Accepted and immutable — they change only by
  supersession, never by editing their bodies.
- `README.md` / `docs/MANUAL.md` "alongside ESP8266" wording must be reconciled after
  acceptance.

## Decision

**Stop supporting ESP8266 on the 2.0.0 line. 2.0.0 targets the ESP32-S3 only** —
`esp32` (OTGW32) and `esp32-classic` (S3 mini drop-in in the Classic socket). Remove
the `esp8266` PlatformIO env and the ESP8266 code paths; the platform abstraction
stays but collapses to a single platform, and the dual-target build becomes
single-target.

This ADR **supersedes ADR-082** and **reshapes ADR-061, ADR-072, ADR-083, and the
ESP32-S3 build topology (ADR-126, as revived by ADR-127 — combo single binary)**.
Dropping the `esp8266` board leaves the ESP32-S3 builds (`esp32`, `esp32-classic`,
`esp32-combo`) intact; ADR-127's combo binary is unaffected since it is already
ESP32-S3-only. The 1.x release line remains available as the ESP8266 migrate-from
baseline.

### Scope and sequencing

This ADR is the decision record only. The actual code removal — the `esp8266` build
env, ESP8266 branches behind the platform abstraction, cooperative-loop-only code, and
retirement of ESP8266-only ADRs — is a phased implementation tracked as follow-up
backlog tasks, gated on this ADR's acceptance. ADR-123 (2.0.0 concurrency model)
depends on this ADR.

## Alternatives Considered

### Alternative 1 — Keep dual-target (status quo, ADR-082)
**Pros:** no migration for ESP8266 users; honours the LTS pin.
**Cons:** caps features at the ESP8266's RAM; forces a permanent platform-divergent
concurrency model (cooperative loop alongside the ESP32 async/FreeRTOS path, per
ADR-123); two build and regression surfaces.
**Rejected:** the maintenance and memory cost is exactly what this decision eliminates.

### Alternative 2 — Keep ESP8266 building but freeze it (legacy/EOL, feature-frozen)
Build `esp8266` but ship no new features there; new work is ESP32-only.
**Pros:** existing ESP8266 users keep a working 2.0.0 build a while longer.
**Cons:** still pays the dual-build/divergence tax (every refactor must keep `esp8266`
compiling); the support promise is ambiguous; the abstraction stays bimodal.
**Rejected:** retains the tax without the benefit — a clean cut is simpler and clearer.

### Alternative 3 — Stay on ESP8266 and optimise memory further
Squeeze more headroom out of the ESP8266 instead of moving platforms.
**Pros:** no hardware change for users.
**Cons:** diminishing returns after years of optimisation; cannot deliver TLS/BLE/async
at ESP8266 RAM; "the end of what is possible" per the maintainer.
**Rejected:** the platform is out of headroom.

## Consequences

### Positive
- **Simpler codebase:** one platform, one build, one regression surface; no
  platform-conditional concurrency (directly enables ADR-123's single code path).
- **New capabilities unlocked:** TLS, BLE (ADR-092), dual-core/FreeRTOS, async
  webserver + MQTT (ADR-123) without ESP8266 constraints.
- **Memory headroom:** ~320 KB+ RAM relaxes the strict static-buffer / `String`-
  prohibition posture (ADR-004/042/053) on the surviving platform.

### Negative
- **Breaking change:** ESP8266 OTGW users must swap to an ESP32-S3 board to run 2.0.0;
  boards without soldered headers need headers added.
- **1.x upkeep:** the 1.x line must stay available (and minimally maintained) as the
  migrate-from baseline.
- **Documentation/ADR churn:** README/MANUAL "alongside" wording and the platform ADRs
  (061/072/083/126) need follow-up reconciliation.
- **TLS still gated:** TLS becomes feasible on ESP32-S3 but remains blocked by ADR-003
  (HTTP-only) and the `CLAUDE.md` "Never add HTTPS/WSS" rule until/unless separately
  superseded.

### Risks & Mitigation
- **Risk:** users with no migration path feel stranded.
  **Mitigation:** the 1.x line stays as the baseline; document the drop-in S3 mini swap
  (`docs/hardware/PINOUT.md` / `esp32-classic`).
- **Risk:** hidden ESP8266 assumptions surface during removal.
  **Mitigation:** phased removal with a CI build-green gate at each step.

## Related Decisions

- **Supersedes:** ADR-082 (ESP8266 Arduino Core 2.7.4 LTS pin).
- **Reshapes:** ADR-061 (unified abstraction → single platform), ADR-072 (SAT compat
  layer), ADR-083 (dual-target build → single-target); drops the `esp8266` fixed build
  from the ESP32-S3 build topology (ADR-126 / ADR-127 combo).
- **Coexists with:** ADR-127 (combo ESP32-S3 single binary) — already ESP32-S3-only,
  unaffected by removing the `esp8266` board.
- **Depended on by:** ADR-123 (2.0.0 concurrency model).
- **Related:** ADR-092 (NimBLE/BLE), ADR-003 (HTTP-only; TLS flagged), ADR-013
  (Arduino framework over ESP-IDF), ADR-001 (original ESP8266 platform selection).
- **Research input:** `docs/research/2.0.0-async-modernization.md`.

## References

- Maintainer announcement (Discord, June 2026) + decision 2026-06-12 "stop ESP8266 support".
- LOLIN S3 mini: <https://www.wemos.cc/en/latest/s3/s3_mini.html>
- LOLIN S3 mini pro: <https://www.wemos.cc/en/latest/s3/s3_mini_pro.html>
- `docs/hardware/PINOUT.md` — S3 mini pin-compatibility with the Wemos D1 mini footprint.

## Enforcement

Manual review only at the decision stage. The mechanical boundary — removal of the
`esp8266` PlatformIO env and ESP8266 code paths — will be enforced by CI build
configuration once the follow-up removal tasks land. No declarative `Enforcement`
block at this stage.
