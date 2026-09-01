---
id: TASK-1097
title: 'feat-2.0.0: port TASK-1096 - FSexplorer upload ignores current subdirectory'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 17:22'
updated_date: '2026-09-01 17:41'
labels: []
dependencies: []
ordinal: 266000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of 1.x TASK-1096. The FSexplorer upload always writes to the LittleFS root because the XHR submit handler posts to a hardcoded /upload and drops the ?path= query that uploadForm.action carries (data/FSexplorer.html:388). The async server side already reads the path parameter (FSexplorer.ino handleFileUpload), so only the client needs the fix. The same worktree also carries the delete-path truncation found on 1.x: deletePath[34] at FSexplorer.ino:432 silently truncates any path over 33 characters and answers File not found.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Upload posts to uploadForm.action so the browsed subdirectory is honoured
- [x] #2 Delete buffer holds a subdirectory path longer than 33 characters
- [x] #3 Firmware builds green and evaluator shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Ported the two 1.x fixes verbatim: FSexplorer.html now posts to uploadForm.action, and deletePath grew from 34 to 64 bytes.
- The async server already reads the path parameter (getParam("path", true) then false), so the client fix is sufficient here too.
- Not verified on ESP32 hardware: the fix was proven on a live ESP8266 under 1.x TASK-1096, and the changed lines are identical in both trees.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports the two FSexplorer subdirectory defects from 1.x TASK-1096 to the 2.0.0 line.

- data/FSexplorer.html: the upload submit handler posted to a hardcoded "/upload", discarding the "?path=" query that loadFileList() had already set on uploadForm.action, so every upload landed in the LittleFS root. It now posts to uploadForm.action. handleFileUpload() already reads the path parameter from the request (body first, then query), so no server change was needed.
- FSexplorer.ino: the delete handler copied the path into char deletePath[34], truncating any path over 33 characters and answering "File not found". Raised to 64 bytes and corrected the comment, which claimed the 31-character limit applies to paths when it applies to the filename.

Verification: both defects were reproduced and fixed on live ESP8266 hardware under TASK-1096, including upload into a subdirectory, upload into root, and delete of a 41-character path after flashing. The changed lines are identical in both trees, so this port carries that evidence. No ESP32 hardware run was made.

Build: build.bat, esp32-classic SUCCESS and esp32-combo SUCCESS, fresh artefacts for both targets. Evaluator: python evaluate.py --quick, 76 checks, 0 failures, 1 pre-existing warning.
<!-- SECTION:FINAL_SUMMARY:END -->
