#  ADR-081 Types merge into `<Component>stuff.h` when both headers exist (amendment to ADR-079)

## Status

Accepted (2026-04-19). Amends ADR-079's "one `<Component>types.h` per subsystem" rule to prevent file-count bloat for components that already have a `<Component>stuff.h` sibling.

## Context

ADR-079 introduced per-component type headers as `<Component>types.h`. For components that had no other header (SAT, OTDirect, Hardware, PIC, OTBus, Flash, Debug until TASK-336, Uptime, NTP, Sensors, S0, Outputs, Webhook, UI, Device — 14 of the 17 components covered by ADR-079), the rule was natural: the `types.h` file was the component's only header.

For three components — **MQTT**, **Network**, and (after TASK-336) **Debug** — a functional `<Component>stuff.h` already existed alongside the `<Component>stuff.ino` implementation. Adding a separate `<Component>types.h` created a redundant three-file cluster per component:

```
networkStuff.h     (131 lines, function decls + macros)
networkStuff.ino   (implementation)
Networktypes.h     ( 59 lines, OTGWNetworkMode + NetworkSection + EthernetSection)
```

The three-file split was defensible on strict ADR-079 grounds ("types always in types.h"). In practice, two problems surfaced during the 2026-04-19 session:

1. **Redundant file count**. Every TU already pulls everything via `OTGW-firmware.h`'s include chain. The per-file build-speed argument from ADR-079 does not bite here because the single-TU Arduino build pattern means any change to `Networktypes.h` triggers the same rebuild as a change to `networkStuff.h`. There is no compile-time saving.

2. **Cognitive overhead**. A developer opening `networkStuff.h` expects to see the full network surface: types, function decls, macros. Splitting types into a sibling file means *two* files answer the question "what does network expose?". That is the opposite of ADR-079's stated goal of making the component surface easier to navigate.

ADR-079's migration-path section already anticipates this by allowing "related structs live in the same file". The present ADR makes explicit that when a `<Component>stuff.h` already exists, it is the "same file" for the component's types.

## Decision

**Per-component types merge into `<Component>stuff.h` when both a `stuff.h` and a `types.h` candidate exist.** The tie-breaker:

- Component has only types/settings (no functional machinery) → `<Component>types.h` (ADR-079 rule, unchanged).
- Component has `<Component>stuff.h` + `.ino` → types live inside `<Component>stuff.h`. No separate `types.h`.
- Component has neither (rare; applies to per-file `<Name>.ino` collections like SAT*) → `<Component>types.h` is the bundled types-only header (ADR-079 rule, unchanged).

The rule keeps ADR-079's goals (types are discoverable in a known location per component) without paying the three-file cost when a functional header already exists.

### Concrete consequences

| Component | Has stuff.h? | Types header name | Change from ADR-079 state |
|-----------|:-:|---|---|
| SAT | No | `SATtypes.h` | unchanged |
| OTDirect | No | `OTDirecttypes.h` | unchanged |
| Hardware, PIC, OTBus, Flash, Uptime, Device, UI, Webhook, Outputs, S0, Sensors, NTP | No | `<Component>types.h` | unchanged |
| **MQTT** | Yes | **Inside `MQTTstuff.h`** | `MQTTtypes.h` merged into `MQTTstuff.h`, file deleted |
| **Network** | Yes | **Inside `networkStuff.h`** | `Networktypes.h` merged into `networkStuff.h`, file deleted |
| **Debug** | Yes | **Inside `debugStuff.h`** | `Debugtypes.h` merged into `debugStuff.h`, file deleted |

### Section ordering within `stuff.h`

When a `stuff.h` absorbs types, the file stays organized with an explicit layout:

```
// ===== Types (ADR-081) =====
// enums, helper structs, state/settings sections

// ===== Interface =====
// macros, function declarations, inline helpers
```

This keeps greppability: a contributor looking for `NetworkSection` opens `networkStuff.h` and finds the types block near the top; one looking for `startWiFi()` finds declarations below. Same file, two clearly-labeled halves.

### Include-order implications

The merge does not break anything because `<Component>stuff.h` headers were already included after the `#define` block in `OTGW-firmware.h` (the same position the per-component `types.h` files occupy after ADR-079 / TASK-326 AC3). Every TU already transitively sees both. The merge removes the redundant second include line per component.

## Consequences

### Positive

- Three fewer files in `src/OTGW-firmware/` (Debugtypes.h, Networktypes.h, MQTTtypes.h deleted after follow-up tasks).
- `<Component>stuff.h` becomes the canonical place to look for everything about the component — types, functions, macros. Matches the mental model of "open this one file to see the component's surface".
- Removes the "why does this component have three files while SAT has one types.h plus many .ino" inconsistency.
- ADR-079's benefit (reduced merge-conflict surface within OTGW-firmware.h) is fully preserved: types still leave the god-header, just land in the already-existing stuff.h instead of a new sibling.

### Negative

- ADR-079's purist pattern "<Component>types.h everywhere" is no longer absolute. The rule now has a tie-breaker. Slightly more to remember when writing a new component.
- `MQTTstuff.h` grows from 361 to ~410 lines (adds 49 lines from MQTTtypes.h). `networkStuff.h` grows from 131 to ~190. `debugStuff.h` grows from ~40 to ~73. All still comfortably under the "god header" threshold.
- Anyone who was mid-air on a feature branch referencing `MQTTtypes.h` / `Networktypes.h` / `Debugtypes.h` will need to update imports. Migration cost is small; follow-up tasks update the few reference sites in-tree.

### Neutral

- ADR-044 single-instance rule still intact.
- ADR-051 Hungarian prefix and naming conventions still intact.
- ADR-080 binding-rules-need-CI-gate still intact. `check_adr_gates` continues to audit this ADR alongside the others.

## Related

- **ADR-079** — Per-component type headers. This ADR amends the rule without superseding it: types still extract from OTGW-firmware.h per ADR-079; they just land in the stuff.h sibling when one exists.
- **ADR-044** — Single-point-of-instantiation. Unchanged.
- **ADR-051** — Dual settings/state structs. Unchanged.
- **ADR-080** — Binding ADR rules must have a CI gate. This ADR is structural (amends file layout), not pattern-level; no new gate required per ADR-080 classification.
- **TASK-333 / TASK-334 / TASK-336** — The three concrete merges that apply this ADR. Each one merges its component's types.h into the stuff.h sibling and deletes the types.h file.
- **TASK-335** — OTGW-Core.h audit. Orthogonal to this ADR; applies regardless of rule direction.
- **TASK-326 AC3** — The migration that created the three redundant types.h files. This ADR course-corrects three cases out of the 17 components covered by AC3; the other 14 remain on the ADR-079 pattern because they have no stuff.h sibling.
