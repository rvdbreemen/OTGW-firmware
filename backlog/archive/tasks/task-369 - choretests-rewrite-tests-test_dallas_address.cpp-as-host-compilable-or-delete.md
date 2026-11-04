---
id: TASK-369
title: >-
  chore(tests): rewrite tests/test_dallas_address.cpp as host-compilable or
  delete
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:48'
updated_date: '2026-04-21 17:12'
labels:
  - code-review
  - tests
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 3A LOW (also flagged by external codebase review): the orphaned Dallas test assumes Arduino.h and Serial and has no Makefile/CMake/PlatformIO hook. It cannot run in CI. Either rewrite as host-compilable (gcc, no Arduino deps) establishing the pattern for future host tests, or delete to reduce repo noise.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Either option (a): test rewritten to compile and run with plain gcc/g++ (no Arduino.h, stub DeviceAddress typedef, stub pgm_read_byte)
- [ ] #2 Or option (b): test file deleted with a commit message explaining why (orphaned, no hook)
- [x] #3 If (a) chosen: a Makefile snippet or README instruction demonstrates how to run the host test
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read existing test (done)
2. Decision: Option A. getDallasAddress is pure byte conversion - only needs uint8_t, pgm_read_byte, PROGMEM. No Arduino runtime deps (no millis/delay/OneWire/Serial in function itself).
3. Rewrite test: replace <Arduino.h> with <cstdio>/<cstdint>/<cstring>; stub PROGMEM (empty), pgm_read_byte (deref), replace setup()/loop() with main() that prints pass/fail and returns non-zero on any failure; remove Unicode check/cross (not portable across terminals).
4. Add tests/README.md with compile+run instructions.
5. Verify: g++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out && ./tests/test_dallas_address.out; exit=0 expected.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Chose Option A. The function under test (getDallasAddress) is pure byte-to-hex conversion with no Arduino runtime dependencies: no millis/delay/Serial/OneWire in the function body. The only Arduino-isms were PROGMEM and pgm_read_byte, both trivially and honestly stubbed on the host (PROGMEM no-op, pgm_read_byte as plain deref, since on the host flash and RAM are the same address space).
- Replaced <Arduino.h> with <cstdint>/<cstdio>/<cstring>. Dropped setup()/loop() in favor of main() returning 0/1 on pass/fail so it is usable by CI and humans.
- Removed the non-ASCII check/cross glyphs to keep output portable across terminals (the original used literal U+2713/U+2717 inline in the source).
- Added tests/README.md with g++/clang++/MSVC build commands and expected output, satisfying AC #3.
- Sanity-checked the algorithm with a Python reimplementation (all 5 vectors match). No host C++ compiler is available in this environment (no g++/gcc/clang/cl on PATH, MinGW not installed), so the compile+run verification command in the task description could not be executed here. The logic is a verbatim copy of the firmware implementation plus a 2-line stub; any standard host toolchain will accept it.

- Kept scope strictly to tests/ per file-ownership rules. No src/ touched. No .gitignore change (out of scope); the README notes the .out/.exe artifacts are not currently ignored.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rewrote tests/test_dallas_address.cpp as a host-compilable unit test and added tests/README.md explaining how to build and run host tests.

## Why

The file was an orphaned Arduino sketch: depended on Arduino.h and Serial, had no Makefile/CMake/PlatformIO hook, and could not run in CI. Phase 3A and the external codebase review both flagged it. Because getDallasAddress is pure byte-to-hex logic with no Arduino runtime dependencies (no millis, delay, Serial, or OneWire inside the function), Option A (rewrite) was strictly better than Option B (delete): same verification value, plus it establishes a reusable host-test pattern for future pure-logic checks (e.g., the getHeapHealth hysteresis test mentioned in TASK-368).

## Changes

- tests/test_dallas_address.cpp: drop <Arduino.h>; include <cstdint>/<cstdio>/<cstring>; stub PROGMEM as a no-op and pgm_read_byte as a plain deref (honest equivalent on the host, where flash and RAM share one address space); replace setup()/loop() with main() that runs the same 6 cases and returns 0 on pass, 1 on failure; drop non-ASCII glyphs for portable terminal output. Function-under-test body kept verbatim from the firmware so the test stays a faithful check.
- tests/README.md (new): documents the tests/ directory, gives g++/clang++/MSVC build commands, expected output, exit-code contract, and a short guide for adding future host tests.

## Verification

- Python reimplementation of the algorithm (separate, independent) matches all 5 vectors: 28FF641E8216C3A1, 0102030405060708, all-zeros, all-0xFF, AABBCCDDEEFF1122. This confirms the test vectors themselves are correct.
- The compile+run verification command (g++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out && ./tests/test_dallas_address.out) could not be executed here: no g++/gcc/clang/cl found on PATH in this environment, and no MinGW is installed. The test is a ~30-line direct port with two trivial stubs; any standard host toolchain will accept it. Anyone running the README command locally will get the compile + run verification.

## Risks / follow-ups

- None for the firmware: no src/ changes, python build.py --firmware is unaffected.
- tests/*.out and tests/*.exe are not in .gitignore. The README flags this; adding patterns is a one-line follow-up if desired but was out of scope for this task (file-ownership: tests/ only).
- If getDallasAddress is ever changed in the firmware, the copy in the test must be updated to match. A future improvement is to extract the function into a header that both the firmware and the test can include, removing the duplication.
<!-- SECTION:FINAL_SUMMARY:END -->
