---
id: TASK-1754
title: 'Wiki: capture instructions for 1.x and 2.x side by side'
status: Done
assignee: []
created_date: '2026-11-09T12:04:00Z'
updated_date: '2026-11-09T12:06:00Z'
labels:
  - docs
  - wiki
dependencies: []
priority: high
ordinal: 856000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Record task. Robert's direct order to 97: put the capture instructions for 1.x and 2.x side by side in the wiki, and say that no analysis happens without a capture. Done in wiki commit c45634c on rvdbreemen/OTGW-firmware.wiki (Robert van den Breemen, 2026-11-09 12:38 +0100, 'Capture instructions for 1.x and 2.x side by side; no analysis without a capture'). It changes How-to-debug-the-OTGW-firmware.md, NL-Hoe-de-OTGW-firmware-te-debuggen.md, Frequently-Asked-Questions.md and NL-Veelgestelde-vragen.md (62+/20-).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Wiki commit c45634c contains the side-by-side 1.x/2.x capture instructions (EN and NL)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 verified by system32-78: the commit exists in a clone of the wiki repo (git show --stat c45634c: 4 files, 62 insertions, 20 deletions). https://github.com/rvdbreemen/OTGW-firmware/wiki/How-to-debug-the-OTGW-firmware and .../NL-Hoe-de-OTGW-firmware-te-debuggen answer HTTP 200. The page content was not checked line by line.
<!-- SECTION:NOTES:END -->
