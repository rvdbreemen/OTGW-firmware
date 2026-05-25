---
id: TASK-619
title: Fix 2 pre-existing PROGMEM flash-string violations failing evaluate.py CI gate
status: To Do
assignee: []
created_date: '2026-05-17 13:23'
updated_date: '2026-05-25 22:35'
labels:
  - bug
  - firmware
  - progmem
  - ci
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
python evaluate.py --quick fails the '[PROGMEM] Flash string compliance' check with 'Found 2 PROGMEM violations (wastes RAM)'. The evaluate.py CI workflow (.github/workflows/evaluate.yml) has a gate step that exits 1 on ANY evaluate.py failure, so this single pre-existing firmware-lint failure red-gates EVERY PR to the dev line (observed on docs-only PR #590). The same failure is present on the 2.0.0 base branch (a sibling port task is recommended). These are string literals not kept in flash, per the project PROGMEM-mandatory rule (ESP8266 ~40KB RAM). Locate and fix both so the literals live in flash (F()/PSTR()/PROGMEM/snprintf_P as appropriate) with no behaviour change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 2 PROGMEM violations flagged by 'python evaluate.py --quick' on dev are located and their file:line documented in the task implementation notes
- [x] #2 Each violation fixed so the string literal is stored in flash per the project PROGMEM rules, with no functional/behavioural change
- [x] #3 'python evaluate.py --quick' reports 0 failed checks on dev (the [PROGMEM] Flash string compliance check passes)
- [x] #4 'python build.py --firmware' exits 0 (firmware compiles clean)
- [ ] #5 The implementing commit bumps _VERSION_PRERELEASE per the versioning policy (staged paths include src/OTGW-firmware/**)
<!-- AC:END -->
