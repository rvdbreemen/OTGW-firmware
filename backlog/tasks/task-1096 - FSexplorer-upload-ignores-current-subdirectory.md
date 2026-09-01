---
id: TASK-1096
title: FSexplorer upload ignores current subdirectory
status: Done
assignee:
  - '@claude'
created_date: '2026-09-01 17:00'
updated_date: '2026-09-01 17:19'
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
- [x] #1 Uploading from a subdirectory stores the file in that subdirectory
- [x] #2 Uploading from root still stores the file in root
- [x] #3 Filesystem image builds and evaluator stays green
- [x] #4 Deleting a file from a subdirectory works for paths longer than 33 characters
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Uploading a file from a subdirectory in FSexplorer wrote it to the LittleFS root instead. Fixed, plus a second defect that surfaced while cleaning up.

What changed:
- data/FSexplorer.html: the submit handler posted to a hardcoded "/upload", discarding the "?path=" query that loadFileList() had already set on uploadForm.action. It now posts to uploadForm.action. The server side (FSexplorer.ino handleFileUpload) already read httpServer.arg("path"), so no server change was needed for this.
- FSexplorer.ino: the delete handler in apilistfiles() copied the path into char deletePath[34], truncating any path over 33 characters and answering "File not found". A file uploaded into /pic16f1847 under a realistic PIC firmware name could therefore not be deleted. Buffer raised to 64; the old comment claimed a 31-character limit on paths, but that limit is on the filename.

Verification on live hardware (ESP8266 at 192.168.88.16):
- Before the fix: curl POST /upload?path=%2Fpic16f1847 landed in the subdirectory, plain POST /upload landed in root, proving the defect was client-side only.
- After the fix, through the real web UI: upload from /pic16f1847 landed there, upload from root landed in root.
- Firmware was flashed OTA (app only; the filesystem image was deliberately skipped because it carries the repo settings.ini and would overwrite the device configuration). Post-flash, delete of the 41-character path /pic16f1847/otgw-gateway-firmware-6.5.hex succeeded where the old build reported "File not found", and delete from the subdirectory also works through the UI button.
- All test files were removed; the device directories are back to their original contents.

Not a regression risk: the upload handler caps the filename at 30 characters before the path prefix is added, and a 42-character total path writes fine on LittleFS.

Findings worth passing to the reporter:
- .ver files are not required. GetVersion() reads the version out of the .hex and writes the .ver itself, so any .hex in the right directory appears in the PIC flash list.
- The directory must match the detected PIC type. On the test device (PIC16F88) /api/v2/firmware/files reads /pic16f88, not /pic16f1847.

Build: build.bat completed successfully (1.7.5-beta.4+bc6114f). Evaluator: python evaluate.py --quick, 37 checks, 0 failures, health 100%. The auto-incremented version.h and data/version.hash were reverted so this bugfix commit carries no version bump.
<!-- SECTION:FINAL_SUMMARY:END -->
