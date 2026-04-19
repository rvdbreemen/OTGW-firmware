---
id: TASK-302
title: '[SEC-M2] Remove TLS verification disable in build.py arduino-cli download'
status: To Do
assignee: []
created_date: '2026-04-18 19:19'
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
- [ ] #1 Use default ssl.create_default_context without weakening
- [ ] #2 Verify downloaded archive against published SHA256 sidecar before extraction
- [ ] #3 tar.extractall uses filter=data (Python 3.12+) or per-member path validation
<!-- AC:END -->
