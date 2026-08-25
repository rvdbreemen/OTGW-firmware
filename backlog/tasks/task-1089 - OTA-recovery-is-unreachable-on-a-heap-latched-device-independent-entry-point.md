---
id: TASK-1089
title: >-
  OTA recovery is unreachable on a heap-latched device (independent entry
  point?)
status: To Do
assignee: []
created_date: '2026-08-25 18:53'
labels:
  - bug
  - adr-required
dependencies: []
priority: medium
ordinal: 188000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while resolving TASK-1039. The flash-upload handlers are reached through the same gated call as ordinary serving: OTGW-firmware.ino runs handleEsp/PicFlashBackgroundTasks in an if/else-if/else on bESPactive/bPICactive, and those flags are set by the upload handler, which is itself only reachable through canServeHttp(). The comment at helperStuff.ino claims the flash-upload handlers are NOT gated; that overstates the guarantee.

Consequence: a device whose HTTP gate has engaged cannot be recovered over the air. TASK-1039's reaper stops the gate latching shut, but it does not change this: it releases the pending connection rather than serving it, so an upload POST arriving during a gated window is closed rather than accepted.

This is deliberately NOT folded into ADR-091. That decision records one rule, that a refusal must not suppress its own cleanup path. Privileging OTA under heap pressure is a different promise with different trade-offs (you want that path favoured, not bounded), and merging them would blur both.

Needs its own ADR before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Whether OTA needs an entry point independent of the HTTP heap gate is decided and recorded in its own ADR
- [ ] #2 If yes: an upload POST is accepted while canServeHttp() is refusing, without reintroducing the unchecked 2100-byte HTTPUpload allocation below the gate threshold
- [ ] #3 Verified on the bench: a device held below the gate threshold can still be flashed over the air
<!-- AC:END -->
