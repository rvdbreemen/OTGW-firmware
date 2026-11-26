---
id: TASK-3110
title: >-
  Device info reads the internal heap through raw heap_caps_ calls in
  restAPI.ino instead of the platform shims
status: To Do
assignee: []
created_date: '2026-11-26T20:33:00Z'
labels:
  - platform
  - rest-api
  - tech-debt
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1168000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The device-info snapshot and its JSON emit read the internal heap with raw IDF calls in restAPI.ino: heap_caps_get_free_size(MALLOC_CAP_INTERNAL) and heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) at restAPI.ino:3242-3243 and :4133-4134 (origin/dev d5f4c5ac0). The project rule is to read the platform through the platformXxx() shims (CLAUDE.md firmware rules). platformFreeHeap() and platformMaxFreeBlock() already return exactly these two values (Esp.cpp:163-173). Found while writing ADR-300. Its Must (never through a raw heap_caps_* call in restAPI.ino) is read as covering the admission rule's own reads only (coordinator and architect, 2026-11-26), so this cleanup is not part of ADR-300 and not a 2.0.0 blocker. The prerelease cbufprobe reads PSRAM too; there is no PSRAM shim yet, so it is out of scope unless a shim is added.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 restAPI.ino:3242-3243 and :4133-4134 (or their lines at the time) read through platformFreeHeap() and platformMaxFreeBlock(); the JSON field names and values of device info are unchanged.
- [ ] #2 A host test or an evaluate.py check fails when a heap_caps_get_free_size(MALLOC_CAP_INTERNAL) or heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) call appears in restAPI.ino outside the prerelease cbufprobe, red on the code before the change.
- [ ] #3 A device-info response from a bench unit before and after the change shows the same internal_free / internal_maxblk semantics (values differ only by normal heap movement).
<!-- AC:END -->
