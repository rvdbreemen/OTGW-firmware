---
id: TASK-844
title: >-
  Patch ESP8266 Core 2.7.4 HTTP null-alloc bug at build time (.patch + build.py
  apply-step)
status: To Do
assignee: []
created_date: '2026-06-08 19:43'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Confirmed root cause of George's crash: ESP8266 Core 2.7.4 BufferedStreamDataSource::get_buffer (DataSource.h) does an unchecked non-throwing new uint8_t[~1460] per TCP segment when streamFile() serves a static asset; under heap fragmentation it returns NULL and readBytes/memcpy writes to NULL -> StoreProhibited (ROM memcpy 0x4000df64). ClientContext::_write_some passes the get_buffer result to tcp_write without a null check. The firmware-side beta.3 gate mitigates it; this task fixes the ROOT in the core via a committed .patch applied by build.py after core install (core is a board-manager install, gitignored, reinstalled fresh on CI, so a raw edit would not ship). Headers-only fix (no toolchain/SDK rebuild). Requires an ADR (build/dependency change) + sign-off.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A unified-diff .patch under patches/ adds: DataSource.h get_buffer -> new(std::nothrow)+return nullptr on OOM; ClientContext.h _write_some -> break on null get_buffer()
- [ ] #2 build.py applies the patch to the installed core after core install: idempotent (skip if already applied), errors loudly if context does not match (wrong core version), logs clearly
- [ ] #3 ADR created (Proposed) documenting the core-patch decision, alternatives (fork, firmware gate, upstream), and consequences; not self-accepted
- [ ] #4 python build.py applies the patch and compiles clean (exit 0); evaluate.py --quick no new failures
<!-- AC:END -->
