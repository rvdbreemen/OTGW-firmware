# ADR-083: PlatformIO as primary build system for dual-target firmware

**Status:** Accepted
**Date:** 2026-04-24
**Supersedes:** ADR-014 (Dual Build System — Makefile + Python Script)

## Context

ADR-014 (2020-01-01, Accepted) documented a dual-build setup using
`arduino-cli` as the core tool, a Makefile for CI/CD, and `build.py` as a
Python wrapper. At that time the firmware was single-target (ESP8266),
the Arduino IDE was still a supported developer workflow, and PlatformIO
was explicitly rejected under the reasoning "Want to support both
Arduino IDE and command-line builds. PlatformIO is either/or."

Since then several facts have shifted:

1. **Dual-target reality.** ADR-061 (Unified ESP8266/ESP32 Platform
   Abstraction, 2026-04-02) added ESP32 support. The single
   `[env:esp8266]` / `[env:esp32]` PlatformIO configuration has to carry
   board-specific settings (board, f_cpu, flash_mode, ldscript,
   platform_packages, build_flags, lib_ignore, build_src_filter) that
   would need parallel Makefile and `build.py` code paths to reproduce.
2. **Library patching pipeline.** `scripts/patch_pio_libs.py` patches
   upstream libraries at build time (NetApiHelpers for Core 3.x;
   Windows/MinGW include-order workarounds). This is wired into the
   PlatformIO `extra_scripts` pre-hook. Replicating that in the Makefile
   path would duplicate the patch logic in two places.
3. **Library version pinning.** The shared `[env]` block pins AceTime to
   a GitHub tag URL (`https://github.com/bxparks/AceTime.git#v2.0.1`);
   `arduino-cli` does not have a first-class equivalent, so that pin
   lives only in the PlatformIO config.
4. **Incremental build caching.** PlatformIO's `.pio/build/` per-target
   object cache gives fast iteration on the ESP32 side (where cold
   builds take ~2 minutes) in a way the `arduino-cli` setup never
   provided.
5. **Arduino IDE is no longer a supported contributor workflow.** The
   codebase now uses constructs (dual-target platform abstraction,
   `#include "platform.h"` entry point, ctags forward-decl stub flags,
   `build_src_filter` to exclude OTDirect.ino on ESP8266) that the
   Arduino IDE does not model.

`build.py` (the Python wrapper) is still relevant for the LTS-1.5.x
branch, which uses `arduino-cli` directly against Core 2.7.4 for
historical continuity and because the LTS branch precedes the PlatformIO
configuration. On the 2.0.0 line, `build.py` is no longer in the
critical path for producing firmware artifacts; `pio run -e esp8266` and
`pio run -e esp32` are.

## Decision

For the **2.0.0 line (dual-target ESP8266 + ESP32)**, PlatformIO is the
**primary build system**. The single source of configuration truth is
`platformio.ini` and the `extra_scripts` pre-hooks it references. All
CI/CD, release builds, and developer workflows on 2.0.0 use:

```
pio run -e esp8266          # build ESP8266 firmware
pio run -e esp32            # build ESP32 firmware
pio run                     # build all targets (default_envs)
pio run -e esp8266 -t upload
```

The `build.py` / `arduino-cli` / Makefile stack is retained for the
**LTS-1.5.x branch only**. Running `python build.py --firmware` on
LTS-1.5.x produces artifacts consumable by the existing field update
flow. The `build.py` wrapper remains in the repository tree on both
branches so historical scripts and automation keep working, but on the
2.0.0 line it is not the expected entry point and is not kept current
with the multi-env matrix.

Release artifact naming convention stays target-suffixed per ADR-061
(e.g., `OTGW-firmware-esp8266-2.0.0-beta.bin` and
`OTGW-firmware-esp32-2.0.0-beta.bin`); both build stacks produce
equivalent binaries for their respective platforms.

## Alternatives Considered

### Alternative 1: Keep Makefile + build.py as primary, add PlatformIO in parallel

**Pros:** Minimal disruption to ADR-014; keeps the decade-old build
flow intact.

**Cons:** Requires reimplementing ESP32 build logic, library version
pinning, per-env build flags, and the patch-libs pre-hook in the
Makefile. Doubles the maintenance surface. Any divergence between the
two stacks produces silent artifact differences.

