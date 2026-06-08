# ADR-084: Build-Time ESP8266 Core Patch for the HTTP NULL-Alloc Crash

## Status

Proposed

## Context

George (`geo83_44083`, Discord `#beta-testing`) reported random reboots on every firmware from v1.6.0 onward. A multi-week field investigation (6-version bisect + parallel hypothesis analysis) confirmed the root cause is **in the ESP8266 Arduino Core (2.7.4), not in our firmware**:

- Serving a static asset via `httpServer.streamFile()` makes the core's `BufferedStreamDataSource::get_buffer()` (`arduino/.../ESP8266WiFi/src/include/DataSource.h`) allocate one TCP segment (~1460 bytes) per chunk with an **unchecked, non-throwing `new uint8_t[]`**. On Core 2.7.4 (`-fno-exceptions`) `operator new[]` is `malloc` and returns `NULL` on OOM (verified `cores/esp8266/abi.cpp`).
- Under heap fragmentation (largest contiguous block below ~1460 B) that `new[]` returns `NULL`; the following `readBytes`/`memcpy` writes to the NULL buffer.
- `ClientContext::_write_some()` (`ClientContext.h`) passes the `get_buffer()` result to `tcp_write()` without a NULL check.
- Result: `Exception(2)` StoreProhibited, ROM `memcpy` `epc1=0x4000df64`, `excvaddr=0`. Confirmed quantitatively: the crash cliff is `maxBlock ~1460` (field floors: beta.6 1944 = no crash; beta.12/16 648/368 = degrade/crash).

The firmware-side mitigation (ADR-style gate `canServeHttp()` + `streamFileGuarded()`, TASK-843, shipped in 1.7.0-beta.3) prevents the crash by refusing to serve (HTTP 503) when the contiguous block is too small. That is portable and sufficient to stop reboots, but it treats the symptom: it can refuse a serve that would actually have succeeded, and it does not fix the unchecked allocation itself (nor the sibling `_parseArguments` `new RequestArgument[]`).

The ESP8266 core is installed by `build.py` via arduino-cli from the board-manager package (`board_manager.additional_urls` -> the 2.7.4 package index). It lives under `arduino/` which is **gitignored** and **reinstalled clean on every CI run**. So a raw local edit to the core does not ship: CI would build against pristine, unpatched 2.7.4. The dev line is deliberately pinned to Core 2.7.4 (1.5.x LTS stability decision; see project memory and the partition/core decoupling note), and 2.7.4 is upstream-EOL (2020) so an upstream patch to 2.7.4 is not available.

The fix is in C++ **headers** (`DataSource.h`, `ClientContext.h` are inline/template code compiled into the sketch), so it requires no toolchain or precompiled-SDK rebuild.

## Decision

**Commit a unified-diff patch under `patches/` and have `build.py` re-apply it to the freshly-installed ESP8266 core after `core install`, on every build.**

- Patch `patches/esp8266-core-2.7.4-http-heap-nullcheck.patch` adds two NULL checks: `DataSource.h get_buffer()` returns `nullptr` when `new uint8_t[]` fails (instead of `memcpy`/`readBytes` into NULL); `ClientContext.h _write_some()` `break`s when `get_buffer()` returns NULL (treat as "come back later", the same contract lwIP `tcp_write` uses for `ERR_MEM`).
- `build.py::apply_core_patches()` applies every `patches/*.patch` to the installed core via `git apply -p1`. It is **idempotent** (reverse-apply `--check` detects already-applied and skips) and **fails the build loudly** if a patch does not apply (e.g. the core version changed), so we never silently ship an unpatched core.
- The firmware-side gate (TASK-843) is retained as defense-in-depth; the core patch removes the false-503 edge and covers the allocation at its source.

## Alternatives Considered

1. **Firmware-side gate only (TASK-843, status quo).** Rejected as the *sole* fix: portable and already shipped, but it treats the symptom (can refuse serves that would succeed) and leaves the unchecked core allocation in place. Kept as defense-in-depth, not as the complete answer.
2. **Fork ESP8266 Core 2.7.4.** Rejected: produces an identical compiled result to the build-time patch (the fix is headers-only) but requires owning/hosting a 200k-line core fork and a board-manager package, for a 2-line change. The patch is ~100x less maintenance with the same outcome; a fork only adds value for third-party redistribution, which this build.py/CI-driven project does not need.
3. **Vendor the whole core into the repo.** Rejected: the project deliberately does not vendor the core (`arduino/**` is gitignored); committing the core (plus toolchain/SDK blobs) is disproportionate to a header fix.
4. **Upstream the fix to esp8266/Arduino 2.7.4.** Rejected for this line: 2.7.4 is EOL; upstream is on 3.x and will not patch 2.7.4, and the dev line is pinned to 2.7.4. The fix should still be upstreamed on Core 3.x for the 2.0.0/ESP32 line (see Related).

## Consequences

Positive:
- Fixes the confirmed crash at its source: failed allocations degrade to "retry later" instead of writing through NULL.
- No false 503s; covers all `streamFile` callers uniformly; the same mechanism can carry future core fixes (e.g. `_parseArguments`).
- Headers-only, so no toolchain/SDK rebuild; the patch is small, reviewable, and committed in-repo for auditability.
- CI-safe: the build fails loudly if the core content drifts, surfacing any future core change instead of silently regressing.

Negative / risks:
- Introduces a maintained divergence from the stock pinned core: the patch must be kept applying. Mitigated by the loud-fail check and the core being version-pinned (content frozen).
- Adds a build step that depends on `git apply` (git is already required by the project and CI).
- If the board-manager core version is ever bumped, the patch context may need regeneration (the loud-fail check makes this obvious rather than silent).
- A second build path that flashes without `build.py` would not get the patch; the firmware-side gate (retained) covers that case.

## Related

- TASK-844 (this patch + build.py apply-step), TASK-843 (firmware-side `canServeHttp`/`streamFileGuarded` gate, 1.7.0-beta.3), TASK-841/837 (earlier heap crash-proofing).
- ADR-030 / ADR-083 (heap-health ladder and per-consumer gating), ADR-004 (no String in hot paths — the firmware itself is allocation-disciplined; the bug is in the core).
- Core files: `arduino/packages/esp8266/hardware/esp8266/2.7.4/libraries/ESP8266WiFi/src/include/DataSource.h`, `.../ClientContext.h`. Firmware call sites: `src/OTGW-firmware/FSexplorer.ino` (`streamFile`).
- Follow-up: upstream the NULL-check to esp8266/Arduino Core 3.x for the 2.0.0/ESP32 feature line.

## References

- Field report and bisect: project memory `project_george_crash_bisect_1_6_0`; transcripts `transcript-20260607/08-*-OTGW-otgw-C8C9A35ACB08.txt`.
- ROM symbol proof: `arduino/.../2.7.4/tools/sdk/ld/eagle.rom.addr.v6.ld` (`memcpy = 0x4000df48`; `epc1=0x4000df64` = +0x1C).
- Non-throwing `new[]`: `arduino/.../2.7.4/cores/esp8266/abi.cpp` (`operator new[]` -> `malloc`, returns NULL on OOM).
