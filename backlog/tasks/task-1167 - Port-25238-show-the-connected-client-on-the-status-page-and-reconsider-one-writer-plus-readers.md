---
id: TASK-1167
title: >-
  Port 25238: show the connected client on the status page, and reconsider one
  writer plus readers
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-26 15:04'
updated_date: '2026-09-26 15:21'
labels:
  - feature
  - port-25238
  - needs-decision
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning 2026-09-25/26'
priority: medium
ordinal: 236000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two requests after the v1.7.5 change to a single client on port 25238 (TASK-1115):
- Schelte Bron (.otgw, Discord #nederlandse-ondersteuning, 2026-09-25): a user cannot connect OTmonitor although the legacy port is enabled; he asks to show on the status page whether a 25238 connection exists and from which address.
- iandury_ (same channel, 2026-09-26): after upgrading to v1.7.5 the Domoticz integration via 25238 stopped working because OTmonitor runs in parallel; reverted to v1.7.4. The maintainer replied that a two-connection mode in 1.7.6 beta is possible and asked why both are needed.

The single-client rule exists because two writers spliced their bytes into one command stream toward the PIC. The release note offered "one writer plus several readers" as the way back, depending on demand; this is that demand.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The web status page (and /api/v2/device/info) shows whether port 25238 has a client and its IP address
- [ ] #2 A decision is recorded on multi-client support (for example one writer plus N read-only clients), with the command-splicing risk addressed
- [ ] #3 iandury_ and Schelte Bron are answered in #nederlandse-ondersteuning
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Part 1 (done, no ADR): device/info fields otgwstream_clients / _client_ip / _last_refused_ip + UI labels. Committed locally.
2. ADR-097 Proposed (committed). WAIT for maintainer acceptance before any multi-client code.
3. After acceptance: SimpleTelnet per-slot read API (availableFrom/readFrom/slot-active) on a library branch; OTGWstream -> SimpleTelnet<2>; per-slot line buffers, forward on CR as one write, discard overflow whole, side effects per line; OTGW_NET_LINE_ATOMIC symbol.
4. Bench test per ADR-097 Confirmation (two concurrent writers, CR-less and overflow lines, HA + second tool, heap 30 min). Needs the bench back online (see TASK-1165).
5. Answer iandury_ and Schelte in #nederlandse-ondersteuning (with maintainer go).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-26: part 1 implemented and built (1.7.6-beta.7+a10e939, build green, evaluate 36/36); not verified on hardware because the bench 192.168.88.68 is offline (TASK-1165). Maintainer chose option C (two writers, whole-line forwarding); ADR-097 written as Proposed and awaiting review. Commits are local only: otgw-1.x.x also carries the unvalidated TASK-1164 fix, so nothing is pushed until that is validated.
<!-- SECTION:NOTES:END -->
