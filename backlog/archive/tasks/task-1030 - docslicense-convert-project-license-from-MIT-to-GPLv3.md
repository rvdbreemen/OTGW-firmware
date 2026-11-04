---
id: TASK-1030
title: 'docs(license): convert project license from MIT to GPLv3'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-06 21:03'
updated_date: '2026-07-06 21:15'
labels: []
dependencies: []
ordinal: 239000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User-directed relicensing: root LICENSE + all source files where Robert van den Breemen is sole copyright holder move from MIT to GPLv3. Third-party vendored code (OpenTherm lib (c)Melnyk, OTGWSerial lib (c)Bron, FSexplorer.ino's LGPL2.1 (c)Fleischer portion) and other-projects/ (external upstream reference) explicitly excluded and left untouched -- confirmed with user (AskUserQuestion) before starting. Structural conversion via a new script (scripts/relicense_mit_to_gplv3.py) with a per-file safety check that skips any file naming a copyright holder other than Robert van den Breemen.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root LICENSE replaced with the verbatim official GPLv3 text (fetched from gnu.org, not reproduced from memory)
- [x] #2 scripts/relicense_mit_to_gplv3.py structurally converts MIT headers/footers to GPLv3 across src/OTGW-firmware/** and src/libraries/{SimpleTelnet,Platform}/**, skipping any file with a non-Robert copyright holder
- [x] #3 SimpleTelnet's own vendored LICENSE file (Robert's own) converted to GPLv3 too
- [x] #4 Third-party files (OpenTherm, OTGWSerial, FSexplorer.ino, other-projects/) verified untouched
- [x] #5 README.md and CHANGELOG note the relicensing
- [x] #6 evaluate.py/build sanity-checked after the conversion
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Relicensed the project from MIT to GNU GPLv3. Root LICENSE replaced with the verbatim official GPLv3 text from gnu.org. Built scripts/relicense_mit_to_gplv3.py to structurally convert MIT headers/footers across src/OTGW-firmware/** and src/libraries/{SimpleTelnet,Platform}/**, with a per-file safety check that skips any file naming a copyright holder other than Robert van den Breemen -- caught and correctly excluded 4 third-party-copyright files during the scan (s0PulseCount.ino, safeTimers.h, plus the already-known OpenTherm/OTGWSerial vendored libs and FSexplorer.ino's LGPL-2.1 portion). SimpleTelnet's own vendored LICENSE file (Robert's own submodule) converted too, committed in its own submodule commit (123106f, not yet pushed -- no standing push permission for that submodule repo). README.md and CHANGELOG.md updated to note the relicensing and the excluded exceptions. Build verified clean (esp32-otgw32 target) after the bulk comment-only edits. Committed and pushed to origin/dev (d3c6209d8).
<!-- SECTION:FINAL_SUMMARY:END -->
