---
id: TASK-337
title: >-
  Fix build.py silent pio failure: exit code + 'Build completed successfully'
  lying when pio fails
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 19:44'
updated_date: '2026-04-26 18:13'
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
- [x] #1 build.py propagates non-zero pio exit code to its own sys.exit
- [x] #2 Building firmware step logs FAIL with full pio stderr when pio returns non-zero
- [x] #3 'Build completed successfully!' message only printed when every platform's pio succeeded
- [x] #4 Regression test (tests/test_build.py) asserts build.py fails fast on mis-configured Python version
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## Summary

Fixed build.py's silent-success failure mode where pio's pre-flight Python version rejection ("Python version must be between 3.10 and 3.13") prints to stdout, exits 0, and produces no artifacts, causing build.py to print "Build completed successfully!" on a fake build.

## Changes

**`build.py`** (~50 lines):

- New helper `verify_artifact_exists(path, step_name, glob_pattern=None)` that calls `sys.exit(2)` with a clear error message when the expected build output is missing.
- Verification calls added after every build subprocess:
  - `build_firmware` (arduino-cli, glob `**/*.ino.bin` in temp_build_dir)
  - `build_filesystem` (mklittlefs, exact `output_file`)
  - `build_firmware_pio` (`.pio/build/<env>/firmware.bin`)
  - `build_filesystem_pio` (`.pio/build/<env>/littlefs.bin`)
- `collect_pio_artifacts`: `print_warning` for empty collected list replaced by `print_error` + `sys.exit(2)`.
- Comment at the success-banner print site documents the implicit gating: reaching that line requires every per-target step to have produced its artifacts.

**`tests/test_build.py`** (+90 lines, new `TestArtifactVerificationFailFast` class):

- 4 unit tests on `verify_artifact_exists` (file present/missing, glob match/no-match) verify the exit-2 contract.
- `test_build_firmware_pio_fails_fast_on_silent_pio` is the AC #4 regression: mocks `subprocess.run` to simulate pio printing the version-rejection message and exiting 0 without producing `firmware.bin`, asserts `build_firmware_pio` exits 2.

## Verification

- `python tests/test_build.py`: 9/9 OK (5 new TASK-337 tests + 4 pre-existing smoke tests).
- `python build.py --firmware` on 2.0.0 branch produces ESP8266 + ESP32-S3 artifacts via PlatformIO, exits 0, banner printed.

## Acceptance criteria

- **AC #1 (propagate non-zero pio exit)**: already via `run_command(check=True)`; new `verify_artifact_exists` is belt-and-suspenders for the silent-success case where pio exits 0 without producing artifacts.
- **AC #2 (log FAIL with stderr)**: existing `run_command` `CalledProcessError` handler prints stderr on non-zero; new helper prints a structured error with actionable hint ("This usually means the build tool printed an error and exited 0...").
- **AC #3 (success banner only on full success)**: gated via early-exit on every failure path; explicit invariant comment added at the print site.
- **AC #4 (regression test)**: `tests/test_build.py::TestArtifactVerificationFailFast::test_build_firmware_pio_fails_fast_on_silent_pio` mocks the silent-pio failure mode end-to-end.

## Branch context

Committed to `feature-dev-2.0.0-otgw32-esp32-sat-support` (commit `14299fdf`). The fix is contextually correct here because the PlatformIO failure mode that motivated the task lives on this branch. The same defensive pattern would benefit `dev`'s arduino-cli-only build.py: a follow-up port to `dev` is recommended but out-of-scope for this task. Tracking that as a future cherry-pick consideration.
<!-- SECTION:FINAL_SUMMARY:END -->
