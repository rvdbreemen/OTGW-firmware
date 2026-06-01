# ADR-113 Hardware-type as codepath-selection contract

## Status

Accepted, 2026-05-29

Guideline-level (per ADR-080): this ADR defines a published contract and a
selection discipline. There is no dedicated `evaluate.py` gate yet; if new
`picavailable`-based selection regressions appear, a `check_hardware_type_selection`
gate should be added and this ADR promoted to binding.

## Context

The firmware historically used a single signal — `picavailable`
(`state.pic.bAvailable`, published on MQTT as `otgw-pic/picavailable` and in the
REST `/api/v2/device/info` payload) — to answer two semantically different
questions that happen to coincide on the original ESP8266 + PIC hardware:

1. **"Is this a PIC-class board?"** — a *static* property of the hardware
   variant. Known at compile time from `boards.h` (`HAS_PIC`).
2. **"Is a PIC responding right now?"** — a *runtime* liveness property. A
   PIC-class board with a missing or dead PIC answers *no* here while still
   *being* a PIC-class board.

On the original hardware both questions had the same answer often enough that
conflating them was harmless. The 2.0.0 line breaks that coincidence:

- **OTGW32 (Nodoshop ESP32-S3, `BOARD_NODOSHOP_ESP32`, `HAS_PIC=0`)** is a
  PIC-less product. OpenTherm is handled natively by OTDirect. There is no PIC
  and there never can be one. Yet the firmware still publishes
  `picavailable = OFF` unconditionally and the web UI still keys PIC-specific UI
  on it. The signal is, for this board, a permanently-false value describing a
  component that does not exist — confusing to read in logs, MQTT, and HA.

Field evidence (telnet log of `otgw-1020BA16C6BC`, firmware
`2.0.0-alpha.84+9be88a0`, captured 2026-05-29): header shows `PIC : no pic found`,
and `OTGW/value/otgw-1020BA16C6BC/otgw-pic/picavailable --> OFF` is published on
a board that is PIC-less by construction.

The transition to OTGW32 is *always* PIC-less. As more hardware variants are
planned, codepath and UI selection should key on an explicit, machine-readable
**hardware type** (the board class), not on a runtime liveness flag that merely
happens to be false.

### Current state of the codebase (inventory)

- **Backend** already selects on semantically-correct helpers, not on the
  published string: `isPICEnabled()` (runtime PIC liveness, compile-time `false`
  on OTGW32), `isOTDirectEnabled()`, `hasOTCommandInterface()`. These are
  correct and stay.
- The only place that *selects codepath/UI shape* on the published
  `picavailable` value is the **frontend** (`data/index.js`,
  `applyPICAvailability()` toggling `pic-only` elements).
- A static board-class string is **not** currently published. `hardwaremode`
  (REST) reflects the *runtime operational mode* (`HW_MODE_PIC` /
  `HW_MODE_OT_DIRECT` / `HW_MODE_DEGRADED`), which is dynamic and unsuitable as a
  stable hardware identity. `boardName()` is a human display string, not a
  machine slug.

## Decision

1. **Introduce a static, machine-readable `hardware_type` contract.** Its value
   is derived at compile time from the selected board in `boards.h` via a new
   `HW_TYPE_NAME` macro and exposed through a `hardwareTypeName()` helper.
   Defined slugs:

   | Board (`boards.h`)        | `HAS_PIC` | `hardware_type` |
   |---------------------------|-----------|-----------------|
   | `BOARD_NODOSHOP_ESP8266`  | 1         | `otgw-classic`  |
   | `BOARD_NODOSHOP_ESP32`    | 0         | `otgw32`        |
   | *(future)* OT-Thing       | 0         | `ot-thing`      |

2. **Publish `hardware_type`** as a retained MQTT topic
   `<topTopic>/value/<uid>/otgw-firmware/hardware_type` and as a
   `hardware_type` field in the REST `/api/v2/device/info` payload, plus a
   Home Assistant diagnostic discovery entity (pseudo-ID 248, firmware-info group).

