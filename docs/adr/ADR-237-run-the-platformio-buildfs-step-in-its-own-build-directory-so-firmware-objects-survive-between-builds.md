---
id: "ADR-237"
title: "Run the PlatformIO buildfs step in its own build directory so firmware objects survive between builds"
status: "Accepted"
date: "2026-11-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-083"
  - "ADR-190"
topics:
  - "build tooling"
  - "incremental build"
  - "PlatformIO build directory"
  - "LittleFS image build"
aliases:
  - "every build is cold"
  - "buildfs wipes firmware objects"
  - "project.checksum"
components:
  - "buildfs build directory"
symbols:
  - "pio_fs_build_root"
  - "build_filesystem_pio"
  - "collect_pio_artifacts"
  - "PLATFORMIO_BUILD_DIR"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-237 Run the PlatformIO buildfs step in its own build directory so firmware objects survive between builds

## Status

Accepted, 2026-11-25, by Robert van den Breemen (interactive in the coordinator session, relayed). Guideline-level (ADR-080): verified by `tests/test_build.py` and the `require_pattern` rule in Enforcement.

**Decision Maker:** Agent (otgw-firmware-95) for TASK-1921, assigned by the coordinator; Robert (maintainer) accepted
this record on 2026-11-25.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent: Claude (otgw-firmware-95)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent: Claude (otgw-firmware-95)"
    reason: Related to ADR-190
    changed_via: adr-kit lifecycle
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent: Claude (otgw-firmware-95)"
    reason: Related to ADR-083
    changed_via: adr-kit lifecycle
  - date: 2026-11-25
    status: Accepted
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Accepted by Robert van den Breemen, 2026-11-25 (interactive in the coordinator session, about 18:45, relayed to otgw-architect); evidence rechecked on origin/dev a60386c99
    changed_via: adr-kit lifecycle
