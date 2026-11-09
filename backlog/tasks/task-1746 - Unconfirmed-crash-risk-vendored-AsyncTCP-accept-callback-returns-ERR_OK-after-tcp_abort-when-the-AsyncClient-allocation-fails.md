---
id: TASK-1746
title: >-
  Unconfirmed crash risk: vendored AsyncTCP accept callback returns ERR_OK after
  tcp_abort() when the AsyncClient allocation fails
status: To Do
assignee: []
created_date: '2026-11-09T06:21:00Z'
labels:
  - asynctcp
  - crash-risk
  - needs-maintainer-ok
  - library
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 850000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1675 review (system32-78, 2026-11-09, read-only; not observed on a device). AsyncTCP_detail::tcp_accept() in the ESP32Async AsyncTCP library (.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp:1545-1585): when 'new AsyncClient' fails (log '_accept failed: couldn't allocate client', ~1580) or there is no onConnect callback (~1582), it calls tcp_abort(pcb) (~1583) and then returns ERR_OK (~1584). The 'couldn't complete setup' branch just above (~1572-1578) returns ERR_ABRT correctly. lwIP contract: lwip/tcp.h (ESP32-S3 framework libs, lwIP 2.2, D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/include/lwip/lwip/src/include/lwip/tcp.h:61-69, tcp_accept_fn): 'Only return ERR_ABRT if you have called tcp_abort from within the callback function!', i.e. after tcp_abort the callback must return ERR_ABRT. tcp_in.c handles the accept event as 'TCP_EVENT_ACCEPT(pcb, ERR_OK, err); if (err != ERR_OK) { if (err != ERR_ABRT) tcp_abort(pcb); return ERR_ABRT; }' and otherwise goes on using pcb (cited from the lwIP 1.4 copy in framework-arduinoespressif8266@3.20704.0/tools/sdk/lwip/src/core/tcp_in.c:755-763; the ESP32 lwIP 2.2 ships precompiled, its tcp_in.c is not on this machine, so the 2.2 code path is NOT verified here). With ERR_OK after an abort, lwIP would continue on a freed pcb: a use-after-free that can crash under heap exhaustion (an AsyncClient allocation failing at the accept of a new web/telnet connection). Not linked to the TASK-1675 hang. A fix is a one-line library change (return ERR_ABRT after tcp_abort) in a vendored/pinned library, which needs Robert's OK; check upstream ESP32Async/AsyncTCP for an existing fix first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides: patch the pinned library, take an upstream release that fixes it, or accept the risk
- [ ] #2 Upstream ESP32Async/AsyncTCP checked for this path (issue/commit link or 'not fixed upstream')
- [ ] #3 If patched: a host or device test that forces the AsyncClient allocation to fail at accept shows no crash, red on the old code where reproducible
<!-- AC:END -->
