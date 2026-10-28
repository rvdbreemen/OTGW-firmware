---
id: TASK-1430
title: >-
  Fix: PIC reset_cause published as a raw 0xFF byte, Home Assistant cannot
  decode it (GH #688)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T20:25:00Z'
updated_date: '2026-10-28T21:19:00Z'
labels:
  - bug
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/688
priority: medium
ordinal: 257000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 (GH #688, 2026-10-25, 1.7.8-beta.5+0de3b19, PIC 6.8): otgw-pic/settings/reset_cause carries the single byte 0xFF. HA logs 'Can't decode payload b'\xff' ... with encoding utf-8' (106x in 10 h) and the entity stays unknown. Same root cause as dev TASK-1406 (PR=Q reply stored verbatim in state.picSettings.sResetCause; published at OTGW-Core.ino:861/910 and served at restAPI.ino:1514, so /api/v2 JSON is invalid UTF-8 too). 1.x sibling of TASK-1406.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A non-printable or non-ASCII PR=Q reply is published on MQTT and served on /api/v2 as readable ASCII (e.g. unknown or hex), proven by a host test with a 0xFF answer that is red on HEAD
- [x] #2 Valid printable reset causes are published unchanged
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A non-printable PIC PR= answer is now stored as hex text, so otgw-pic/settings/reset_cause and /api/v2/pic/settings stay valid UTF-8 (GH #688).

Changes:
- New header-only copyPicSettingValue() (picSettingValue.h): printable ASCII copied as is; any other answer becomes "0x" + uppercase hex of every byte, cut at a whole byte, empty if not even one byte fits.
- handlePRresponse() cleans into a local buffer first and compares the cleaned value, so the same 0xFF answer does not republish on every readout.
- sResetCause 4 -> 8 bytes (4 was too small for "0xFF"; +4 bytes RAM).
- HA discovery for reset_cause is a plain sensor (no enum/template) and the web UI falls back to the raw value, so "0xFF" shows as is.

Evidence:
- test/host/test_picSettingValue.cpp, 11 checks. RED: against a byte-for-byte strncpy stand-in (same behaviour as the old strlcpy at the call site) 5 of 11 fail, incl. got "\xFF" want "0xFF". GREEN: fix 11/11. Full test\run_tests.bat: 7 tests, 0 failures.
- python build.py --firmware: exit 0, "Build completed successfully", bin 22:17:37. evaluate.py --quick: 38 passed, 0 failed.

Not verified:
- The call site in handlePRresponse() is not compiled by the host test.
- Not reproduced on hardware: needs a PIC answering PR=Q with 0xFF (seen after a PIC flash); test-flashing the PIC is off-limits without an explicit order. Field confirmation from mrfox7688 on the next beta.
- 2.0.0 sibling: dev TASK-1406 (note added with the same representation).

Commit 2baaf75f9 on otgw-1.x.x, not pushed yet.
<!-- SECTION:FINAL_SUMMARY:END -->
