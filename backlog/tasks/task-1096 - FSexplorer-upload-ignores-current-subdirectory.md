---
id: TASK-1096
title: FSexplorer upload ignores current subdirectory
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 17:00'
updated_date: '2026-09-01 17:09'
labels: []
dependencies: []
ordinal: 194000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Uploading a file from a subdirectory in FSexplorer always writes it to the LittleFS root. The XHR submit handler posts to a hardcoded /upload URL and drops the ?path= query the form action carries, so the server-side handler never sees the target directory. Reported on 1.7.4+b77304b while uploading PIC hex files into /pic16f1847.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Uploading from a subdirectory stores the file in that subdirectory
- [ ] #2 Uploading from root still stores the file in root
- [ ] #3 Filesystem image builds and evaluator stays green
- [ ] #4 Deleting a file from a subdirectory works for paths longer than 33 characters
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Reproduce on device .88.16 with curl (server honours ?path, client does not send it)
2. Fix FSexplorer.html submit handler to post to uploadForm.action
3. Verify on hardware through the real UI: subdir upload + root upload
4. Fix deletePath buffer truncation found while cleaning up test files
5. Build firmware + filesystem, run evaluator
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Root cause: FSexplorer.html submit handler called xhr.open(POST, /upload) literally, discarding the ?path= query that loadFileList() had already put on uploadForm.action. Server side (FSexplorer.ino:545-550) already reads httpServer.arg("path").
- Verified on live ESP8266 192.168.88.16 (1.7.4): curl POST /upload?path=%2Fpic16f1847 lands in the subdirectory, plain POST /upload lands in root.
- Second defect found while cleaning up: deletePath[34] in apilistfiles() truncated any path over 33 chars, so /pic16f1847/<longname>.hex could not be deleted ("File not found"). Buffer raised to 64.
- Path length is not a regression risk: filename is capped at 30 chars before the prefix is added, and a 42-char total path writes fine on LittleFS.
- .ver companion files are not required for a hex to appear in the PIC flash list; GetVersion() reads the version from the .hex and writes the .ver.
<!-- SECTION:NOTES:END -->
