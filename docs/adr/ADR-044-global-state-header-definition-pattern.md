# ADR-044: Global State — extern Declaration in Header, Definition in .ino

**Status:** Accepted
**Date:** 2026-03-05
**Context:** v1.3.0-beta refactoring review

## Context

Several large global arrays and flags that are shared between `.ino` files were originally **defined** (with initialisers) directly inside `OTGW-Core.h`:

```cpp
// OTGW-Core.h — WRONG: definition with initialiser in a header
time_t   msglastupdated[256] = {0};
uint32_t mqttlastsent[256]   = {0};
uint16_t mqttlastsentstatusbit[16] = {0};
bool     mqttPublishAllowed  = true;
```

The same pattern existed for `OT_cmd_t cmdqueue[CMDQUEUE_MAX]` (also in the header).

**Why this is a problem:**

In standard C++, placing a variable *definition* (not just a declaration) in a header included by multiple translation units violates the **One Definition Rule (ODR)**. Each translation unit that `#include`s the header would create its own copy of the variable, and the linker would emit duplicate-symbol errors.

**Why it worked in the Arduino build:**

The Arduino IDE (and `arduino-cli`) concatenate all `.ino` files in a sketch directory into a **single `.cpp` translation unit** before compiling. With only one TU there is only one definition, so the ODR violation is silently hidden.

**Why it is still dangerous:**

1. If the project ever switches to a standard CMake or PlatformIO build that compiles each `.ino` separately, the code breaks at link time with no other changes.
2. Future `.ino` files that `#include "OTGW-Core.h"` (directly or transitively) contribute extra definitions when compiled by tools that do not perform the Arduino single-TU merge.
3. The existing `MQTTstuff.ino` already `#include`s `OTGW-Core.h` explicitly (`#include "OTGW-Core.h"` at line 16) — this is the exact scenario where a second definition would appear in a conventional compiler.
4. Static analysers and IDE linters flag definitions-in-headers as errors, producing noise that obscures real problems.

## Decision

**Use `extern` declarations in the header and a single definition in the owning `.ino` file.**

### Pattern

```cpp
// OTGW-Core.h — declaration only (no initialiser, no storage allocated)
extern time_t   msglastupdated[256];
extern uint32_t mqttlastsent[256];
extern uint16_t mqttlastsentstatusbit[16];
extern bool     mqttPublishAllowed;
```

```cpp
// OTGW-Core.ino — one definition (storage allocated here, initialiser here)
time_t   msglastupdated[256]       = {0};
uint32_t mqttlastsent[256]         = {0};
uint16_t mqttlastsentstatusbit[16] = {0};
bool     mqttPublishAllowed        = true;
```

### Rule

> A symbol is defined **once**, in the `.ino` file that logically owns it.
> Every other file that uses it gets an `extern` declaration, either through the header or directly.

**Static (`static`) symbols in headers are acceptable** because `static` gives the symbol internal linkage — each TU gets its own private copy. This is intentional for per-TU state such as `static OTdataStruct OTcurrentSystemState` and `static int cmdptr`. Do not confuse `static` global (internal linkage) with `extern` global (external linkage).

## Alternatives Considered

### Alternative 1: Leave definitions in the header (status quo)

**Pros:** Works in the current Arduino single-TU build.

**Cons:** ODR violation in any multi-TU build; misleading for developers; linter warnings.

**Why not chosen:** The hidden dependency on the Arduino merge quirk is a trap. A one-line change per symbol eliminates the risk permanently.

### Alternative 2: Move all shared state to a dedicated `globalState.ino`

**Pros:** Single authoritative source for all shared state.

**Cons:** Adds a new file with no functional difference; increases indirection; the logical owner of a symbol is already clear from context (e.g., `msglastupdated` belongs in `OTGW-Core.ino`).

**Why not chosen:** Unnecessary indirection. Define in the owning file.

### Alternative 3: Wrap in an anonymous namespace or `inline` variables (C++17)

**Pros:** `inline` variables in a header are ODR-safe in C++17.

**Cons:** ESP8266 Arduino core targets C++11/C++14 by default; `inline` variable support is compiler-version-dependent; adds a language-level dependency that is not validated in CI.

**Why not chosen:** The `extern`+definition pattern works on all C++ standards without any compiler feature requirements.

## Consequences

### Positive

- **ODR-safe:** no duplicate-symbol risk regardless of build system.
- **No functional change:** the Arduino single-TU merge still produces exactly one definition.
- **Linter-clean:** static analysers no longer flag the header for definition-in-header.
- **Explicit ownership:** the definition site clearly identifies which module owns the state.

### Negative

- **Minor verbosity:** two places to update when renaming a symbol (header declaration + .ino definition). Acceptable given the rarity of such changes.

### Risks & Mitigation

- **Missing `extern`:** if an `extern` declaration is accidentally omitted and a `.ino` tries to use the symbol, the compiler will emit an "undeclared identifier" error at compile time. This is the correct, detectable failure mode.
- **Forgetting to add definition in `.ino`:** linker emits "undefined reference". Also detectable at build time.

## Implementation

Changed in v1.3.0-beta refactoring:

- `OTGW-Core.h`: `time_t msglastupdated[256]`, `uint32_t mqttlastsent[256]`, `uint16_t mqttlastsentstatusbit[16]`, `bool mqttPublishAllowed` — changed from definitions to `extern` declarations.
- `OTGW-Core.ino`: definitions (with initialisers) added after the `OpenthermData_t` declarations.

The `OTPublishGate` RAII struct is defined in `OTGW-Core.h` because it is a struct (type), not a variable. Struct/class definitions in headers do not violate the ODR provided the definition is identical in every TU that includes the header (which the include guard guarantees).

## Related Decisions

- ADR-002: Modular `.ino` Architecture (explains why multiple files share a single TU)
- ADR-006: MQTT Integration Pattern (uses `mqttlastsent`, `mqttPublishAllowed`, `OTPublishGate`)
- ADR-004: Static Buffer Allocation Strategy (context for why large arrays are global rather than stack-allocated)
