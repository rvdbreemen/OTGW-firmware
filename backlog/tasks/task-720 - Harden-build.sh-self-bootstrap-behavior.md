---
id: TASK-720
title: Harden build.sh self-bootstrap behavior
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-26 17:18'
updated_date: '2026-05-26 17:34'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Run build.sh in WSL, fix failures, and ensure it bootstraps required dependencies without user intervention.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.sh runs from a clean WSL environment without manual installs
- [ ] #2 Any missing dependency is auto-installed or clearly auto-downloaded by script
- [ ] #3 Firmware+filesystem image build completes successfully in WSL
- [x] #4 Documented fixes and validation are recorded in task notes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Run build.sh inside WSL and capture the first failing bootstrap point.\n2. Patch build.sh to self-bootstrap Python/pip locally when system Python is unavailable.\n3. Re-run build.sh in WSL and fix any additional dependency/bootstrap failures.\n4. Validate successful image generation and record outcomes in task notes.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
$- Patched build.sh with unattended WSL bootstrap paths: local Miniconda runtime fallback (.build-python) when Python 3 is missing, curl/wget download abstraction, and pip bootstrap fallback (ensurepip -> get-pip.py).\n- Verified shell syntax: sh -n build.sh (pass).\n- Verified post-patch WSL execution reaches full PlatformIO compile flow (toolchain/framework install + object compilation) using ./build.sh --target esp8266.\n- Validation still pending for a fully clean no-python WSL host and full end-to-end completion of firmware+filesystem output in this session.
<!-- SECTION:NOTES:END -->
