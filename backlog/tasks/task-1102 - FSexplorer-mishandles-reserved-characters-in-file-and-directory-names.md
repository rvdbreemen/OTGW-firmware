---
id: TASK-1102
title: FSexplorer mishandles reserved characters in file and directory names
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 18:34'
updated_date: '2026-09-01 18:41'
labels:
  - audit
  - fsexplorer
dependencies: []
ordinal: 199000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three encoding defects around names. The open and download links assign the raw path to href without encoding, while the delete link on the same row uses encodeURIComponent, so a name holding a hash, question mark or percent opens the wrong URL or nothing at all; a directory carrying such a character breaks every link inside it. The upload handler runs urlDecode on the multipart filename, which browsers never percent-encode, so a literal percent or plus in a name is mangled on the way in. api/listfiles builds its JSON with a raw format specifier, so a stored name containing a quote or backslash breaks the whole listing rather than one row. Found by the FSexplorer audit of 2026-09-01, findings 5, 8 and 9 of 18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A file whose name contains a hash, question mark or percent opens and downloads from any directory
- [ ] #2 The stored filename matches what the browser sent, without a decode pass
- [ ] #3 One name containing a quote or backslash cannot break the listing for the other files
<!-- AC:END -->
