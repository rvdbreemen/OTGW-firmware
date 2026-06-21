# ADR-151 — BLE Name Capture Stays on the Passive-Continuous Scan (Active-Scan Burst Rejected)

- **Status**: Accepted
- **Date**: 2026-06-21
- **Tags**: ble, esp32, sat, scan
- **Supersedes**: (none)
- **Superseded by**: (none)
- **Relates to**: ADR-092 (NimBLE adoption + continuous passive scan — upheld, not amended)

## Status

Accepted, 2026-06-22 (maintainer). Guideline-level per ADR-080 (no automated CI
gate; the behaviour is a runtime radio characteristic only observable on
hardware). Field-validated 2026-06-22: passive-only is stable on a live OTGW32;
the rejected active-burst crash-looped the device.

## Context

TASK-895 adds a name-prefix filter + advertised-name picker to the SAT BLE
roster. Filtering by advertised BLE name requires the firmware to have that
name. A BLE local name is reliably present only in the scan response, which a
peripheral sends only in reply to an **active** scan; a passive scanner often
sees an empty `getName()`.

The first TASK-895 implementation added a *hybrid* model: a short active-scan
burst (boot + on-demand `POST /api/v2/sat/ble/rescan`) that flipped the running
NimBLE scan passive→active→passive on the loop task to solicit scan responses.

**This was field-tested on a real OTGW32 (ESP32-S3) and crash-looped the
device.** After the OTA flash the unit served its web UI once, then went dark
and stopped responding on the network (0/30 HTTP probes over ~90 s; ICMP
"destination host unreachable"); recovery required a USB reflash. The new
runtime behaviour relative to the proven baseline was exactly the
active/passive flip (`stop()` → `setActiveScan(true)` → `start()` and back).
The reference implementation
(`other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp:358-364`) **never**
flips scan mode — it runs passive-continuous only.

A second consideration surfaced during recovery: ATC/pvvx sensors were expected
to advertise their name as `ATC_<mac>` in the **primary advertisement**, so a
passive scan would capture it. A field test on 2026-06-22 (combo firmware, live
OTGW32) **disproved this for the actual sensor**: its name is empty on passive
scan (`getName()` returns `""`); the name lives in the **scan-response**, not
the primary advertisement. So passive capture does NOT reliably yield the ATC
name — the name filter only keys on sensors that put their name in the primary
advertisement. The passive-friendly alternative for ATC sensors is a MAC-prefix
filter on the shared Telink OUI `A4:C1:38` (the MAC is captured passively).

## Decision

**Keep the scan passive and continuous (ADR-092 baseline, OT-Thing parity).
Do not flip scan mode at runtime.** Capture advertised names with `getName()`
on passive advertisements. The active-scan burst is **rejected**.

Specifics (`src/OTGW-firmware/SATble.ino`):

- `setActiveScan(false)` + `start(0,false,true)` only — no `stop()`/`start()`
  re-trigger, no boot burst, no on-demand burst.
- `getName()` is read in `onResult()` (after the temp-parse guard) and cached
  in `BLERuntime.sName[32]` (runtime-only); a passive ad with an empty name
  never overwrites a previously captured name.
- `POST /api/v2/sat/ble/rescan` and the UI "Rescan" button remain wired but the
  handler `satBLERescanRequest()` is a **safe no-op** (nothing to re-trigger on
  a continuous passive scan); the UI simply re-fetches `/discovery`.
- The name-prefix filter (display + optional ingest gate + loop-task prune)
  works on whatever names passive capture yields.

Consequence for the feature: any sensor that carries its name only in the
scan-response (the field-tested ATC sensor, and many BTHome sensors) stays
nameless on passive scan and is admitted/shown under the "empty name = admit"
rule. The name filter is therefore only effective for sensors that put their
name in the primary advertisement. For ATC sensors, a MAC-prefix filter on the
OUI `A4:C1:38` achieves the same noise-reduction goal passively.

## Alternatives Considered

### Hybrid passive base + on-demand active burst (original TASK-895 design)

Flip the running scan to active for a bounded window to harvest scan-response
names. **Rejected: it crash-looped the OTGW32 in field test.** Flipping a live
NimBLE scan on the single-radio ESP32-S3 is not a proven-safe operation here,
and OT-Thing never does it.

### Permanent active scan

Reliable names for all sensors but a continuous coexistence/power cost on the
single-radio S3, and it abandons the ADR-092 "passive to save power" decision
for a benefit only needed while picking a sensor. Rejected.

### A safer active-burst mechanism (future)

A correctly-sequenced active scan (e.g. set active before the first `start()`
for a one-shot bounded discovery, or an upstream-supported mode switch) might
be revisited if BTHome name coverage proves necessary. Out of scope for
TASK-895; would need its own ADR and hardware validation. Not pursued now.

## Consequences

**Positive**:

- The device boots and runs on the proven-stable ADR-092 / OT-Thing passive
  path; no radio-mode flipping.
- For ATC sensors a passive MAC-prefix filter (OUI `A4:C1:38`) still meets the
  original noise-reduction goal without any active scan or crash risk.

**Negative / risks**:

- BTHome sensors whose name lives only in the scan response stay nameless;
  the name filter cannot key on them. Documented in the setting help text.
- The "Rescan" affordance is cosmetic (re-fetch only); kept so the UI/REST
  surface is stable, but it does not force a fresh radio scan.

## Verification gates

- **Completeness**: Context (incl. the field-test failure), Decision,
  Alternatives, Consequences explicit. Scope contained to `SATble.ino` + the
  no-op `rescan` route + `BLERuntime.sName`.
- **Evidence**: Field test — OTGW32 crash-loop after the hybrid OTA flash
  (0/30 HTTP probes, ICMP unreachable, USB-reflash recovery). Reference —
  `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp:358-364` is
  passive-continuous only. Field test 2026-06-22 — the live ATC sensor's name is
  empty on passive scan (`getName()` == `""`), confirming its name is in the
  scan-response, not the primary advertisement.
- **Clarity**: Decision uses no hedging verbs; the rejected alternative is
  named explicitly.
- **Consistency**: Upholds ADR-092 (does not amend it). Honours the TASK-895
  "empty name = admit" rule. No change to ADR-090 `portMUX` model or ADR-004.

## Related Decisions

- ADR-092 — NimBLE adoption + continuous passive-scan model (upheld)
- ADR-090 — cross-task `portMUX` guard (name store uses the existing critical
  section; unchanged)
- ADR-080 — guideline-level vs binding ADRs (this is guideline-level)

## References

- TASK-895 — name picker + name-prefix filter (implements this decision)
- TASK-494 — continuous passive-scan switch (the baseline upheld here)
- `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp` — passive-only
  reference the maintainer directed us to follow
