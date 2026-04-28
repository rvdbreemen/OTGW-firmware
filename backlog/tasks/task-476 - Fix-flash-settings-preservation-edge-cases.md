---
id: TASK-476
title: Fix flash settings preservation edge cases
status: In Progress
assignee:
  - '@codex'
created_date: '2026-04-28 17:48'
updated_date: '2026-04-28 19:04'
labels:
  - flash scripts
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Resolve review findings in the cross-platform flash helpers so settings backup/restore is recoverable, non-interactive shell flashes keep working, and Windows preserve mode builds the prompted host URL correctly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Shell preservation keeps backup files on failed flash or restore and only removes them after restore success.
- [x] #2 Shell factory flow does not prompt or abort when stdin is non-interactive.
- [x] #3 Windows preserve-settings prompt uses the typed host when constructing HOST_BASE.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Patch flash_otgw.sh cleanup so backup directories are deleted only after RESTORE_DONE=1 and retained path is shown on failure. 2. Make the shell preserve prompt stdin-safe by defaulting to no preserve when stdin is non-interactive or read hits EOF. 3. Patch flash_otgw.bat prompted host handling to use delayed expansion inside the parenthesized block. 4. Run syntax/static checks for the touched scripts and update task status.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Patched flash_otgw.sh so backup cleanup only deletes after RESTORE_DONE=1, preserves and prints the backup directory on failed restore paths, and avoids preserve prompts when stdin is non-interactive.

Patched flash_otgw.bat to use delayed expansion for the prompted ARG_HOST when constructing HOST_BASE.

Verification: cmd /c flash_otgw.bat --help passed; Git Bash syntax check passed outside the sandbox after sandbox access-denied failures; git diff --check passed with only the existing CRLF warning for the batch file.

Broader PR-checklist attempt: evaluate.py --quick ran but exited 1 because current firmware sources report 15 PROGMEM violations; warnings were ADR-062 mqtt_configuratie.cpp not found and sendMQTTheapdiag buffer arithmetic body mismatch. This is outside the flash script changes.

Broader PR-checklist attempt: tests/test_evaluate.py and tests/test_build.py both passed.

Broader PR-checklist attempt: build.py failed in the ESP8266 PlatformIO link step with undefined references to setup and loop after only MQTTHaDiscovery.cpp and mqtt_discovery_verify.cpp objects were compiled under .pio/build/esp8266/src. Not marking Done while this checklist gate is red.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Resolved the flash preservation review findings: shell backups are retained unless restore completes, shell preserve prompts are stdin-safe for scripted factory flashes, and Windows preserve mode now uses the typed host with delayed expansion. Targeted checks passed: batch --help, bash -n via Git Bash, git diff --check, tests/test_evaluate.py, and tests/test_build.py. Full PR checklist is not green yet: evaluate.py --quick fails on existing PROGMEM violations, and build.py currently fails in ESP8266 link with missing setup/loop outside these flash-script edits.
<!-- SECTION:FINAL_SUMMARY:END -->
