---
id: TASK-1115
title: Allow only one writer on the OTmonitor bridge port 25238
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-03 05:05'
updated_date: '2026-09-03 05:12'
labels: []
dependencies: []
ordinal: 206000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGWstream is declared SimpleTelnet<2>, so two clients can hold the port at once. available() and read() serve the first slot that has data, and there is no per-client stream identity anywhere: read() returns a bare int. Two clients writing at the same time therefore splice into one PIC command stream, for example client A's half-line PR=A concatenating with client B's TT=20.5 into a single malformed command. Within one client order is preserved; between clients it is not.\n\nFound by an adversarial audit of byte-exactness on port 25238. The audit deliberately left the choice open because it is a field question rather than a code question: fixing it in the library means adding a per-slot read API and changing every caller, while declaring the instance single-client is one template argument. The maintainer decided on one writer.\n\nWith MAX_CLIENTS at 1 the library's reconnect rotation becomes active: a new connection from the SAME address silently evicts the previous session and takes it over, so a crashed OTmonitor can always reclaim the port, while a connection from a different address is refused at accept. That is the intended single-writer semantic and it is strictly better than silently interleaving two writers into the PIC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGWstream is declared with a single client slot
- [ ] #2 A reconnect from the same address takes over the session rather than being refused, so a client that crashed can reclaim the port
- [ ] #3 A second client from a different address is refused at accept instead of having its bytes interleaved into the PIC stream
- [ ] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->
