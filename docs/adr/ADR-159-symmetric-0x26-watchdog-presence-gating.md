# ADR-159 Symmetric Presence-Gating of the External 0x26 Watchdog (Amends ADR-135)

## Status

Proposed, 2026-06-29.

This ADR is **guideline-level** per ADR-080: it does not introduce a new
`evaluate.py` gate. The invariant it protects (arm and feed gated by the same
runtime presence flag) is a semantic correctness property of three small
functions in one file, verified by code review and on-device serial observation,
not by a static pattern match. See the Enforcement note for why a declarative
gate would be unsound here.

It **amends ADR-135** (TWDT primary, external 0x26 optional secondary) and
**supersedes the 2026-06-14 "always feed/arm unconditionally" directive** recorded
in ADR-135 Decision §3 and its Status History. ADR-135's core architecture (the
ESP32 TWDT is the always-on primary on every build; the 0x26 chip is an optional
secondary present only on the OTGW Classic PCB) is unchanged. Only the rule for
*how* the secondary 0x26 chip is engaged changes: from unconditional arm+feed to
symmetric presence-gated arm+feed.

## Status History

status_history:
  - date: 2026-06-29
    status: Proposed
    changed_by: Agent
    reason: Replace ADR-135's unconditional 0x26 arm+feed with symmetric presence-gating (both arm and feed gated on a single retried boot probe into s_extWdPresent; disarm stays unconditional). Fixes the ESP32-S3 esp32-hal-i2c-ng log storm from feeding an absent chip (TASK-945) without re-introducing the armed-but-unfed spurious-reset hazard ADR-135 warned against (the asymmetry left by TASK-945, closed by TASK-946). Guideline-level per ADR-080; amends ADR-135.
    changed_via: adr-kit

## Context

ADR-135 established the ESP32 Task Watchdog Timer (TWDT) as the always-on primary
watchdog on the 2.0.0 ESP32-S3 line and demoted the external I2C watchdog chip at
address `0x26` (on `HAS_PIC_WATCHDOG` boards: `esp32-classic`, and combo when
booted into PIC mode) to an optional secondary. To close an asymmetry hazard,
ADR-135 Decision §3 mandated that the 0x26 arm, feed, and boot-status read all run
**unconditionally** on every `HAS_PIC_WATCHDOG` build, never gated on a probe.

### The premise that turned out to be false

That unconditional rule rested on one explicit premise (ADR-135 §3, and the
original in-code comment in `OTGW-Core.ino`): writing to an absent 0x26 is a
**harmless silently-NACKed no-op** on floating I2C pins. On the ESP8266 Wire
implementation that premise held. On the ESP32-S3 it does not. The
Arduino-ESP32 `esp32-hal-i2c-ng` driver does not return quietly when no device
ACKs: it logs an ARDUHAL `[E]` `i2c_master_transmit` error on **every** failed
write. Because `feedWatchDog()` writes the feed byte at up to 10 Hz (the 100 ms
rate limit), an absent 0x26 turned the secondary feed into a continuous error log
storm. (ARDUHAL is the Arduino-ESP32 hardware-abstraction logging layer; IDF is
the ESP-IDF Espressif IoT Development Framework underneath it.)

This was observed on real hardware: a classic-S3 build with no responding 0x26
chip (MAC AC:27:6E:CE:45:D8) produced **97 `i2c_master_transmit` errors in 10
seconds** on the serial console, flooding the log and obscuring all other
diagnostics.

### The first fix, and the regression it left

TASK-945 (commit 48daa364) stopped the spam by adding a single-shot boot probe of
0x26 into a file-static `s_extWdPresent` and gating **only the periodic feed** on
it. That silenced the storm, but an aggressive code review found it had
re-introduced the exact failure mode ADR-135 §3 was written to prevent. The
**arm** path (`WatchDogEnabled(1)`) was still unconditional while the feed was now
gated. If a 0x26 chip is physically **present** but its boot probe
false-negatives (a transient bus glitch, slow power-up, marginal solder), the
chip gets **armed but never fed**. An armed-but-unfed hardware watchdog bites: it
issues a true hardware reset of the ESP. If the probe diverges only
intermittently, the result is a spurious reset; if it diverges permanently, a
reset loop. This is the armed-but-unfed asymmetry, just inverted from the one
ADR-135 fixed.

### Forces

- The 0x26 ARDUHAL `[E]` log line is emitted inside the vendored
  `esp32-hal-i2c-ng` driver and is compile-gated by `CORE_DEBUG_LEVEL`, not
  runtime-suppressible per tag. `esp_log_level_set("i2c.master", ...)` does not
  silence the ARDUHAL line. So the only way to stop the spam at runtime is to
  **not write** to an absent chip.
- An armed external hardware watchdog that is not fed will reset the board. The
  arm and feed decisions must therefore agree on whether the chip is engaged. Any
  state where the chip is armed but not fed is a latent spurious-reset bug.
