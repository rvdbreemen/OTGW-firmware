---
id: TASK-1030
title: 'docs(license): convert project license from MIT to GPLv3'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-06 21:03'
updated_date: '2026-07-06 21:04'
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
- [ ] #1 Root LICENSE replaced with the verbatim official GPLv3 text (fetched from gnu.org, not reproduced from memory)
- [ ] #2 scripts/relicense_mit_to_gplv3.py structurally converts MIT headers/footers to GPLv3 across src/OTGW-firmware/** and src/libraries/{SimpleTelnet,Platform}/**, skipping any file with a non-Robert copyright holder
- [ ] #3 SimpleTelnet's own vendored LICENSE file (Robert's own) converted to GPLv3 too
- [ ] #4 Third-party files (OpenTherm, OTGWSerial, FSexplorer.ino, other-projects/) verified untouched
- [ ] #5 README.md and CHANGELOG note the relicensing
- [ ] #6 evaluate.py/build sanity-checked after the conversion
<!-- AC:END -->
