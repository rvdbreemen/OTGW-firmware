---
id: TASK-3025
title: >-
  ADR-107 heap recovery: drop /ws clients by aborting their TCP instead of a WS
  close (likely AC3 entry into the stuck DISCONNECTING state)
status: To Do
assignee: []
created_date: '2026-11-22T01:58:00Z'
updated_date: '2026-11-22T06:04:00Z'
labels:
  - heap
  - websocket
  - adr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1139000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parent: TASK-3024. ADR-107's action 1 (emergencyHeapRecovery, helperStuff.ino:1313-1317) calls doWebSocketDisconnectAll() = otLogWs.closeAll(): a WS-level close that sets WS_DISCONNECTING and queues a close frame. At CRITICAL heap that frame may not leave, the TCP stays open, the client receives nothing (TASK-3017 AC3, 174 min; consistent with the data, not proven: heap 3132 B at 22:00:32, a recovery line lost). It also frees no memory until the close completes, which is the point of the action. Proposal: abort the /ws TCP (RST, the queue freed at once). Design constraints: emergencyHeapRecovery runs on the loop task and the library's client list/lock are private, so the abort needs a safe cross-task path (e.g. a flag the async_tcp side acts on, or the locked client(id) plus AsyncClient::abort(), with the deletion race analysed). ADR check: ADR-107's Decision names 'webSocket.disconnect() (close-all variant)', so switching to an abort changes the decided mechanism: a superseding ADR via adr-kit (Proposed, to Robert), not an edit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design note: which task does the abort, why it is safe against the async_tcp deletion of the client, and the superseding ADR (Proposed) that replaces ADR-107 action 1
- [ ] #2 Host test: at CRITICAL the recovery aborts every /ws connection (RST) instead of a WS close; red on the code before
- [ ] #3 Bench: a forced CRITICAL (or a reproduction) shows the /ws client gone at once with its memory back, and a web UI tab reconnecting
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22 reproduction (otgw-bench-tester, 2 h AC3 capture on 045e1fd, .61; relayed by the coordinator): at 05:17:42 a '[heap-recovery]' (ADR-107, closeAll()) was followed by 73 s of /ws silence until the 70 s rx timeout of TASK-3017 freed the client; the same pattern at 04:08:04. So ADR-107's WS-level close leaves the client stuck outside WS_CONNECTED until the rx timeout; an abort would end it at once.
<!-- SECTION:NOTES:END -->
