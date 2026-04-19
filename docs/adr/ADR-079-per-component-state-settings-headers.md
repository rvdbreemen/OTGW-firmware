# ADR-079 Per-component Type Headers (amendment to ADR-051)

## Status

Accepted (2026-04-19). Revised (2026-04-19) to settle on **one `<Component>types.h` file per subsystem** (bundling state + settings + enums) instead of the original two-file `state_<x>.h` / `settings_<x>.h` split, because the defaults of the settings struct typically reference enums from the runtime state. See "Naming revision" below.

Supersedes ADR-051's single-file layout choice; keeps ADR-051's dual-struct, Hungarian-prefix, two-level-section conventions intact.

## Context

ADR-051 established the dual encapsulating structs (`OTGWSettings`, `OTGWState`), two-level sections (e.g. `settings.mqtt.sBroker`, `state.sat.fRoomTemp`), and Hungarian prefixes. It did not prescribe where those structs are declared. In practice every section landed in `src/OTGW-firmware/OTGW-firmware.h`.

As of 2026-04-19 the consequences are:

- `OTGW-firmware.h` grew from 173 lines (v1.3.5) to 1102 lines, and now carries every component's struct: Hardware, Network, PIC, OTDirect, OTBus, MQTT runtime, Flash, Debug, Uptime, PicSettings, SAT (three structs including a 175-line SATRuntimeSection), plus every settings section (MQTT, NTP, Sensors, S0, Outputs, Webhook, UI, PICBoot, SAT, OTDirect, Ethernet, Device).
- Editing any section triggers a rebuild of every `.ino` file (every TU that sees this header), which is every firmware source file.
- Every feature-branch merge-conflicts against this file because it is touched constantly.
- Contributors touching a SAT setting have to wade past 800 lines of other sections.

The review (2026-04-18, finding ARCH-L2) recommended splitting.

## Decision

Section struct declarations move into per-component headers, following a consistent `state_<component>.h` / `settings_<component>.h` naming scheme. The aggregate `OTGWSettings` / `OTGWState` structs and the two global instances (`settings`, `state`) stay in `OTGW-firmware.h`, which now just includes the per-component headers. ADR-044's single-point-of-instantiation guarantee is preserved because the globals are still defined in exactly one translation unit and included from exactly one place.

### Naming revision (2026-04-19)

The original draft proposed a two-file split per component: `state_<x>.h` + `settings_<x>.h`. That introduced an artificial dependency: every `settings_<x>.h` had to `#include "state_<x>.h"` because settings-struct default values typically reference enums declared in the runtime state (e.g. `SAT_HSYS_AUTO`, `SAT_MFR_AUTO`). The split added a file without adding value.

Revised decision: **one `<Component>types.h` file per subsystem**, bundling enums, helper structs, the runtime-state struct, and the settings struct. This mirrors the sibling-to-`<Component>*.ino` naming convention already used by existing per-file headers (`OTGW-Core.h` beside `OTGW-Core.ino`, `MQTTstuff.h` beside `MQTTstuff.ino`, etc.).

### Directory layout

Per-component type headers live alongside their sibling `.ino` files in `src/OTGW-firmware/`:

```
src/OTGW-firmware/
  OTGW-firmware.h                       <- aggregates + globals only
  SATtypes.h                            <- SAT enums, SATWindowRecord, SATZoneState,
                                           SATRuntimeSection, SATSection
  OTDirecttypes.h                       <- OTDirect enums + OTDirectSection + OTDirectSettingsSection
  MQTTtypes.h                           <- MQTT runtime + MQTT settings sections
  Networktypes.h                        <- NetworkSection + Ethernet settings
  PICtypes.h                            <- PICSection + PicSettingsSection + PICBootSection
  Hardwaretypes.h                       <- HardwareSection + board enums
  ...                                   <- etc. (25+ sections total, but each file bundles related ones)
```

Grouping guidance: when a settings struct and a runtime state struct cover the same subsystem and share enums, they go in the SAME file. When two components are genuinely independent (e.g. `UISection` vs `WebhookSection`), they get their own files. The `types` suffix makes it explicit that the file contains only type declarations, not function definitions.

### Header file conventions

Every per-component header:

