---
id: TASK-1167
title: >-
  Port 25238: show the connected client on the status page, and reconsider one
  writer plus readers
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-26 15:04'
updated_date: '2026-09-26 16:47'
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
- [x] #2 A decision is recorded on multi-client support (for example one writer plus N read-only clients), with the command-splicing risk addressed
- [ ] #3 iandury_ and Schelte Bron are answered in #nederlandse-ondersteuning
- [ ] #4 Bench validation per ADR-097 Confirmation: byte transparency (all byte values 0-255, serial side captured), two concurrent writers without interleaving, idle release after 100 ms, a binary stream with CR bytes keeps the floor while the other client sends commands, HA opentherm_gw plus a second tool connected together, and heap with two streaming clients over 30 minutes
- [ ] #5 otgwstream_* fields verified on the Device Info page and in /api/v2/device/info on hardware, including a refused third connection
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

2026-09-26: ADR-097 accepted (write floor, byte transparent both ways; option C line buffering rejected by the maintainer). Implemented:
- SimpleTelnet e8d01df on branch feat/per-slot-read (local, not pushed): availableFrom/readFrom/isSlotActive; same-address takeover for any slot count (only when all slots are full, so two tools on one host each keep a slot; debugTelnet is <1> so unaffected).
- Firmware 78b1e4553: OTGWstream -> SimpleTelnet<OTGW_NET_SLOTS=2> (OTGW-Core.h); write floor in handleOTGW() (OTGW-Core.ino, OTGW_NET_WRITE_FLOOR): holder read via readFrom, bytes to OTGWSerial unchanged, other slot unread; release on silence (OTGW_NET_FLOOR_IDLE_MS 100 / OTGW_NET_FLOOR_BINARY_IDLE_MS 3000 after any non-text byte) or disconnect; round-robin between busy clients; sWrite observer reset on release. device/info lists all occupied slot addresses.
- Build green 1.7.6-beta.7+1b0a1b7, evaluate 36/36, static RAM +60 B (DATA 3120->3124, BSS 39168->39224).
- NOT verified on hardware: bench offline (TASK-1165). AC #3 (answer iandury_ and Schelte) waits for the maintainer's go and ideally for a validated build.
- Nothing pushed: otgw-1.x.x also carries the unvalidated TASK-1164 fix.

## Beta announcement text (maintainer-approved 2026-09-26; include verbatim in the next /beta-prerelease announcement, marked as a breaking change; do not post before the bench validation of AC #4/#5 passed)

Breaking change: port 25238 (the legacy OTmonitor port) accepts two clients again.

v1.7.5 limited the port to one client, because two tools writing at the same time could mix their bytes into one broken command for the PIC. That left anyone running Home Assistant or Domoticz next to OTmonitor stuck on v1.7.4.

What changes in this beta:
- Two clients can connect at the same time, for example the Home Assistant OpenTherm Gateway integration and OTmonitor. Both receive everything the PIC sends.
- Only one client at a time can send to the PIC. It keeps that right until it has been quiet for 100 ms. The other client's command waits in the meantime and then goes through whole. Commands can no longer get mixed up.
- Nothing is changed on the way through. Every byte a client sends reaches the PIC exactly as sent, in both directions. The port is still fully transparent.
- A PIC firmware upgrade from OTmonitor over this port is protected. Once a client sends binary data, it keeps the right to send until it has been quiet for 3 seconds, so the other client cannot interrupt an upgrade. Upgrading the PIC over the network remains discouraged; the web interface's PIC flash page is the safer route.
- A third connection is refused. If it comes from the same address as a client that is already connected, it replaces that client instead, so a tool that crashed and reconnects gets its place back.
- The Device Info page and /api/v2/device/info now show how many clients are connected, their addresses, and the last address that was refused. If OTmonitor cannot connect, that tells you who holds the port.

If you run two tools against port 25238, we would like to hear how it behaves.

2026-09-26: the unvalidated work (TASK-1164 fix, TASK-1167 write floor + status fields, ADR-066/083/097 changes) is pushed to the separate branch origin/otgw-1.x.x-pending-bench-validation (f87d93bde), NOT to origin/otgw-1.x.x. SimpleTelnet per-slot API pushed as origin/feat/per-slot-read (e8d01df) in rvdbreemen/SimpleTelnet. Local otgw-1.x.x still carries the same commits ahead of origin; merge the pending branch into otgw-1.x.x only after the bench validation passes.
<!-- SECTION:NOTES:END -->
