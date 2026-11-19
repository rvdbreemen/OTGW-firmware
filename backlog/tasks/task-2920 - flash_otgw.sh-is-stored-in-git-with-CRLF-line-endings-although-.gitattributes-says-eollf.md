---
id: TASK-2920
title: >-
  flash_otgw.sh is stored in git with CRLF line endings although .gitattributes
  says eol=lf
status: Done
assignee: []
created_date: '2026-11-19T16:08:00Z'
updated_date: '2026-11-19T18:33:00Z'
labels:
  - tools
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1058000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-19 (TASK-2028): git show origin/dev:flash_otgw.sh has a CR on 499 of 499 lines (after TASK-2028: 586 of 586), while git check-attr gives eol: lf, text: set. The blob was committed with CRLF, so the attribute does not convert it. No user impact today: the working copy is LF and build.py packs the script from it; the flash zip of 2.0.0-beta.391+40f511a holds flash_otgw.sh with 0 CR. Risk: anyone taking the script straight from git (raw GitHub download, a Linux checkout that keeps the blob bytes) can get a CRLF script that fails with $'\r': command not found.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 git ls-files --eol shows flash_otgw.sh as i/lf (normalised blob), and the other *.sh files checked the same way
- [ ] #2 A check (test or evaluate.py) fails when a tracked *.sh blob contains CR, red on today's blob
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 PREMISE REFUTED (otgw-dev, who filed it): the flash_otgw.sh blob was never CRLF. Raw bytes via git cat-file blob: 3fb853b04 CR 0 / LF 499, 166eace70 CR 0 / LF 586, origin/dev the same; git ls-files --eol shows i/lf for flash_otgw.sh and for all 10 tracked *.sh files. The '499 of 499 lines with CR' in the description came from a faulty check of mine (grep -c on a pattern that matched every line, not CR bytes). No change needed; the CR-check AC is dropped with the premise.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
No defect: the task premise (flash_otgw.sh stored with CRLF in git) was wrong. Evidence: git cat-file blob counts 0 CR at 3fb853b04 (LF 499) and 166eace70 (LF 586); git ls-files --eol reports i/lf for all 10 tracked *.sh files, flash_otgw.sh included; the release zip had LF as well (0 CR in the beta.391 zip). The original count was a measurement error by the filer. ACs not ticked: there is nothing to normalise, and a CR gate for a problem that does not exist was not added.
<!-- SECTION:FINAL_SUMMARY:END -->
