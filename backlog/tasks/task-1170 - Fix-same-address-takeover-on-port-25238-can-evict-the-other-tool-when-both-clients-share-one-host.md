---
id: TASK-1170
title: >-
  Fix: same-address takeover on port 25238 can evict the other tool when both
  clients share one host
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-29 03:56'
updated_date: '2026-09-29 04:04'
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
- [ ] #1 Reproduced on the bench: two clients from one host, one of them reconnects while its old socket is still considered alive; recorded which slot is evicted
- [ ] #2 If reproduced: the takeover evicts the stale connection (for example the slot with no traffic for longest, or a socket that fails a liveness probe), measured old vs new with the same reproduction
- [ ] #3 iandury_ answered in #nederlandse-ondersteuning with the finding
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ID notice: the backlog CLI reused ID 1170, which also belongs to an archived won't-do task (backlog/archive/tasks/task-1170 - Non-blocking-port-25238-writers-with-a-per-client-buffer.md). This active task is the same-address takeover fix; references to "TASK-1170" before 2026-09-29 mean the archived one.
<!-- SECTION:NOTES:END -->
