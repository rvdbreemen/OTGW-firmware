---
id: TASK-1101
title: FSexplorer upload reports success it cannot back up
status: Done
assignee:
  - '@claude'
created_date: '2026-09-01 18:33'
updated_date: '2026-09-01 19:17'
labels:
  - audit
  - fsexplorer
dependencies: []
ordinal: 198000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Four related defects in the upload path, all reporting plain success. The handler never checks that LittleFS.open() returned a usable File or that the writes landed, so an upload onto a full filesystem, or onto a name that collides with a directory, still answers 303 and the listing looks normal. A filename longer than 30 characters is silently renamed to its last 30, which now matters more because uploads land in the directory being browsed and PIC firmware names run long. On 1.x the write gate omits uploadAuthorized, so an unauthenticated upload can write through a handle left open by an aborted authorized one. Found by the FSexplorer audit of 2026-09-01, findings 6, 12, 15 and 16 of 18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 An upload that could not be opened or fully written is reported as failed, not as success
- [x] #2 A filename that has to be shortened is either refused or reported as renamed
- [x] #3 The write gate requires the upload to be authorized
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
handleFileUpload() now tracks an uploadOk flag (reset before the auth return) and sends a 507 instead of the 303 when the open or a write fails, tripping the client's existing failure branch. The write gate requires uploadAuthorized, so an aborted authorized upload cannot leave a handle open for an unauthenticated write. The UI refuses a filename over 30 characters up front instead of the server silently keeping the last 30. Build and evaluator green. AC3 (write gate authorized) is hardware-confirmed via the CSRF/auth test; AC1 (failure reported) needs a full filesystem to exercise and is code-verified.
<!-- SECTION:FINAL_SUMMARY:END -->
