---
id: TASK-1538
title: >-
  Field panic (Sergeant D, alpha.566): Exception/panic on ipc1 inside
  heap_caps_malloc, caller not in the dump
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T07:50:00Z'
updated_date: '2026-11-02T12:54:00Z'
labels:
  - crash
  - field
dependencies: []
priority: high
ordinal: 694000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Sergeant D in #alpha-testing 2026-11-01 (msg 1566327932199116884): '2026-10-31 08:41:53 - reboot cause: Exception/panic (4)' and 'Core dump: ipc1 pc=0x4038c7e4 elf=2965efb0 bt=0x4038c7e4 0x40383bff 0x403885d2 0x4038876c 0x40376ddb 0x40376df5'. elf=2965efb0 matches OTGW-firmware-esp32-combo-2.0.0-alpha.566+2691785.elf (sha256 prefix, from ../OTGW-build-archive). addr2line: _xt_context_save (xtensa_context.S:134) <- _frxt_int_enter (portasm.S:133) <- vPortExitCriticalSafe / multi_heap_internal_unlock (multi_heap.c:176) <- multi_heap_malloc (multi_heap_poisoning.c:265) <- heap_caps_aligned_alloc_base (heap_caps_base.c:175) <- heap_caps_malloc_base (heap_caps_base.c:202). The frames are the interrupt/panic entry on core 1 during a heap allocation; the application caller is beyond the 6 recorded frames. Same ipc1 + _frxt_int_enter signature as the bench dump in TASK-1479 note 2 (pc=0x40383bf7). Possible readings (unverified): interrupt watchdog while core 1 held the heap lock, or heap corruption caught by poisoning. Needs a longer backtrace or the full core dump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Full core dump (or a longer backtrace) obtained from the unit or a reproduction, and the application caller named with file:line
- [ ] #2 Cause identified with evidence (old-vs-fix reproduction) or, if not reproducible, the diagnostics gap closed so the next dump shows the caller
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
AC2 (diagnostics gap): REBOOT_LOG_LINE_MAX 240 (OTGW-firmware.h) sizes log_line_regs (updateRebootLog), readLatestCrashLog line, copyOlderRebootLogLines line and restAPI crashDetails, so the Core dump line keeps all 16 PCs of esp_core_dump_summary (max 236 chars). Test first: tests/test_crashlog_coredump.py reads the buffer sizes from the real sources; 16-frame writer, reader and copy cases. No OSS question (sizing only). Full dump download = TASK-1560 (new REST contract, ADR). AC1 (caller named) needs the next field dump: stays open.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: bt goes into log_line_regs[120] (helperStuff.ino:332, platform_esp32.h:578-588), ~6 frames, caller lost. AC2 diagnostics doable now; root cause needs a field dump.

2026-11-02 evidence (wt-1552, base 6b0d0024d beta.9): tests/test_crashlog_coredump.py on HEAD copies (OTGW_PLATFORM_H/HELPER_INO/REST_INO/FW_H = git show HEAD): FAILED 3 = full16 6 frames (113 chars, as in Sergeant D's dump), parse16 reader cut the line, copy16 copy cut it. Fix: 10/10 OK, 16 frames, 223 chars. Side effect: the External Reason line was snprintf'd with LOG_LINE_LENGTH 140 into the 120 buffer (latent overflow), now 240. evaluate --quick 0 failed 100%; full tests/ 93/93. Review: risk-based, no agent review (buffer sizing only; stack: async_tcp readLatestCrashLog + REST +160 B on a 16 KB stack). Full core dump: still only over USB (esptool read_flash 0x3F0000 0x10000 + esp-coredump with the .elf); REST download filed as TASK-1560.
<!-- SECTION:NOTES:END -->