3. **Maintain the capability-vs-liveness invariant.** `hardware_type` drives
   codepath / UI *shape* (does this board class have a PIC slot at all). Runtime
   liveness ("is the PIC / OT bus working now") stays the job of
   `isPICEnabled()` / `isOTDirectEnabled()` / `hasOTCommandInterface()` in the
   backend and `otcommandinterface` in the frontend. The two MUST NOT be
   collapsed: a `otgw-classic` board with a dead PIC shows PIC UI (it belongs to
   the board class) with a "PIC not detected" substatus, rather than hiding it.

4. **Migrate frontend selection** off `picavailable`: `pic-only` elements are
   shown when `hardware_type === 'otgw-classic'`, not when the runtime
   `picavailable` is true. `picavailable` becomes a pure liveness substatus.
   Frontend falls back to the previous behaviour when the `hardware_type` field
   is absent (forward/backward compatibility during the transition).

5. **Deprecate `picavailable` for one release.** It keeps being published
   (MQTT + REST) so existing HA dashboards/automations do not break, with a
   deprecation marker in code and docs and a one-time telnet deprecation log.
   It is removed in the release following the one that introduces
   `hardware_type`.

### Scope

2.0.0 only. OTGW32 exists only on the 2.0.0 line; `dev` (1.5.x) is Classic-only,
where `hardware_type` is trivially constant. This is not a cross-worktree change.

## Alternatives Considered

1. **Keep conflating board-class and PIC-liveness on `picavailable`** (status
   quo). Rejected: the two questions ("is this a PIC-class board?" vs "is a PIC
   responding now?") diverge on OTGW32, where `picavailable` is a permanently
   false value describing a component that does not exist — confusing in logs,
   MQTT and HA (field evidence: `otgw-1020BA16C6BC`, alpha.84, publishes
   `picavailable OFF` on a PIC-less board).
2. **Reuse the existing `hardwaremode` REST field for selection.** Rejected:
   `hardwaremode` reflects the *runtime operational mode*
   (`HW_MODE_PIC` / `HW_MODE_OT_DIRECT` / `HW_MODE_DEGRADED`), which is dynamic —
   unsuitable as a stable hardware identity to switch codepath/UI on.
3. **Reuse `boardName()`.** Rejected: it is a human-facing display string, not a
   machine-readable slug; keying UI logic on display text is brittle.
4. **Introduce a static `hardware_type` slug from `boards.h`** (chosen):
   compile-time, machine-readable, zero runtime cost, and extensible — a new
   board variant adds one `HW_TYPE_NAME` and a table row.

## Consequences

**Positive**

- Codepath/UI selection reads as what it means ("this is an OTGW32") instead of
  a double-negative on a never-present component.
- A stable identity to switch on as more board variants land; new variants add a
  `HW_TYPE_NAME` and a row, nothing else.
- Compile-time slug: zero runtime cost, foldable, no extra RAM beyond one
  PROGMEM string.

**Negative / costs**

- One release carries both `picavailable` and `hardware_type` (transition
  duplication). Accepted: it is the price of not breaking field testers' HA
  setups.
- A second migration commit is required next release to remove `picavailable`.
  Tracked as the deprecation follow-up.

**Neutral**

- `hardwaremode` (operational) and `boardName()` (display) are unchanged and
  retain their distinct roles; `hardware_type` is the third, machine-identity,
  axis.

## Related Decisions

- Supersedes the implicit "picavailable means everything PIC" convention.
- ADR-051 (settings/state architecture), ADR-079 (per-component type headers) —
  `OTGWHardwareMode` / `HardwareSection` live in `Hardwaretypes.h`.
- ADR-065 (PIC subtree topic API) — `otgw-pic/picavailable` lives there; this
  ADR begins its deprecation.
- ADR-080 (binding ADRs need a CI gate) — this ADR is guideline-level until a
  gate exists.

## References

- TASK-753 (implementation of the `hardware_type` contract).
- Field evidence: telnet log of `otgw-1020BA16C6BC`, firmware
  `2.0.0-alpha.84+9be88a0`, captured 2026-05-29 (`PIC : no pic found` +
  `picavailable --> OFF` on a PIC-less OTGW32).
