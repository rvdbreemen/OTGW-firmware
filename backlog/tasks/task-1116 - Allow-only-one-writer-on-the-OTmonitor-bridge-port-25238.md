---
id: TASK-1116
title: Allow only one writer on the OTmonitor bridge port 25238
status: To Do
assignee: []
created_date: '2026-09-03 05:06'
labels: []
dependencies: []
ordinal: 273000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of TASK-1115 on otgw-1.x.x. OTGWstream is declared AsyncSimpleTelnet<2>, so two clients can hold the port at once. available() and read() serve the first slot with data and there is no per-client stream identity, so two simultaneous writers splice into one PIC command stream.\n\nOn this branch the defect is sharper than on 1.x because the reader is not single-threaded: the same audit found that a disconnect between available() and read() could hand the PIC a fabricated 0xFF (fixed separately as TASK-1114). Reducing to one slot removes the interleaving half of the problem.\n\nWith MAX_CLIENTS at 1 the reconnect-rotation branch in AsyncSimpleTelnet becomes live: a new connection from the same address silently evicts the previous session, so a crashed OTmonitor can reclaim the port, while a different address is refused at accept.\n\nNote the template argument also appears in the setTelnetNegotiation call, which must change with the declaration or the firmware will not compile.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGWstream is declared with a single client slot and every reference to its template argument is updated so the firmware compiles
- [ ] #2 The NEG_OFF call still runs before the listener binds, so telnet IAC escaping stays off for the raw bridge
- [ ] #3 A reconnect from the same address takes over the session; a second client from a different address is refused at accept
- [ ] #4 build.bat is green for the default targets and evaluate.py --quick shows no new failures
<!-- AC:END -->