**Why not chosen:** The PlatformIO config already carries the dual-env
semantics; duplicating it in Makefile form offers no benefit and
increases the chance of "green on CI, broken in field" drift.

### Alternative 2: Migrate LTS-1.5.x to PlatformIO too

**Pros:** One build system across all active branches.

**Cons:** LTS-1.5.x was forked specifically to freeze the ESP8266 LTS
configuration with Core 2.7.4. Introducing PlatformIO would add a new
source of variability on a branch whose purpose is stability. The
existing `arduino-cli` + `build.py` chain has produced all LTS
artifacts; switching tools late risks silent artifact changes.

**Why not chosen:** Scope-creep. LTS-1.5.x's purpose is to remain
unchanged unless a critical fix is needed.

### Alternative 3: Delete build.py / Makefile entirely on 2.0.0

**Pros:** Cleaner tree; one obvious way to build.

**Cons:** Some downstream automation (release scripts, Docker images,
documentation, older instructions in README) references the
`arduino-cli` / `build.py` path. Removing them would require a
coordinated pass across documentation, CI, and any community fork
guides.

**Why not chosen:** The operational cost of retiring the old stack
outweighs the tree-cleanliness benefit right now. A follow-up ADR can
retire `build.py` on 2.0.0 once no automation depends on it.

## Consequences

### Positive

- **Single-source-of-truth config.** `platformio.ini` is the only file
  that needs editing when adding a new build flag, library version, or
  target env. The ctags stub `-D` flags, the lib_ignore list, and the
  build_src_filter exclusion all live in one place.
- **Dual-target symmetry.** Adding a third target in the future (e.g.
  ESP32-C3) is a new `[env:...]` block rather than a parallel
  Makefile + build.py branch.
- **Pre-hook library patching.** `scripts/patch_pio_libs.py` is wired
  into `extra_scripts` and runs consistently before every build, so
  upstream patches never get skipped.
- **Fast incremental rebuilds.** `.pio/build/` caching gives
  sub-minute ESP8266 rebuilds and ~2-minute ESP32 rebuilds after the
  first cold compile.

### Negative

- **Two branches, two build tools.** 2.0.0 developers run `pio run`;
  LTS-1.5.x developers run `python build.py`. Documentation must make
  that explicit.
- **`build.py` still lives on 2.0.0 but is not authoritative.** Risk
  of developers running it on 2.0.0 and getting a stale or incomplete
  build. Mitigate by making `platformio.ini` the prominent entry point
  in README and CLAUDE.md.
- **Arduino IDE no longer viable on 2.0.0.** Anyone who still wants an
  IDE workflow needs VSCode + PlatformIO extension. (In practice the
  Arduino IDE had already stopped handling the modern codebase per
  Context §5.)

### Neutral

- **Binary output identical.** Both build stacks compile the same
  sources through the same toolchain packages; artifact content is
  equivalent when targeting the same Core version.
- **CI migration cost.** Already absorbed: existing CI for the 2.0.0
  line has been using `pio run` since ADR-061 landed.

## Migration Notes

- **ADR-014 is superseded by this ADR.** Its status field will be
  updated to reflect that; its content remains unchanged per the
  project rule "NEVER edit an Accepted or Deprecated ADR" (only the
  status line is touched).
- **Documentation in README.md and CLAUDE.md** should lead with `pio run`
  for 2.0.0 and mention `python build.py` as the LTS-branch path.
- **Release notes for 2.0.0-beta** should state the build-tool
  expectation so downstream maintainers know which command to use.

## Related

- ADR-014 (Dual Build System) — superseded by this ADR.
- ADR-061 (Unified ESP8266/ESP32 Platform Abstraction) — introduced
  the `platformio.ini` file and the dual-env configuration.
- ADR-082 (ESP8266 Arduino Core 2.7.4 LTS pin) — the Core-version
  pinning lives in the PlatformIO `[env:esp8266]` block described here.
- `platformio.ini` — the configuration file this ADR centres on.
- `scripts/patch_pio_libs.py` — the pre-hook library patcher referenced
  in Context §2.
