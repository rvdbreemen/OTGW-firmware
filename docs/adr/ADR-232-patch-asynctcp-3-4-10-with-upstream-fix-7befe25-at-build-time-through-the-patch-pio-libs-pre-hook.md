---
id: "ADR-232"
title: "Patch AsyncTCP 3.4.10 with upstream fix 7befe25 at build time through the patch_pio_libs pre-hook"
status: "Proposed"
date: "2026-11-13"
binding: false
gate: "tests/test_asynctcp_accept_patch.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "build"
  - "asynctcp"
  - "lwip"
  - "library-patch"
  - "boot-stability"
aliases:
  - "tcp_accept ERR_OK after tcp_abort"
  - "AsyncTCP 7befe25"
  - "TASK-1746.1"
components:
  - "Build-time patch of the AsyncTCP library copy in .pio/libdeps (scripts/patch_pio_libs.py)"
symbols:
  - "AsyncTCP_detail::tcp_accept"
  - "tcp_abort"
  - "ERR_ABRT"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-232 Patch AsyncTCP 3.4.10 with upstream fix 7befe25 at build time through the patch_pio_libs pre-hook

Terms used below: TCP (Transmission Control Protocol), IP (Internet Protocol), lwIP (the lightweight IP stack inside ESP-IDF), ESP (Espressif's chip family prefix), pcb (lwIP protocol control block, one per connection), IDF (ESP-IDF, the Espressif IoT Development Framework), PIO (PlatformIO, the build system), SHA (a git commit hash), LGPL (Lesser General Public License), DM (Discord direct message), ELF (the firmware image with symbols).

## Status

Proposed, 2026-11-13.

**Decision Maker:** to be decided by User: Robert van den Breemen (maintainer), by DM through the coordinator. Robert's instruction to the bench-tester for TASK-1746, "check upstream ESP32Async/AsyncTCP; if fixed there, use that, otherwise patch" (relayed by the coordinator, 2026-11-12), is the approval for a library change in principle. This record asks him to accept how the patch is carried.

## Status History

```yaml
status_history:
  - date: 2026-11-13
    status: Proposed
    changed_by: "Agent: otgw-firmware-95"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

**The bug.** The firmware pins AsyncTCP (ESP32Async) 3.4.10 in platformio.ini. Its `AsyncTCP_detail::tcp_accept`, the lwIP accept callback, ends in `tcp_abort(pcb); return ERR_OK;` when there is no `onConnect` callback or when allocating the `AsyncClient` fails (v3.4.10 `src/AsyncTCP.cpp`, about lines 1584-1585). lwIP reads `ERR_OK` as "connection delivered" and goes on into `tcp_receive()` on a pcb that `tcp_abort()` has just freed. Any return value other than `ERR_ABRT` makes it abort the pcb a second time. TASK-1746 records this risk; on beta.215 the bench-tester saw a panic in `tcp_accept` during a reboot teardown (1 in 12 /ReBoot cycles, ELF-decoded).

**Upstream.** Checked by the bench-tester on 2026-11-12 and re-checked in a clone of `https://github.com/ESP32Async/AsyncTCP` on 2026-11-13:

- The latest release, v3.5.0, still has the same code (`src/AsyncTCP.cpp:1605-1610`).
- Upstream `main` fixes it in commit 7befe25 ("fix: Report an aborted connection to LwIP as aborted", 7befe25a549c71b0fc7794596ba9d21fa6fd9d1f, 2026-09-10). It returns `ERR_ABRT` on every path that calls `tcp_abort()`, and allocates the event before the client so a client never sits on a failure path.
- 7befe25 applies cleanly to v3.4.10 (one hunk, offset -21 lines). The `AsyncClient(tcp_pcb *)` constructor and `_free_event()` are identical in v3.4.10 and in 7befe25's parent, so the branch 7befe25 removes ("pcb has already been aborted") was dead code in 3.4.10 too, and freeing an accept event whose client is still null is safe.
- Two later commits that the bench-tester named, 4192390 and d233104 (accept delivery decided under the queue mutex, `end()` disowning queued accepts), sit on about 50 commits of refactor after v3.4.10 (4 files, +1325/-562 lines in `src/`). They do not apply to 3.4.10.

**What 7befe25 covers and what it does not.** It covers the allocation-failure and missing-callback paths. The bench-tester's reading of the beta.215 crash (64736 B free heap at that teardown) is that the crash more likely came from an accept on a server being torn down, d233104's case. That reading is not proven. The firmware already narrows that case: since beta.219, `prepareForReboot()` stops the port-80 listener before the teardown (TASK-1746).

**How the library arrives.** AsyncTCP comes from the PlatformIO registry into `.pio/libdeps/<env>/AsyncTCP`. ESPAsyncWebServer 3.11.2 declares its own dependency `ESP32Async/AsyncTCP ^3.4.10`. PlatformIO installs lib_deps before SCons and the `pre:` extra_scripts run (`platformio/run/processor.py:81`), and `libdeps/<env>/integrity.dat` lists only the dependency specs, not file hashes (`platformio/package/commands/install.py:273-290`). The project already has a pre-hook for build fixes, `scripts/patch_pio_libs.py` (ADR-083, `platformio.ini:29`), which once patched NetApiHelpers and now holds only a tool-path fix. `tests/test_reboot_listener_and_reason.py` already reads the libdeps `AsyncTCP.cpp`.

**Project rule.** Library code is not changed without a confirmed upstream bug and the maintainer's approval (agent memory "No library changes without explicit approval"). This case meets both: the bug is fixed upstream in 7befe25 but not released, and Robert gave the instruction above.

## Decision

Carry 7befe25 as a build-time patch on the registry copy of AsyncTCP 3.4.10, applied by `scripts/patch_pio_libs.py`:

1. The repo stores the upstream commit as a patch file, `scripts/patches/asynctcp-3.4.10-7befe25.patch`, with the commit SHA and its web address in its header.
2. Before each build of an esp32 environment, the pre-hook hashes `.pio/libdeps/<env>/AsyncTCP/src/AsyncTCP.cpp` with line endings normalized:
   - equal to v3.4.10 (`10b7124e…48da`): apply the patch, then check the result;
   - equal to v3.4.10 + 7befe25 (`c786989e…e4e5`): already patched, do nothing;
   - anything else: stop the build with an error that names this ADR. A changed pin or an upstream file that moved past 3.4.10 fails loudly instead of building unpatched.
3. After applying, the file must hash to `c786989e…e4e5`, or the build stops.

The patch stays until an upstream release contains 7befe25. v3.5.0 does not. ESPAsyncWebServer 3.12 needs AsyncTCP 3.5.0, so dropping the patch is a combined AsyncTCP and ESPAsyncWebServer upgrade with its own task, not a pin bump.

d233104 and 4192390 are not taken: they need the upstream refactor around them, and a hand-port would be our own code against the old structure with no upstream review.

## Decision Contract

### Must

- Apply exactly upstream commit 7befe25 to AsyncTCP 3.4.10 in every esp32 environment, from the stored patch file.
- Stop the build when the libdeps `AsyncTCP.cpp` matches neither the 3.4.10 hash nor the patched hash, and when the patched result does not hash to `c786989e…e4e5`.
- Print one line per build that says whether the patch was applied or already present.
- Keep a host test on the real `tcp_accept` that fails on unpatched 3.4.10 and passes on the patched file.

### Must Not

- Edit any other part of AsyncTCP, or any other library, under this record.
- Vendor AsyncTCP into `src/libraries` while ESPAsyncWebServer depends on it from the registry.
- Silently build an unpatched or unknown AsyncTCP version.

### Exceptions

- None.

### Verification

- `tests/test_asynctcp_accept_patch.py`:
  - slices the real `tcp_accept` from the libdeps file and stubs lwIP, the server and the event queue;
  - makes the event allocation, the client allocation and the missing `onConnect` callback each fail in turn;
  - asserts the lwIP contract on every path that calls `tcp_abort`: the return is `ERR_ABRT`, and `tcp_abort` is called at most once;
  - is red with `OTGW_ASYNCTCP_CPP` pointing at the unpatched v3.4.10 file, green on the patched file;
  - also checks the hash logic of the pre-hook against an unknown file.
- Build: one esp32-combo build log shows the patch line, and only one `AsyncTCP.cpp` object is compiled.
- Bench by the bench-tester: at least 20 /ReBoot cycles and a connection storm on .61, with no tiT panic and the redirect page still arriving. A storm rarely makes an allocation fail inside `tcp_accept`, so the host test proves the mechanism; the bench is a regression check.

## Alternatives Considered

- **Vendor AsyncTCP into `src/libraries/AsyncTCP` with the patch (the ADR-186/187 espMqttClient way).** Rejected for now. espMqttClient had no dependent library; ESPAsyncWebServer 3.11.2 depends on `ESP32Async/AsyncTCP ^3.4.10` and would still pull the registry copy into libdeps. That gives two `AsyncTCP` libraries and either a duplicate-symbol link or the wrong copy chosen without notice. It also moves 2193 lines of LGPL-3.0 library code (`src/`) into the repo for a change of 31 lines added and 31 removed.
- **Track upstream `main` at or after d233104.** Rejected: an unreleased refactor (+1325/-562 lines in `src/` between v3.4.10 and d233104), with `abort()` semantics that ESPAsyncWebServer 3.11.2 does not expect (3.12 needs AsyncTCP 3.5.0).
- **Hand-port d233104 onto 3.4.10.** Rejected: it is no longer upstream's code. The race it closes is narrowed on our reboot path by the listener stop of TASK-1746.
- **Wait for an upstream release.** Rejected: v3.5.0 does not contain the fix and there is no release date; the use-after-free stays possible meanwhile.
- **Do nothing.** Rejected by Robert's instruction "otherwise patch".

## Consequences

**Positive:**

- The lwIP use-after-free on the allocation-failure and missing-callback paths of `tcp_accept` is removed, with upstream's own code.
- The diff in the repo is the upstream patch itself (31 lines added, 31 removed, against 2193 lines in AsyncTCP's `src/` and a 41 KB `AsyncTCP.cpp`), easy to compare with upstream and easy to drop.
- A pin change or a moved upstream file fails the build at once, so the patch can never silently stop applying.

**Negative:**

- The library copy in `.pio/libdeps` no longer matches the registry package. Mitigation: the pre-hook prints the state each build, and the hash check catches drift.
- The beta.215 teardown crash may have a different cause than this patch fixes (accept on a server being torn down). Mitigation: the listener stop of TASK-1746 stays; d233104 is reconsidered with the combined upgrade.

## Open Questions

- [ ] Does Robert accept carrying 7befe25 as a hash-checked build-time patch on the registry copy (this decision), or does he prefer one of the alternatives?

## Related Decisions

- **ADR-083 (PlatformIO as the primary build system):** introduced the `scripts/patch_pio_libs.py` pre-hook that this record reuses.
- **ADR-186 and ADR-187 (espMqttClient vendored and tracking upstream):** the vendoring precedent, not followed here because of the transitive dependency.
- **ADR-139 and ADR-147 (AsyncTCP task and queue configuration):** unchanged.

## References

- TASK-1746 (the risk, the listener stop in beta.219, the bench evidence), TASK-1746.1 (this change).
- https://github.com/ESP32Async/AsyncTCP/commit/7befe25a549c71b0fc7794596ba9d21fa6fd9d1f
- https://github.com/ESP32Async/AsyncTCP/commit/d233104 and https://github.com/ESP32Async/AsyncTCP/commit/4192390 (not taken)
- AsyncTCP v3.4.10 (tag commit 0f5b54654a0a), `src/AsyncTCP.cpp` sha256 (line endings normalized) `10b7124eea6b9b0b8734d058d480219f81e2663ddfd958eb6be27241bac048da`; with 7befe25 `c786989e25a4372d7a0c855910d84a823c1e3a6e011c5b7a052a31214044e4e5`.
- `platformio.ini` lib_deps (`ESP32Async/AsyncTCP @ 3.4.10`, `ESP32Async/ESPAsyncWebServer @ 3.11.2`) and `extra_scripts` (`pre:scripts/patch_pio_libs.py`).
- PlatformIO Core: `platformio/run/processor.py:81`, `platformio/package/commands/install.py:273-290`.
