---
id: ADR-120
title: Platform Abstraction Headers Promoted to the src/libraries/Platform Library
status: Accepted
date: 2026-06-02
tags: [architecture, esp32, esp8266, abstraction, portability, build, library]
supersedes: []
superseded_by: []
related: [ADR-061, ADR-115, ADR-083, ADR-079]
deciders: [Robert van den Breemen]
---

# ADR-120: Platform Abstraction Headers Promoted to the src/libraries/Platform Library

## Status

Accepted. Date: 2026-06-02.

**Decision Maker:** User: Robert van den Breemen (TASK-746, ESP-abstraction Tier 6).

## Status History

status_history:
  - date: 2026-06-02
    status: Accepted
    changed_by: User
    reason: Approved the Tier 6 relocation; implementation landed and builds green on both targets (esp8266 84.7% flash, esp32 95.7% flash)
    changed_via: backlog

## Context

ADR-061 established a single ESP8266/ESP32 abstraction layer and ADR-115 fixed
the home for per-board numeric constants. Through Tiers 0-5 (TASK-740..745) the
layer matured into four headers:

- `platform.h` — the single application include surface; selects the per-platform
  sub-header and hosts platform-agnostic helpers (`PlatformDir`, the
  `PLATFORM_INT_DISTINCT_FROM_INT32` integer-model flag).
- `platform_esp8266.h` / `platform_esp32.h` — the divergent-API shims.
- `boards.h` — pin maps, `HAS_*` capability flags, per-board tuning constants.

Until now these lived in `src/OTGW-firmware/` as application-tier neighbour
headers, included with relative quotes (`#include "platform.h"`). The boundary
between "abstraction layer" and "application code" was therefore only *nominal*:
nothing physically stopped application code from sitting in the same directory
and reaching for a raw `#ifdef`. The only thing holding the line was the
`evaluate.py` boundary scan plus reviewer discipline.

A library is the idiomatic way to make such a boundary *physical*. The project
already vendors abstraction-bearing libraries this way (`SimpleTelnet`,
`OpenTherm`), each with a `library.properties` and a `src/` subfolder, resolved
by arduino-cli (`build.py --libraries src/libraries`) and PlatformIO
(`lib_extra_dirs = src/libraries`, ADR-083). Promoting the platform headers into
the same shape means application code includes `<platform.h>` / `<boards.h>` over
the library include path, and the directory separation makes the layering
self-evident without relying on a lint to notice a stray neighbour file.

This Tier is **purely structural**: no shim logic, constant, or conditional
changes. Only file location and include form change.

## Decision

The platform abstraction headers live in a first-class Arduino library at
**`src/libraries/Platform/`** with this layout:

```
src/libraries/Platform/
  library.properties          # name=Platform, architectures=esp8266,esp32
  src/
    platform.h
    platform_esp8266.h
    platform_esp32.h
    boards.h
```

- Application code includes the layer through the library path
  (`#include <platform.h>`, `#include <boards.h>`) and never via a relative
  path into `src/OTGW-firmware/`.
- Headers sit under the library's `src/` subfolder, matching the existing
  `SimpleTelnet` / `OpenTherm` convention so both build backends place
  `Platform/src/` on the compiler include path. The build (both targets) is the
  arbiter that this resolves.
- The `evaluate.py` abstraction allowlist
  (`ESP_ABSTRACTION_ALLOWED_FILES`) points at the new
  `src/libraries/Platform/src/*.h` paths. This is load-bearing, not cosmetic:
  the boundary scan rglobs `src/libraries/`, so without the repoint every
  `#if defined(ESP8266/ESP32)` inside the relocated headers would be miscounted
  as a fresh leak.

**Abstraction-layer contract (split home, post-promotion):**

