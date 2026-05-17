---
id: TASK-615
title: >-
  Fix: Update Firmware button hidden on touch-capable PCs in FSExplorer (GitHub
  #575)
status: To Do
assignee: []
created_date: '2026-05-17 07:07'
updated_date: '2026-05-17 07:07'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/575'
  - 'GitHub reporter: rkuijer'
  - '2026-05-15'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 'Update Firmware' button in FSExplorer is hidden on devices that report as touch-capable (pointer: coarse, hover: none) even when used as a desktop PC — e.g., Microsoft Surface Pro 9. Root cause identified by reporter rkuijer: the button's container uses class='desktop-only' which is hidden by the CSS media query '@media (pointer: coarse) and (hover: none)'. Maintainer (rvdbreemen) acknowledged the bug and committed to backlogging it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Update Firmware button is visible on touch-capable PCs (e.g. Surface Pro 9) running Chrome and Firefox
- [ ] #2 CSS fix does not regress the mobile-hide behaviour for genuinely small-screen touch devices (phones/tablets)
- [ ] #3 FSExplorer HTML/CSS change compiles cleanly (python build.py --firmware exits 0)
<!-- AC:END -->