- The TWDT remains the always-on primary on every build (ADR-135). A 0x26 that is
  not engaged for one boot costs only the optional secondary layer for that boot;
  software-independent recovery via the TWDT is never lost.

## Decision

**On every `HAS_PIC_WATCHDOG` build, the external 0x26 secondary watchdog is
presence-gated SYMMETRICALLY: both the periodic feed and the arm are gated on a
single runtime presence probe (`s_extWdPresent`); the disarm stays
unconditional.**

This amends ADR-135 Decision §3 and supersedes its unconditional arm+feed
directive. The TWDT-primary architecture from ADR-135 is otherwise unchanged.

Concretely, as built in TASK-946 (commit 5d6ac053) in
`src/OTGW-firmware/OTGW-Core.ino`:

1. **One retried boot probe sets `s_extWdPresent`.** `initWatchDog()` probes 0x26
   exactly once at boot. The boot status-read transaction doubles as the probe:
   `Wire.endTransmission() == 0` (the chip ACKed) means present. The probe is
   **retried up to 3 times** (with a 5 ms gap on miss) to cut the false-negative
   rate from a transient bus glitch or slow power-up. The result is stored in the
   file-static `bool s_extWdPresent` (default `false`).

2. **The feed is gated.** `feedWatchDog()` always resets the TWDT (the primary)
   when `s_twdtReady`, and writes the 0x26 feed byte (`0xA5`) **only if**
   `s_extWdPresent`. An undetected chip is never written to, so the
   `i2c_master_transmit` log storm cannot occur.

3. **The arm is gated, with the same flag.** `WatchDogEnabled(stateWatchdog)`
   arms the 0x26 chip (write `1` to register 7) **only if** `s_extWdPresent`:
   `if (stateWatchdog && !s_extWdPresent) return;`. Gating the arm with the same
   flag as the feed is the load-bearing change: an undetected chip (including a
   present chip that false-negatived the probe) is never armed, so it can never be
   left armed-but-unfed, so it can never bite.

4. **The disarm stays unconditional.** `WatchDogEnabled(0)` always writes `0` to
   register 7. Turning a present chip off is safe; an absent chip NACKs exactly
   once (not the 100 ms feed storm); and the boot-time disarm runs *before* the
   probe in the boot sequence, so it cannot depend on `s_extWdPresent`.

Symmetric gating is safe precisely because the only correctness hazard for a
hardware watchdog is "armed but not fed", and a chip that is never armed can never
reach that state. A rare false-negative now degrades gracefully: the chip is left
disarmed (the unconditional boot disarm already ran), the secondary layer is
skipped for that one boot, the TWDT covers the board as primary, and the chip is
re-probed on the next boot.

## Alternatives Considered

### Alternative A: Revert to unconditional feed (the ADR-135 directive) and suppress the log
Keep the unconditional arm+feed from ADR-135 §3 and silence the spurious 0x26
error line at runtime instead.
**Why rejected:** The ARDUHAL `log_e` in `esp32-hal-i2c-ng.c` fires on
`i2c_master_transmit` failure and is compile-gated by `CORE_DEBUG_LEVEL`. It is
not addressable by `esp_log_level_set("i2c.master", ...)`, which targets the
ESP-IDF `i2c.master` tag, not the ARDUHAL line. There is no runtime knob that
silences this line short of recompiling the core with a lower debug level (which
would also blind every other ESP error). The spam cannot be killed without
not-writing, so "suppress the log" is not actually available.

### Alternative B: Asymmetric gate (feed gated, arm unconditional)
Gate only the feed on `s_extWdPresent` and leave the arm unconditional. This is
exactly what TASK-945 shipped.
**Why rejected:** This is the regression. A present chip whose probe
false-negatives gets armed (unconditional) but never fed (gated off) and bites,
causing a spurious reset, or a reset loop if the probe diverges permanently. It
re-creates the armed-but-unfed hazard ADR-135 §3 was written to eliminate, just
from the opposite direction.

### Alternative C: Symmetric gate (chosen)
Gate both arm and feed on the same `s_extWdPresent`, keep the disarm
unconditional, retry the probe to reduce false negatives.
**Why chosen:** It kills the log storm (no writes to an absent chip) and closes
the armed-but-unfed hazard (an undetected chip is never armed). A false-negative
degrades to "no secondary this boot" with the TWDT still primary, which is the
benign direction.

### Alternative D: Drop the 0x26 chip entirely
Remove the secondary path and rely on the TWDT alone on all boards.
**Why rejected:** Already considered and rejected in ADR-135 Alternative 2. The
0x26 chip is a sanctioned defence-in-depth secondary physically present on real
OTGW Classic PCBs; it provides a true-hardware reset path that the TWDT's
panic-reset cannot fully replicate if the SoC wedges below the scheduler. This ADR
keeps that layer; it only changes how the chip is engaged.

## Consequences

**Benefits**

