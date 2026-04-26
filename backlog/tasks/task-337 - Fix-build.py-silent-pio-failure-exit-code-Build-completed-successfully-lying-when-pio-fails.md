---
id: TASK-337
title: >-
  Fix build.py silent pio failure: exit code + 'Build completed successfully'
  lying when pio fails
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-19 19:44'
updated_date: '2026-04-26 18:01'
labels:
  - build
  - reliability
  - review-2026-04-18-followup
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
During the ADR-081 refactor session (2026-04-19) the project's .venv ran Python 3.14 while platform-espressif32/8266 requires 3.10-3.13. pio rejected all builds with 'Python version must be between 3.10 and 3.13'. Despite this, build.py continued through its pipeline, printed 'Build completed successfully!' in its output, and exited with code 0. Multiple 'verified builds' in that session were fake, relying on stale artifacts or unchecked exit codes. Bug: build.py does not propagate pio's non-zero exit into its own exit code, and does not gate the 'Build completed' message on pio success. Impact: CI/CD cannot trust build.py; users shipping from a mis-configured venv get silently broken artifacts. Fix: check pio exit code in build.py's Building firmware step and propagate failure to outer exit code + suppress 'Build completed' line on failure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.py propagates non-zero pio exit code to its own sys.exit
- [ ] #2 Building firmware step logs FAIL with full pio stderr when pio returns non-zero
- [ ] #3 'Build completed successfully!' message only printed when every platform's pio succeeded
- [ ] #4 Regression test (tests/test_build.py) asserts build.py fails fast on mis-configured Python version
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Root cause

Two interacting weaknesses in build.py:

1. **`build_firmware_pio` / `build_firmware` only check the subprocess exit code.** When pio fails because of mis-configured Python version, pio prints the error but exits 0 in some configurations (its banner is printed before the actual build is attempted). With `check=True`, run_command does not raise, so build.py proceeds.
2. **`collect_pio_artifacts` warns rather than fails when no `.pio/build/<env>/firmware.bin` is produced.** A real pio failure that produced no artifacts is not propagated.
3. **`main()` prints "Build completed successfully!" unconditionally** at line 1678 after the build loop ends, regardless of per-target outcomes.

## Fix design

Defense-in-depth at three layers:

1. **Artifact verification helper.** Add `_verify_artifact_exists(path, step_name)` that prints a clear error and `sys.exit(2)` if the expected build output does not exist. Call it immediately after each `pio run` and `arduino-cli compile` in:
   - `build_firmware_pio` (check `.pio/build/<env>/firmware.bin`)
   - `build_filesystem_pio` (check `.pio/build/<env>/littlefs.bin`)
   - `build_firmware` (check `<temp_build_dir>` glob `**/*.ino.bin`)
   - `build_filesystem` (output_file already implicitly checked, add explicit guard)
2. **`collect_pio_artifacts` strict.** Change the "No build artifacts found" warning into an error that exits non-zero. With (1) already in place this is belt-and-suspenders.
3. **"Build completed successfully" gating.** Track per-target success in `main()`. Only print the final success banner when every target collected artifacts successfully.

## Regression test (AC #4)

Add `tests/test_build.py` that uses `unittest.mock.patch` on `subprocess.run` to simulate pio returning the "Python version must be between 3.10 and 3.13" message with exit code 0, and asserts that `build_firmware_pio()` exits non-zero (because the artifact verification step fails). The test imports build.py as a module and exercises the relevant function with mocked subprocess and a synthetic project_dir.

## Verification

- Run `python build.py --firmware` on the current dev (1.5.0-beta+ff5c774 with arduino-cli) to confirm normal builds still pass.
- Run `python -m pytest tests/test_build.py` to confirm the regression test passes.
- Manually confirm that with a deliberately broken pio shim (`echo "Python version must be"; exit 0`) on PATH, `build.py` fails fast.

## Phase plan

1. Implement helper + verification calls in build.py (~30 lines).
2. Tighten `collect_pio_artifacts` (~5 lines).
3. Track and gate success banner (~10 lines).
4. Write `tests/test_build.py` (~80 lines).
5. Local validation.
6. Commit as one task-scoped change.
<!-- SECTION:PLAN:END -->
