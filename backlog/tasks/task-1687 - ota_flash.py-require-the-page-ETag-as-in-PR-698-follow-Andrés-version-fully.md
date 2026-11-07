---
id: TASK-1687
title: >-
  ota_flash.py: require the page ETag as in PR #698 (follow André's version
  fully)
status: Done
assignee: []
created_date: '2026-11-07T11:05:00Z'
updated_date: '2026-11-07T11:54:00Z'
labels:
  - tooling
  - webui
dependencies: []
priority: medium
ordinal: 807000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: follow André's PR #698 head (7bcae1b97) completely for scripts/ota_flash.py. On dev the completeness check requires Content-Encoding gzip on / and gunzips the body, but does not require the ETag; André's version also required an ETag for /v2.html.gz and had tests for the FSexplorer fallback and a corrupt page. Port the missing checks and tests from refs/pr/698 (scripts/ota_flash.py, tests/test_ota_flash.py).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ota_flash.py stops with an error when / answers without an ETag, test red on the old script and green on the fix
- [x] #2 Every ota_flash check and test from PR #698 head 7bcae1b97 is on dev or its replacement is named in the notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-07 chose the combination after a comparison of both versions: PR #698's ETag requirement as the content check, dev's robustness kept. Verified facts: (1) with v2.html.gz missing, / falls back to the gzipped FSexplorer recovery page (FSexplorer.ino:164 sendIndex -> sendFSexplorerPage), which is valid gzip, so only the ETag tells it apart; dev passed it as complete. (2) zlib.error is not an OSError (issubclass False): PR #698 dropped that catch, so damaged deflate data ('invalid block type') escapes as a traceback. (3) The real device (.61) sends ETag "86f6716-/v2.html.gz", the format the check requires.

Ported from refs/pr/698 (7bcae1b97): the ETag check '-/v2.html.gz' with its message 'does not serve the gzipped web UI'; _get_raw returns the headers; tests test_fsexplorer_fallback_stops_with_an_error and the corrupt-trailer case (test_corrupt_gzip_trailer_stops_with_an_error, PR name test_corrupt_page_stops_with_an_error); the fake device sends the firmware's ETag. Replaced / kept instead: PR removed the zlib.error catch, the case-insensitive Content-Encoding compare, the unpacked size, the plain-page test and the not-gzip test - all kept on dev on purpose. Added: test_page_without_etag_stops_with_an_error (AC1) and test_damaged_deflate_data_stops_with_an_error. The PR's PAGE = os.urandom fixture is not ported: dev's seeded random.Random(1) page is reproducible.

Evidence: new tests on the old dev script: 2 failed (fsexplorer_fallback, page_without_etag), 18 passed. On PR #698's script: damaged_deflate_data fails (traceback), plus 3 tests that depend on dev wording/--webui. On the fix: 20 passed (5 of 6 runs; one run had a WinError 10060 local-socket timeout in test_no_app_flashes_the_filesystem_only while 54 was building). The old suite on the old script shows the same flake under this load (1 of 4 runs, WinError 10060 in a different test). evaluate.py --quick: 0 FAIL. scripts/ and tests/ only: no prerelease bump, no firmware build, no device flashed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC2 done. ota_flash.py now requires the page ETag (-/v2.html.gz) as in PR #698, so a filesystem image without v2.html.gz (FSexplorer fallback) is no longer reported complete, and keeps dev's zlib.error catch, case-insensitive encoding and unpacked size (Robert's choice of the combination). New tests red on the old script (2) and green on the fix; every PR #698 check/test is on dev or its replacement is named in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