- The `i2c_master_transmit` log storm is eliminated on classic-S3 / combo boards
  with no responding 0x26: measured drop from 97 errors / 10 s to 0 errors / 11 s
  (alpha.287 to alpha.289), with a clean boot.
- No spurious-reset / reset-loop hazard: an undetected (or false-negatived) chip
  is never armed, so it can never be left armed-but-unfed and can never bite.
- Defence-in-depth is preserved on boards that have the chip: a detected 0x26 is
  both armed and fed exactly as ADR-135 intended.
- The TWDT remains the always-on primary on every build, so the worst case from a
  probe miss is loss of the optional secondary for one boot.

**Trade-offs**

- A present-but-mis-probed 0x26 loses its secondary watchdog coverage for that one
  boot (re-probed next boot). The TWDT covers the board in the meantime. The 3x
  retry makes a full miss unlikely but not impossible.
- The change adds a small amount of state and branching (one file-static flag, a
  retry loop, and two gate checks) to the watchdog functions versus the
  unconditional form.

**Risks and mitigations**

- *Risk:* the boot probe false-negatives a present chip, leaving the secondary
  layer off for that boot. *Mitigation:* the probe is retried up to 3 times with a
  5 ms gap; the TWDT remains primary; the chip is re-probed every boot so the
  condition is transient, not sticky.
- *Risk:* the few boot-time 0x26 I2C transactions (the unconditional disarm plus
  up to 3 probe attempts) themselves NACK on a board with no chip. *Mitigation:*
  these are a handful of one-time boot NACKs, not the 100 ms periodic feed; they do
  not constitute the log storm this ADR eliminates and are an acceptable cost of
  detection.

## Related Decisions

- **Amends:** ADR-135 (TWDT primary, external 0x26 optional secondary). The
  TWDT-primary architecture is unchanged; this ADR replaces ADR-135 Decision §3's
  unconditional 0x26 arm+feed with symmetric presence-gating, and supersedes the
  2026-06-14 unconditional-feed directive in ADR-135's Status History.
- **Coexists with:** ADR-127 (combo ESP32-S3 single binary, runtime PIC/OTDirect
  boot detection). The 0x26 presence probe is independent of the PIC/OTDirect boot
  mode: it is keyed on whether the 0x26 chip itself ACKs, never on `isPICEnabled()`
  (consistent with ADR-135 Decision §3, which kept the 0x26 path off the PIC-mode
  gate).
- **Governed by:** ADR-080 (binding ADR rules must have a CI gate). This ADR is
  explicitly labeled guideline-level, so per ADR-080 it carries no new
  `evaluate.py` gate; see the Enforcement note.

## References

### Contributing tasks and commits
- TASK-945 (initial spam fix: feed-only gate; commit 48daa364) — silenced the log
  storm but left the arm unconditional (the asymmetry this ADR closes).
- TASK-946 (symmetric-gating remediation; commit 5d6ac053) — gates both arm and
  feed on `s_extWdPresent`, keeps the disarm unconditional, retries the probe.

### Implementation files
- `src/OTGW-firmware/OTGW-Core.ino`:
  - `initWatchDog()` — single retried 0x26 probe (up to 3 attempts) into
    `s_extWdPresent`; the boot status-read doubles as the probe
    (`Wire.endTransmission() == 0` means present).
  - `feedWatchDog()` — TWDT reset always (when `s_twdtReady`); 0x26 feed (`0xA5`)
    gated on `s_extWdPresent`.
  - `WatchDogEnabled()` — arm (register 7, value 1) gated on `s_extWdPresent`
    (`if (stateWatchdog && !s_extWdPresent) return;`); disarm (value 0)
    unconditional.

### On-device evidence
- classic-S3 build, MAC AC:27:6E:CE:45:D8, alpha.287 to alpha.289:
  `i2c_master_transmit` errors fell from 97 / 10 s to 0 / 11 s, with a clean boot
  and no reset loop observed.

### External
- Arduino-ESP32 `esp32-hal-i2c-ng` driver (`log_e` on `i2c_master_transmit`
  failure, the source of the per-write `[E]` log line):
  <https://github.com/espressif/arduino-esp32>

## Enforcement

This ADR is guideline-level (ADR-080): no new `evaluate.py` gate. The invariant —
the 0x26 **arm** and **feed** must be gated on the same presence flag, while the
**disarm** stays unconditional — is a semantic property of three functions in one
file. A declarative forbid/require pattern cannot express "these two writes are
gated by the same condition and that third one is not" without becoming brittle
(any refactor of the gate variable name or control-flow shape would break the
pattern while the property still held, or pass while it was violated). The
correct verification is code review of the three functions plus on-device serial
observation (no `i2c_master_transmit` storm on a chip-less board, no spurious
reset on a board with the chip). The pre-existing ADR-135 forbid-pattern gate on
the retired external-0x26-only watchdog shape in `OTGW-Core.ino` remains in force
and is unaffected by this amendment.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
