---
id: TASK-525
title: Port native build wrappers to feature 2.0.0 branch
status: Done
assignee:
  - '@codex'
created_date: '2026-05-03 10:17'
updated_date: '2026-05-03 16:42'
labels:
  - build branch-2.0.0 port
dependencies: []
references:
  - build.bat
  - build.sh
  - build.py
  - docs/guides/BUILD.md
  - .gitattributes
  - .gitignore
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port commit c0a2ac72 from dev to the feature 2.0.0 branch once work switches there. The feature adds native build entrypoints so Windows users can run build and Linux/macOS users can run ./build.sh without knowing how Python is invoked. Keep the port scoped to the build-wrapper feature and do not mix unrelated 2.0.0 changes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 feature-dev-2.0.0-otgw32-esp32-sat-support contains the native Windows build.bat wrapper from dev commit c0a2ac72 or an equivalent branch-appropriate port.
- [x] #2 feature-dev-2.0.0-otgw32-esp32-sat-support contains the executable Linux/macOS build.sh wrapper from dev commit c0a2ac72 or an equivalent branch-appropriate port.
- [x] #3 The wrappers use or create a local build Python environment, quietly install requirements-build.txt or requirements.txt when present, pass all arguments through to build.py, and avoid interactive prompts.
- [x] #4 Build documentation and build.py help examples on the 2.0.0 branch advertise build on Windows and ./build.sh on Linux/macOS instead of requiring users to call Python directly.
- [x] #5 Wrapper smoke checks pass on the 2.0.0 branch: cmd /c build --help and bash -n build.sh, or documented platform-equivalent checks if one shell is unavailable.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Compare dev commit c0a2ac72 build-wrapper changes against the current 2.0.0 branch.
2. Port only the wrapper-related files: build.bat, build.sh, build.py help text, docs/guides/BUILD.md, .gitattributes, and .gitignore.
3. Run wrapper smoke checks for Windows and POSIX syntax.
4. Mark task ACs complete, add implementation notes/final summary, and leave unrelated dirty files untouched.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Ported native build wrappers to the 2.0.0 feature branch.
- Adjusted wrappers for the 2.0.0 PlatformIO default by prepending the active venv Scripts/bin directory to PATH before running build.py.
- Updated build.py help and docs/guides/BUILD.md to advertise build on Windows and ./build.sh on Linux/macOS while preserving 2.0.0 --target esp8266/esp32 and PlatformIO/arduino-cli backend options.
- Verification: cmd /c build --help passed; Git Bash syntax check `bash -n build.sh` passed; .venv\Scripts\python.exe tests\test_build.py passed 9 tests.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the native build wrapper feature from dev commit c0a2ac72 to the feature-dev-2.0.0-otgw32-esp32-sat-support branch.

Changes:
- Added build.bat for Windows Command Prompt and executable build.sh for Linux/macOS so users can invoke the build without calling Python directly.
- The wrappers use/create a local .build-venv, ensure pip is available, quietly install requirements-build.txt or requirements.txt when present, prepend the active venv Scripts/bin directory to PATH, and pass all arguments through to build.py.
- Updated build.py help examples and docs/guides/BUILD.md for the 2.0.0 PlatformIO default while preserving --target esp8266/esp32 and arduino-cli backend guidance.
- Added .build-venv/ to .gitignore; .gitattributes already preserved LF shell scripts and CRLF batch files on this branch.

Tests:
- cmd /c build --help
- Git Bash syntax check: bash -n build.sh
- .venv\Scripts\python.exe tests\test_build.py

Risk:
- Full firmware build was not rerun because the port changes wrapper/help/documentation behavior and the targeted build test suite plus wrapper smoke checks covered the changed surface.
<!-- SECTION:FINAL_SUMMARY:END -->
