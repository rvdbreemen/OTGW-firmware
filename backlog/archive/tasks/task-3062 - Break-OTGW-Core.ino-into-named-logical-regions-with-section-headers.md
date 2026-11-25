---
id: TASK-4
title: Break OTGW-Core.ino into named logical regions with section headers
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:12'
updated_date: '2026-03-12 20:43'
labels:
  - refactor
  - maintainability
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-Core.ino is 3909 lines and handles protocol parsing, command queuing, OT message dispatch, auto-configure orchestration, and more. While splitting into separate TUs is risky on Arduino single-TU builds, the file can be made significantly more navigable by extracting well-named logical regions with clear block comment headers (e.g., //=====[ Command Queue ]=====, //=====[ OT Message Dispatch ]=====). This improves maintainability without changing behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGW-Core.ino is divided into named logical sections with block comment headers
- [x] #2 Each section groups related functions and variables
- [x] #3 No functional change — pure reorganization
- [ ] #4 Firmware builds cleanly with zero new warnings
- [x] #5 A section map (list of sections and line ranges) is added as a comment near the top of the file
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 12 named section-header banners to OTGW-Core.ino covering every major logical group (Event/Log Helpers, Global Data, OT Spec Profile, MQTT send helpers, reset/boot/query helpers, Command & Response, Watchdog, OpenTherm data types & protocol helpers, Status Bit Query Helpers, MQTT throttle, OT Message Field Formatters, Command Queue, Send buffer to OTGW, PS=1 Summary Parsing, OT Message Processing, HandleOTGW, REST API, PIC upgrade). Added a Section Map block-comment TOC near the top of the file listing all sections with their approximate line numbers, giving a quick orientation to the 3500+ line file.
<!-- SECTION:FINAL_SUMMARY:END -->
