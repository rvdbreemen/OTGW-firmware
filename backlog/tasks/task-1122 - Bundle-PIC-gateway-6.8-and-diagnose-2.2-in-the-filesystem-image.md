---
id: TASK-1122
title: Bundle PIC gateway 6.8 and diagnose 2.2 in the filesystem image
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 21:22'
updated_date: '2026-09-03 21:55'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 212000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The shipped PIC images under src/OTGW-firmware/data/pic16f1847/ lagged Schelte Bron's site: gateway 6.6 and diagnose 2.1. Both were released upstream and the diagnose update is what Schelte reported he could not install from the web. Bundling the current images means a fresh flash already carries them and the update-from-web path only has to close a small gap.\n\nOne trap sits in this data. FSexplorer.ino:374 prefers the version parsed out of the hex by GetVersion(), and falls back to the .ver file only when that parse finds nothing. The diagnose image carries no 'OpenTherm Gateway ' banner, so GetVersion() always returns empty for it and the .ver file is its only version source. A .ver that does not match its .hex is therefore not cosmetic for diagnose: the device would report a version it does not run and would never fetch the real one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 gateway.hex is byte-identical to the upstream 6.8 image and gateway.ver reads 6.8
- [x] #2 diagnose.hex is byte-identical to the upstream 2.2 image and diagnose.ver reads 2.2
- [x] #3 Every bundled hex file is well-formed Intel HEX: every record checksum verifies and an EOF record is present
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The filesystem image now ships PIC gateway 6.8 and diagnose 2.2, replacing gateway 6.6 and diagnose 2.1.

Both hex files were fetched from otgw.tclcode.com and are byte-identical to the upstream images (sha256 comparison), with matching .ver files. Validated as Intel HEX before installing: gateway 623 records, diagnose 287 records, every record checksum verifying, both carrying an EOF record. The gateway image was additionally confirmed by parsing its embedded banner, which reads 6.8.

Why the diagnose pairing matters: FSexplorer.ino:374 prefers the version GetVersion() parses out of the hex and falls back to the .ver file only when that parse finds nothing. The diagnose image carries no "OpenTherm Gateway " banner, so GetVersion() always returns empty for it and the .ver file is its only version source. The tree was found half-updated, with diagnose.ver already bumped to 2.2 against a 2.1 hex, which would have shipped a device reporting a version it does not run and never fetching the real one.
<!-- SECTION:FINAL_SUMMARY:END -->
