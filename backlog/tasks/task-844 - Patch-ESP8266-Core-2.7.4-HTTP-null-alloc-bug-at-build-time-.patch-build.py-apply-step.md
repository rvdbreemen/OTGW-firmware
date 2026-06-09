---
id: TASK-844
title: >-
  Patch ESP8266 Core 2.7.4 HTTP null-alloc bug at build time (.patch + build.py
  apply-step)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-08 19:43'
updated_date: '2026-06-09 22:42'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Confirmed root cause of George's crash: ESP8266 Core 2.7.4 BufferedStreamDataSource::get_buffer (DataSource.h) does an unchecked non-throwing new uint8_t[~1460] per TCP segment when streamFile() serves a static asset; under heap fragmentation it returns NULL and readBytes/memcpy writes to NULL -> StoreProhibited (ROM memcpy 0x4000df64). ClientContext::_write_some passes the get_buffer result to tcp_write without a null check. The firmware-side beta.3 gate mitigates it; this task fixes the ROOT in the core via a committed .patch applied by build.py after core install (core is a board-manager install, gitignored, reinstalled fresh on CI, so a raw edit would not ship). Headers-only fix (no toolchain/SDK rebuild). Requires an ADR (build/dependency change) + sign-off.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A unified-diff .patch under patches/ adds: DataSource.h get_buffer -> new(std::nothrow)+return nullptr on OOM; ClientContext.h _write_some -> break on null get_buffer()
- [x] #2 build.py applies the patch to the installed core after core install: idempotent (skip if already applied), errors loudly if context does not match (wrong core version), logs clearly
- [x] #3 ADR created (Proposed) documenting the core-patch decision, alternatives (fork, firmware gate, upstream), and consequences; not self-accepted
- [x] #4 python build.py applies the patch and compiles clean (exit 0); evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Patch + build.py apply-step shipped (commits d581780d, 51f79e45). Verified: pristine core -> build.py auto-applies -> compiles clean. Idempotent + loud-fail-on-drift. *.patch pinned LF via .gitattributes. ADR-084 Proposed - awaiting user sign-off before Accepted (do not self-accept).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
REVERTED (2026-06-10, commit 5567d944): the core patch was field-falsified. beta.4 shipped with the patch confirmed in the binary and still crashed with the identical epc1=0x4000df64/excvaddr=0, proving the streamFile/get_buffer site was NOT the faulting allocation. Per maintainer preference (no core patching), removed apply_core_patches() + call from build.py, deleted the patch file, and marked ADR-084 Rejected. build.py builds stock core, exit 0; evaluate.py --quick clean. Firmware-side heap gates (TASK-837/841/843) retained as defense-in-depth. decode-crash.sh + the beta.4 .elf retained for the pending USB stack trace, which will pin the real caller for a firmware-side fix.
<!-- SECTION:FINAL_SUMMARY:END -->