| Concern | Home |
|---|---|
| Application include surface | `<platform.h>` (library) |
| Divergent runtime API shims | `platform_esp8266.h` / `platform_esp32.h` (library) |
| Pin maps, `HAS_*` flags, per-board constants/typedefs | `boards.h` (library, per ADR-115) |
| ModUpdateServer platform trio | `src/OTGW-firmware/OTGW-ModUpdateServer*.h` (application tier, still allowlisted) |

The `OTGW-ModUpdateServer` trio deliberately stays in the application tier: it
is OTA-update glue, not part of the general platform shim surface, and was out
of scope for this relocation. The abstraction allowlist therefore spans two
locations by design.

## Alternatives Considered

1. **Leave the headers in `src/OTGW-firmware/` (status quo).** Rejected: the
   boundary stays nominal. Nothing physical separates the abstraction layer from
   application code; the layering depends entirely on the lint and on reviewers
   noticing a misplaced neighbour file.
2. **Flat legacy library layout (headers at `Platform/` root, no
   `library.properties`),** mirroring the `OTGWSerial` vendored library.
   Rejected: AC#1 requires a `library.properties` so the library is a
   self-describing unit (name, supported architectures, version). The two
   abstraction-bearing libraries this project already ships (`SimpleTelnet`,
   `OpenTherm`) use the `library.properties` + `src/` form; matching that
   convention keeps the tree consistent and lets the build resolve headers on
   the standard include path.
3. **`src/libraries/Platform/` with `library.properties` (chosen).** Physical
   boundary, self-describing unit, consistent with existing vendored
   abstraction libraries, resolved by both build backends.

## Consequences

**Positive:**
- The abstraction boundary is physical: application code and the platform layer
  live in different directories, and the `<...>` include form documents the
  dependency direction.
- The library is a self-describing, relocatable unit (`library.properties`),
  consistent with `SimpleTelnet` / `OpenTherm`.
- No logic change, so no behavioural risk; `git mv` preserves file history.

**Negative:**
- The abstraction allowlist now spans two locations
  (`src/libraries/Platform/src/` for the core, `src/OTGW-firmware/` for the
  ModUpdateServer trio). A future contributor adding a shim must know the core
  home is now the library, not the sketch directory. This ADR and the
  `evaluate.py` comment are the signposts.
- Relative includes into `src/OTGW-firmware/platform.h` no longer compile; any
  out-of-tree consumer must switch to `<platform.h>`. Inside this repo all six
  call sites were updated in the same change.

## Enforcement

Guideline-level under ADR-080, sharing the existing ESP-abstraction CI gate
`evaluate.py::check_esp_abstraction_boundary()`. The gate's allowlist now names
the library paths; a relocated header is recognised as abstraction layer rather
than counted as a leak, and any application file that reverts to a raw platform
`#ifdef` still trips the baseline ratchet. No new gate is added.

## Related Decisions
- ADR-061: Unified ESP8266/ESP32 Platform Abstraction (this ADR makes ADR-061's layer a physical library; the home assignments are unchanged, only relocated)
- ADR-115: Per-board numeric constants live in `boards.h` (that home moves with `boards.h` into the library)
- ADR-083: PlatformIO as primary build system (`lib_extra_dirs = src/libraries` resolves the new library; arduino-cli resolves it via `--libraries`)
- ADR-079: Per-component type headers (the platform layer is a distinct, cross-component concern, hence a library rather than a `<Component>types.h`)
- ADR-080: Binding-ADR-needs-a-CI-gate meta-rule (this ADR is guideline-level, sharing the existing boundary gate)

## References
- TASK-746 (ESP-abstraction Tier 6 — this relocation); depends on TASK-745 (Tier 5, Done)
- TASK-739 (the abstraction-leak audit that established the boundary scan and allowlist)
- `evaluate.py::check_esp_abstraction_boundary()` / `ESP_ABSTRACTION_ALLOWED_FILES` — the boundary gate and allowlist repointed by this change
- Existing convention: `src/libraries/SimpleTelnet/`, `src/libraries/OpenTherm/` (`library.properties` + `src/` subfolder)
