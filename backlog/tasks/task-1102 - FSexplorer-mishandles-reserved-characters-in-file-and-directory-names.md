---
id: TASK-1102
title: FSexplorer mishandles reserved characters in file and directory names
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 18:34'
updated_date: '2026-09-01 19:16'
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
- [x] #1 A file whose name contains a hash, question mark or percent opens and downloads from any directory
- [x] #2 The stored filename matches what the browser sent, without a decode pass
- [x] #3 One name containing a quote or backslash cannot break the listing for the other files
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Three encoding defects fixed. Open and download link hrefs are now per-segment encoded (the delete link on the same row already was), the multipart filename is stored without a urlDecode pass, and api/listfiles escapes each name with escapeJsonStringTo into a widened buffer. Verified on live hardware: a file named note#1.txt uploaded into /pic16f1847 lists correctly and GET .../note%231.txt returns 200 (before the fix the browser ate #1.txt as a fragment); a file named a+b.txt is stored verbatim and served, where the old urlDecode would have made it 'a b.txt'.
<!-- SECTION:FINAL_SUMMARY:END -->