1. Starts with `#pragma once` (project convention — already used by existing headers).
2. Includes the minimum set of standard/platform headers it needs (`<Arduino.h>` for `uint8_t` and friends). Does NOT re-include `OTGW-firmware.h` (no circular dependency).
3. Bundles every type declaration for the subsystem: enums, helper structs, runtime-state section, and persistent-settings section in the order listed. Settings go last because their default values reference the enums.
4. Uses the existing Hungarian prefix + naming rules from ADR-051.
5. Carries a top-of-file comment linking back to this ADR and the originating ADR-051, and briefly lists what the file contains.

### OTGW-firmware.h after migration

`OTGW-firmware.h` keeps:

- The global settings/state types (`struct OTGWSettings`, `struct OTGWState`) — aggregates only.
- The global instances `extern OTGWSettings settings;` / `extern OTGWState state;` with single-point-of-instantiation rule unchanged (ADR-044).
- Project-wide enums that cross component boundaries (e.g. `OTGWHardwareMode`) if and only if they are used by more than one component.
- Forward declarations of cross-file functions (e.g. the `satHandle*` family) — these are project-level glue and belong with the aggregates.
- `#include` directives for every per-component header, in a stable order.

It should NOT keep any component-scoped struct body, magic-number constant, or helper function.

### Migration path (incremental, per PR)

Splitting all 25+ sections in one PR is high-risk (every `.ino` file is affected). The migration runs incrementally:

1. **Per-PR scope**: one or two sibling headers per PR, with a stable check-list of sections still pending. **SAT landed first (2026-04-19) as `SATtypes.h`** — serves as the canonical template for the remaining subsystems.
2. **Order**: start with the longest, most self-contained sections (SAT done, OTDirect next) because they give the most readability gain per move. Leave cross-cutting enums for last.
3. **Forward-compat**: until all 25 sections are moved, `OTGW-firmware.h` contains a mix of inline structs and `#include`s. That is temporarily uglier but keeps every intermediate state buildable.
4. **Validation**: every migration PR must build cleanly on both ESP8266 and ESP32, and keep `OTGWSettings` / `OTGWState` sizes byte-identical (printed by a one-liner added to `verboseConfig()` for this purpose).

### What stays in OTGW-firmware.h regardless

- `OTGW-firmware.h` remains the anchor file. `config.py` / build scripts that parse firmware metadata still key off it. Do not rename or split the anchor.
- Constants that define memory-layout contracts (e.g. `CMSG_SIZE`) stay in the anchor so they have a single canonical location.

## Consequences

### Positive

- Editing a SAT setting stops triggering a full-project recompile of anything that doesn't actually depend on SAT state. Build times drop on iterative work.
- Merge conflict surface reduces drastically: different contributors editing different components no longer collide on the same 1102-line file.
- Grepping for the SAT state no longer requires scrolling past 10 unrelated sections.
- The per-component header makes it obvious which structs a component owns, which simplifies code review of cross-component PRs.
- New components (e.g. a future `weather_station` module) follow the same template, which reduces decision overhead.

### Negative

- More files in the `src/OTGW-firmware/` directory (roughly +25). ls output gets longer; C4 code docs need to reflect the new layout.
- The migration is a noisy diff. Every `.ino` that accessed a section transitively picks up the new header chain — usually zero change because everything goes through `OTGW-firmware.h` anyway, but the noise needs to be reviewed.
- Tooling that greps for `struct XxxSection` has to scan multiple files now. Acceptable — `grep -r` on the source tree works as before.

### Neutral

- Single-instance rule (ADR-044) unchanged.
- Hungarian-prefix and two-level-section rules (ADR-051) unchanged.
- No behaviour change; the compiled binary should be byte-identical if nothing else moves.

## Related

- **ADR-044** — Single-point-of-instantiation for globals. Preserved.
- **ADR-051** — Dual encapsulating settings/state structs and Hungarian prefixes. Amended only on file layout; naming rules unchanged.
- **ARCH-L2** review finding (2026-04-18) — source for this ADR.
- **TASK-319** — tracking task. Proposes this ADR; the actual per-section file moves land in follow-up tasks.
- **Per-component C4 code docs** (`docs/c4/c4-code-settings.md`, `docs/c4/c4-code-otgw-core.md` etc.) — need a one-liner update per section during migration.
