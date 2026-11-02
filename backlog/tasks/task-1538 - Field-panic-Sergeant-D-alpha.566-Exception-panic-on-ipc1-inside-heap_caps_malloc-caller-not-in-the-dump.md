---
id: TASK-1538
title: >-
  Field panic (Sergeant D, alpha.566): Exception/panic on ipc1 inside
  heap_caps_malloc, caller not in the dump
status: To Do
assignee: []
created_date: '2026-11-01T07:50:00Z'
updated_date: '2026-11-02T04:38:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: bt goes into log_line_regs[120] (helperStuff.ino:332, platform_esp32.h:578-588), ~6 frames, caller lost. AC2 diagnostics doable now; root cause needs a field dump.
<!-- SECTION:NOTES:END -->
