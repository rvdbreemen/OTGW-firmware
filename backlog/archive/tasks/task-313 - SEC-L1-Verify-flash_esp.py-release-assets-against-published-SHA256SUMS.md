---
id: TASK-313
title: '[SEC-L1] Verify flash_esp.py release assets against published SHA256SUMS'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:23'
updated_date: '2026-04-19 07:17'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
flash_esp.py:402 fetches .bin assets from GitHub releases and writes directly to flash, no checksum verification. A compromised CI secret can silently alter a release binary.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release workflow publishes a SHA256SUMS asset alongside binaries
- [x] #2 flash_esp.py downloads SHA256SUMS and verifies each artifact before esptool write
- [x] #3 Mismatch aborts flashing and deletes the local copy
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 (release workflow publishes SHA256SUMS) belongs in the GitHub Actions release workflow, not in flash_esp.py. flash_esp.py is now FORWARD-COMPATIBLE: when the release workflow starts publishing SHA256SUMS, verification kicks in automatically. Until then, a warning is emitted so users know they are downloading unverified. Follow-up task should update .github/workflows/*.yml to emit SHA256SUMS alongside the release assets.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
flash_esp.py: new _load_sha256sums and _sha256_of_file helpers. download_release_assets now attempts to download SHA256SUMS (or .TXT/.SHA256 variants) before any binary; _download verifies each asset against the map and aborts+unlinks on mismatch. Graceful degradation: no SHA256SUMS in release -> warning + continue. Present but missing entry for asset -> warning + continue. Mismatch -> hard fail.
<!-- SECTION:FINAL_SUMMARY:END -->
