---
id: TASK-1560
title: >-
  Crash diagnostics: offer the stored ESP32 core dump for download (REST), so a
  field panic can be decoded offline
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T12:52:00Z'
updated_date: '2026-11-05T18:11:00Z'
labels:
  - crash
  - diagnostics
dependencies: []
priority: medium
ordinal: 712000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1538. The reboot log now keeps all 16 backtrace frames of the core-dump summary, but the application caller can still be deeper (the ipc1 panic of alpha.566 entered through _frxt_int_enter inside heap_caps_malloc). The full ELF core dump stays in the coredump partition (0x3F0000, 64 KB, partitions_otgw_esp32.csv) and today can only be read over USB (esptool read_flash 0x3F0000 0x10000, then esp-coredump with the matching .elf). A GET endpoint that streams the partition would let a field tester send it without USB. New REST contract: needs an ADR (admin-protected? size, streaming, ADR-056/146), docs/api update.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR (Proposed) for the endpoint: auth, streaming, response format
- [x] #2 Endpoint streams the coredump partition; bench: download decodes with esp-coredump against the matching .elf
- [x] #3 docs/api and the capture tool mention it
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Robert 2026-11-04 (interactive): admin-protected. GET /api/v2/device/coredump under the admin auth + CSRF rules (ADR-056), streamed in chunks as application/octet-stream from the coredump partition, 404 when no dump. Proposed ADR via adr-kit. Bench: download from .61/.63 and decode with esp-coredump against the matching .elf (if no dump is stored, say so; never force a crash without asking). Docs: docs/api + capture tool.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 (sonnet agent; lead reran in wt-int4): GET /api/v2/device/coredump behind checkHttpAuth() (ADR-056, Robert's choice), streamed by a beginResponse filler from platformCoreDumpRead() (Platform shim, partition by type/subtype), only the image length, Content-Disposition otgw-coredump-<githash>.bin, 404 when no dump. ADR-210 Proposed. tests/test_device_coredump_api.py 12 OK (old platform_esp32.h: errors=1), test_crashlog_coredump 10 OK, adr_governance 15 OK, evaluate 0 failed. Not done yet: capture-tool download step (AC3 part), bench download + esp-coredump decode (AC2). Tag beta.73.

2026-11-04 bench beta.73: GET /api/v2/device/coredump on .61: 200, 29796 B, on .63: 200, 31908 B, Content-Type application/octet-stream, Content-Length = body. The .63 dump matches the archived beta.5+d0efe73 ELF (dump SHA ba693715a) and esp-coredump 1.16.0 decodes it: Panic reason 'Task watchdog got triggered ... loopTask (CPU 1)', registers; the thread listing then fails inside esp-coredump (KeyError current-thread-id, gdb quirk). The .61 dump (SHA 12206dc4f) matches no archived ELF. FINDING: the attachment name uses the RUNNING githash (otgw-coredump-1f6a124.bin) while the dump is from an older build: misleading. Open: filename fix, capture-tool download step (AC3), 401/403 paths with an admin password set (not exercised: no password on the bench).

2026-11-04 follow-up (sonnet agent; lead reran in wt-int4): attachment named after the dump's own app ELF SHA (otgw-coredump-app-<9 hex>.bin, header X-Coredump-App-Sha256) via shim platformCoreDumpAppSha (platform_esp32.h:671); capture tool scripts/otgw_capture.py fetch_coredump() once per capture (200 keeps coredump-<run>.bin + adds it to the upload zip, 404 one line, 401/403 'admin password needed', --skip-coredump). tests/test_device_coredump_api.py 16 OK (old red), test_capture_tool CoreDumpFetchCase 7 OK (old: no fetch_coredump), crashlog_coredump 10, adr_governance 15, launcher 7 OK; evaluate 0 failed. Tag beta.74.

2026-11-04 bench beta.74: .63 attachment otgw-coredump-app-ba693715a.bin, X-Coredump-App-Sha256 ba693715a = sha256sum prefix of the beta.5+d0efe73 .elf (ba693715a4decd32...) that esp-coredump decodes (TWDT loopTask CPU 1); .61 12206dc4f. Real capture (scripts/otgw_capture.py --device .63 --duration 20): 'Core dump: 31908 bytes -> coredump-20261104-161434.bin (app ELF sha256 ba693715a ...)', file in the upload zip. Auth on .61 with a temporary password: no creds 401, admin creds 200 (29796 B), foreign Origin 403; password cleared afterwards (password=0), settings identical (cmp). Posted msg 1567558144534642780.

2026-11-05: ADR-210 Accepted by Robert (acronyms expanded at first use, unknown Enforcement keys forbid_call/require_call/allowed_in removed so the strict lint passes).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
GET /api/v2/device/coredump streams the stored ESP32 core dump (admin route when a password is set; ADR-210 Proposed), named after the crashed build's app ELF SHA; the capture tool fetches it into the upload zip. Evidence: tests/test_device_coredump_api.py 16 OK, CoreDumpFetchCase 7 OK (both red on old code); bench beta.74: dumps downloaded from both units, the .63 dump's SHA header matches the beta.5 .elf and esp-coredump decodes it; 401/200/403 shown with a temporary password, then cleared. Not covered: esp-coredump's thread listing fails inside the tool (gdb KeyError), not in our data.
<!-- SECTION:FINAL_SUMMARY:END -->