```

## Context

Every esp32-combo build on this machine was a full rebuild of 10 to 14 minutes, also when no library changed. That
cost every build slot, and the full LTO (link-time optimisation) memory peak in every build is where the memory reaper
stopped background build shells several times (TASK-1921).

PlatformIO (PIO Core 6.2.0, `.build-venv`) stores a checksum of the project configuration in
`<build_dir>/project.checksum` and removes the whole build directory when it differs: `clean_build_dir()`,
`platformio/run/helpers.py:26-45`, `fs.rmtree(build_dir)` at line 41. The checksum covers the configuration as JSON,
the PIO version and the list of source paths (`compute_project_checksum()`, `platformio/project/helpers.py:93`).

`build.py` builds the firmware with `pio run -e esp32-combo` and then the LittleFS image with
`pio run -e esp32-combo -t buildfs` and `PLATFORMIO_DATA_DIR` set to the minified staging copy (ADR-190). That override
changes the configuration, so the two runs have different checksums. Measured with the build venv on this tree:
`b16a76e10b81c30f051c3ec4742bda3469e0f4bc` without the override, `a52dde20dc6f340b1399c0a0192a8f41dbea4062` with it.
Each run wipes the other's tree. After build 2.0.0-beta.240, `.pio/build/project.checksum` held the buildfs checksum
and `.pio/build/esp32-combo` held only `littlefs.bin`. `collect_pio_artifacts()` already carried a workaround for the
symptom ("buildfs may clean parts of `.pio/build/<env>` and remove `firmware.bin`").

## Decision

The buildfs step runs with `PLATFORMIO_BUILD_DIR=.pio/build-fs` (`pio_fs_build_root()`, `build.py:2614-2621`), so
`.pio/build` keeps one checksum and its object files between builds; `build.py` verifies and collects `littlefs.bin`
from `.pio/build-fs/<env>/` (`build.py:2646`, `:2689`), `--clean` removes that directory too (`build.py:2758`), and
`flash_otgw.bat` looks there in its last fallback for a filesystem image.

## Decision Contract

### Must

- Run `pio run -t buildfs` with a build directory other than the firmware build directory whenever it sets an option
  override such as `PLATFORMIO_DATA_DIR`.
- Read the filesystem image from the buildfs build directory.
- Keep the build lock, the jobs cap and the artifact names in `build/` unchanged.

### Must Not

- Let a build step that changes the PlatformIO configuration run in `.pio/build`.
- Point `data_dir` in `platformio.ini` at the staging directory to hide the override.

### Exceptions

- `scripts/device-verify.sh` runs `pio run -t buildfs` itself without an override; it keeps the default directory.

### Verification

- `tests/test_build.py`, class `TestFilesystemStepKeepsFirmwareObjects`: the buildfs step gets its own
  `PLATFORMIO_BUILD_DIR` (fails on the `build.py` from before TASK-1921), the image is collected from there, and
  PlatformIO's own `compute_project_checksum()` differs with the data-dir override (the cause).
- Two consecutive esp32-combo builds without a source change: the second shows far fewer `Compiling` lines than the
  first, with the same filesystem image (TASK-1921 AC3).

**Evidence at acceptance (2026-11-25, otgw-architect, origin/dev a60386c99):**

- `tests/test_build.py` class `TestFilesystemStepKeepsFirmwareObjects` (run from `tests/` with `python -m unittest`): Ran 3 tests, OK. TASK-1921 recorded the core test failing on the `build.py` from before the fix.
- TASK-1921 AC3 (2026-11-14, 36fb13763): cold build 609 s with 364 `Compiling` lines; warm build without a source change 253 s with 1 `Compiling` line (the sketch, restamped `version.h`); 364 object files kept; the littlefs files identical.
- Builds of 2026-11-25: the build trees `wt-d3-rel` (buildfs checksum written 18:35) and `wt-d3-fr` (17:16) hold `.pio/build-fs/project.checksum` beside 364 object files in `.pio/build/esp32-combo`, so the firmware objects survived the filesystem step.

## Alternatives Considered

- **Set `data_dir` in `platformio.ini` to the staging directory.** Rejected: the override disappears, but a plain
  `pio run -t buildfs` or `uploadfs` by hand would then read a staging copy that may be missing or stale, and the
  staging copy is a build product (ADR-190), not the source.
- **Copy the staged files into `src/OTGW-firmware/data/` for the buildfs run.** Rejected: the build would write into
  the source tree.
- **Restore `project.checksum` after the buildfs run.** Rejected: it fights PlatformIO's own rule and still loses the
  objects, because the buildfs run already removed them.
- **Do nothing.** Rejected: every build stays cold at 10 to 14 minutes with the full memory peak.

## Consequences

**Positive:**

- A build without library changes recompiles only the sketch and what changed; the measured counts are recorded in
  TASK-1921.
- `firmware.bin` and `firmware.elf` stay in `.pio/build/esp32-combo` after the buildfs step.

**Negative:**

- One more directory, `.pio/build-fs` (a small tree: `littlefs.bin` and the PlatformIO metadata), outside `.pio/build`.
- A tool that looks for `.pio/build/<env>/littlefs.bin` directly finds no image; `flash_otgw.bat` is updated, and
  `build/` stays the place for artifacts.
- A warm build reuses library objects. A stale object is the classic risk of incremental builds; PlatformIO and SCons
  track the dependencies, and `build.py --clean` stays the way to force a cold build.

## Open Questions

None.

## Related Decisions

- **ADR-190 (Build the LittleFS image from a minified staging copy of the web assets)**: the staging copy and the
  `PLATFORMIO_DATA_DIR` override this record keeps.
- **ADR-083 (PlatformIO as primary build system)**: the build system whose build-directory rule this works with.

## References

- TASK-1921.
- `build.py:2614-2621` (`pio_fs_build_root`), `:2624` (`build_filesystem_pio`, `PLATFORMIO_BUILD_DIR` at `:2639`), `:2689` (collect), `:2749`/`:2758` (`clean_build`), at origin/dev a60386c99 (the line numbers of 2026-11-14 were `:2488`, `:2498`, `:2555`, `:2616`).
- `.build-venv/Lib/site-packages/platformio/run/helpers.py:26-45`, `platformio/project/helpers.py:93` (PIO Core 6.2.0).
- `flash_otgw.bat:403` (fallback for a filesystem image).

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "PLATFORMIO_BUILD_DIR",
      "path_glob": "build.py",
      "message": "ADR-237: the buildfs step runs in its own build directory, or every build is cold."
    }
  ],
  "llm_judge": false,
  "llm_judge_reason": "One environment variable on one subprocess call, guarded by tests/test_build.py."
}
```
