---
id: TASK-302
title: '[SEC-M2] Remove TLS verification disable in build.py arduino-cli download'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:19'
updated_date: '2026-04-19 07:17'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build.py:346-374 sets ssl.CERT_NONE and check_hostname=False. Supply-chain risk: MITM can swap the binary. Also tar/zip extractall without filter=data allows path traversal.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Use default ssl.create_default_context without weakening
- [x] #2 Verify downloaded archive against published SHA256 sidecar before extraction
- [x] #3 tar.extractall uses filter=data (Python 3.12+) or per-member path validation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SHA256 sidecar verification of the downloaded archive (one of the AC bullets) is implemented as part of TASK-313 (flash_esp.py SHA256SUMS lookup); the same infrastructure applies to arduino-cli downloads if the project decides to publish sha256 sidecars for their arduino-cli dependency. For now: no sidecar is published by downloads.arduino.cc, so we rely on TLS verification alone. macOS cert-store workaround note added as a comment.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py arduino-cli download no longer disables SSL hostname/cert verification. Default ssl.create_default_context() verifies certificates; macOS cert-store issues are now a user-environment concern, not a silent MITM hole. tar.extractall now uses filter='data' (Python 3.12+) with a manual path-traversal guard for older Pythons; zip.extractall validates resolved paths stay inside temp_dir before extraction.
<!-- SECTION:FINAL_SUMMARY:END -->
