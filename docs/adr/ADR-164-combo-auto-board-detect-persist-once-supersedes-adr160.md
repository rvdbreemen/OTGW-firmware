---
id: ADR-164
title: "Combo AUTO board detection persists its PIC verdict once (supersedes ADR-160)"
status: Accepted
date: 2026-07-04
tags: [board-detection, combo, otgw32, esp32s3, pic, boot, reliability, i2c]
supersedes: [ADR-160]
superseded_by: []
related: [ADR-126, ADR-127, ADR-158, ADR-160]
deciders: [Robert van den Breemen]
---

# ADR-164: Combo AUTO board detection persists its PIC verdict once (supersedes ADR-160)

## Status

Accepted, 2026-07-04 (authored and accepted the same day by the maintainer Robert van den
Breemen, who directed the reversal). This ADR documents the board-detection behaviour that
is already shipped and on-device-verified on the 2.0.0 `dev` line; it reverses the
never-persist rule that ADR-160 recorded.

**Guideline-level** (per ADR-080): this is a structural / boot-sequencing decision about
one board-detection path, not a code-pattern rule. It has no automated `evaluate.py` gate;
the invariant is verified by code review and on-device serial observation.

**Supersedes ADR-160** ("Combo board: AUTO hardware detection re-probes every boot and never
persists its verdict"). ADR-160 stays immutable; a "Superseded by ADR-164" status line is
added to it (the sanctioned immutability exception), its body unedited.

## Status History

status_history:
  - date: 2026-07-04
    status: Proposed
    changed_by: Agent
    reason: ADR-160's never-persist rule was implemented (TASK-947, c3afa5f4f), regressed a plain S3 Mini's ability to boot Classic (TASK-949, 7f69f6b91), and the attempted fix hung boot on-device and was urgently reverted (d7a34f4ad) back to persist-once with a 3x detectPIC retry. Shipped code diverged from ADR-160; this ADR realigns the record with reality.
    changed_via: adr-kit
  - date: 2026-07-04
    status: Accepted
    changed_by: maintainer (Robert van den Breemen)
    reason: Accepted after code-implementation verification (persist-once + 3x retry live at OTGW-firmware.ino:341-405) on maintainer instruction in-session, choosing a superseding ADR over an in-place amendment for a genuine decision reversal.
    changed_via: manual

## Context

ADR-160 decided that on the runtime-detecting combo board, AUTO detection (`iBoardMode == 0`)
would re-probe the PIC on **every** boot and **never** persist its verdict, removing the
ADR-127 section 3 cache-back. The intent was to stop a single transient `detectPIC()` miss
from permanently stranding a real OTGW Classic board in OT-Direct mode.

That rule was implemented (TASK-947, commit `c3afa5f4f`) and then broke a different case:
on a plain LOLIN S3 Mini (not the Pro), the always-run `probeProImu()` brought up I2C on the
Pro pin map (GPIO 11/12) before `readSettings()`. On a plain S3 Mini, GPIO 12 is `PIC_RST`,
so probing disturbed the PIC reset on every boot and the board could no longer boot Classic
(TASK-949). The fix attempt (`7f69f6b91`) reordered settings loading to the top of `setup()`
and gated the IMU probe to AUTO only, but that reorder **hung the boot** on-device (app
entered, no banner, no AP) and had already shipped to `dev`, forcing an urgent revert
(`d7a34f4ad`) back to the alpha.288 logic: **persist-once** detection with a **3x
`detectPIC()` retry** in the AUTO branch.

The net result is that shipped code implements persist-once, while ADR-160 still states
never-persist. This ADR reconciles the record with the shipped, field-validated behaviour
and captures why never-persist did not survive contact with the hardware boot order.

## Decision

On the combo board, AUTO board detection (`iBoardMode == 0`) **detects once on first boot and
persists its verdict once** into the `iBoardMode` setting, rather than re-probing every boot:

1. **First-boot detection with retry.** In the AUTO branch, `detectPIC()` is retried up to
   3 times before concluding the PIC is absent. `detectPIC()` performs its own clean PIC
   reset, which also recovers from the transient the early `probeProImu()` IMU probe can
   leave on GPIO 12. The retry is the mitigation ADR-160 was trying to achieve by re-probing
   every boot, without the every-boot cost or the boot-order hazard.
2. **Persist once.** Once AUTO resolves a verdict it writes it into `iBoardMode`
   (`3` = Classic on LOLIN S3 Mini Pro, `1` = Classic on plain S3 Mini, `2` = OTGW32 /
   OT-Direct) via a single `writeSettings(false)`. Subsequent boots take the deterministic
   forced path for that mode and do not re-run AUTO.
3. **Manual override is unchanged.** An explicit `iBoardMode` of `1`, `2`, or `3` is honoured
   directly and never runs AUTO (as in ADR-158). `iBoardMode` remains a soft setting: the
   user can reset it to `0` to force a fresh AUTO detection at the next boot.

This reverses ADR-160's "never persists" clause. It keeps ADR-158's `iBoardMode` semantics
(`0` auto, `1` PIC S3 Mini, `2` OT-Direct, `3` PIC S3 Mini Pro) and ADR-126's fixed-target
model (fixed `esp32` / `esp32-classic` targets do no runtime detection at all; only the combo
binary does).

## Consequences

**Positive**
- Deterministic, boot-order-safe behaviour: after first boot the mode is fixed, so the
  fragile interaction between the IMU probe, the Pro pin map, and PIC reset cannot recur on
  later boots.
- Cheaper steady-state boot (no PIC probe every boot).
- The 3x retry absorbs a transient first-boot `detectPIC()` miss, addressing the failure mode
  ADR-160 targeted.

**Negative / mitigations**
- A wrong verdict persisted on first boot sticks until the user resets `iBoardMode` to `0`.
  Mitigated by the 3x retry (a single transient miss no longer decides) and by `iBoardMode`
  being a user-visible soft setting that can be re-armed to AUTO.
- Persisting into `iBoardMode` conflates the auto verdict with a user force in that one
  setting; this is the accepted trade for determinism and is documented at the write site
  (`OTGW-firmware.ino`).

## Related / References

- **Supersedes:** ADR-160 (combo AUTO never-persists).
- **Related:** ADR-126 (fixed build targets, no runtime detection on fixed targets),
  ADR-127 (persist the boot hardware-detection result), ADR-158 (S3 Mini Pro as a third
  boot-detected Classic variant, `iBoardMode` semantics).
- **Commits:** `c3afa5f4f` (TASK-947, never-persist), `7f69f6b91` (TASK-949, settings-first
  reorder attempt), `d7a34f4ad` (urgent revert to persist-once + 3x retry).
- **Code:** `OTGW-firmware.ino` AUTO branch (first-boot detection, 3x `detectPIC()` retry,
  persist-once `writeSettings`), `probeProImu()`, `comboActivePinMap()`, `activePicRst()`.
- Surfaced by the 2026-07-04 dev-2.0.0 commit audit (TASK-1000).
