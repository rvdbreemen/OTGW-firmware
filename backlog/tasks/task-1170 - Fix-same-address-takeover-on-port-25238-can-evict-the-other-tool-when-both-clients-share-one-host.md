---
id: TASK-1170
title: >-
  Fix: same-address takeover on port 25238 can evict the other tool when both
  clients share one host
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-29 03:56'
updated_date: '2026-09-29 04:20'
labels:
  - bug
  - port-25238
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning iandury_ 2026-09-28'
priority: low
ordinal: 239000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
iandury_ (Discord #nederlandse-ondersteuning, 2026-09-28) runs Domoticz and OTmonitor on the same PC against port 25238 on v1.7.6-beta.7; he confirms it works and asks whether sharing one IP can cause problems.

From code (not yet seen on the bench): when both slots are occupied and a new connection arrives, SimpleTelnet::_acceptNewClients() (src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:427-433) evicts the FIRST active slot whose IP matches. With two tools on one host both slots carry the same IP, so if one tool reconnects before its old socket is detected dead, the takeover can evict the OTHER tool instead of the stale one, and the evicted tool reconnecting can evict again. Normal operation without reconnects is unaffected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced on the bench: two clients from one host, one of them reconnects while its old socket is still considered alive; recorded which slot is evicted
- [x] #2 If reproduced: the takeover evicts the stale connection (for example the slot with no traffic for longest, or a socket that fails a liveness probe), measured old vs new with the same reproduction
- [ ] #3 iandury_ answered in #nederlandse-ondersteuning with the finding
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ID notice: the backlog CLI reused ID 1170, which also belongs to an archived won't-do task (backlog/archive/tasks/task-1170 - Non-blocking-port-25238-writers-with-a-per-client-buffer.md). This active task is the same-address takeover fix; references to "TASK-1170" before 2026-09-29 mean the archived one.

2026-09-29 AC#1 reproduced on bench .88.68 (v1.7.6 code, 1.7.6-beta.7+6266b84), harness scratchpad t1170.py: A connects (slot 0), B connects (slot 1), then B opens a new connection while its old socket stays open (a crash without FIN, as seen by the gateway). Result 5 of 5 rounds: A (the other tool) was evicted, B's old socket stayed alive, B's new socket got A's slot. Cause as read from code: _acceptNewClients() takes the FIRST active slot whose IP matches (SimpleTelnet_impl.tpp:427-433).

2026-09-29 fix and old-vs-new (maintainer chose option A, library change):
- Realistic harness t1170b.py: B's old connection hangs (SO_RCVBUF 1024, never reads), A and B auto-reconnect within 0.5 s on eviction, both send PR=A each second. OLD (1.7.6-beta.7): A and B evicted each other 49 times each, 52 s of churn, stale socket kept in slot 1. NEW: 18/17 evictions, 17.5 s of churn.
- Two live clients (t1170.py): unchanged by design, 5/5 first match (tie, nothing to tell them apart).
- Rule: evict the same-address slot with the least availableForWrite() (most unacked outbound data); tie keeps the first match. SimpleTelnet 3e64801 on feat/per-slot-read (pushed), firmware commit 1ea57b483.
- Why not immediate: a hung app whose OS still ACKs is invisible at TCP level until its receive buffer fills; a vanished host (no ACKs) shows queued data at the next PIC output. The vanished-host case could not be reproduced from this laptop without admin rights (no way to drop ACKs), so it is inferred, not measured.
- Regression: refusal from a second address, single-match takeover, 60 s two-writer run (1354 answers, 0 errors) unchanged. build.bat and evaluate --quick green.
- 2.0.0: not affected (AsyncSimpleTelnet<1>, one slot). ADR-097 does not specify which same-address slot is replaced.
<!-- SECTION:NOTES:END -->
