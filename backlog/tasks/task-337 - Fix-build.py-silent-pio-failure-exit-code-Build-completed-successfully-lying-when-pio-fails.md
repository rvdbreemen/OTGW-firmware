---
id: TASK-337
title: >-
  Fix build.py silent pio failure: exit code + 'Build completed successfully'
  lying when pio fails
status: To Do
assignee: []
created_date: '2026-04-19 19:44'
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
