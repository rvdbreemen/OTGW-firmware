---
id: TASK-305
title: >-
  [ARCH-M1] Remove tracked networkStuff.h.tmp orphan and add .tmp/.bak to
  gitignore
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:20'
updated_date: '2026-04-18 21:02'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
src/OTGW-firmware/networkStuff.h.tmp is a pre-ADR-061 snapshot shipped via commit 00c91f16. Still declares ESP8266*.h directly, violates platform abstraction.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 git rm src/OTGW-firmware/networkStuff.h.tmp
- [x] #2 .gitignore adds *.tmp and *.bak patterns
- [x] #3 Repo grep confirms no other .tmp shadows remain
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
git rm src/OTGW-firmware/networkStuff.h.tmp; .gitignore extended with *.tmp and *.bak patterns so future tool-generated shadows stay out of git. No other .tmp files found in working tree.
<!-- SECTION:FINAL_SUMMARY:END -->
