---
id: TASK-865.18
title: 'fix(combo): gate Ethernet UI on runtime presence, fix stale 0x26 comment'
status: To Do
assignee: []
created_date: '2026-06-14 20:11'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.16
parent_task_id: TASK-865
ordinal: 81000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (board-delta audit 2026-06-14 — make the combo honest)
1. HAS_ETH_CAPABLE is compile-time 1 on the combo (inherited from the OTGW32 base, never overridden in the COMBO delta). On a combo-on-Classic boot the Ethernet settings struct + REST/UI fields (ethstaticip/ethdns/ethgateway/ethipaddress/ethsubnet) all compile in and are shown to the user on a board with no W5500. Functionally inert (initEthernet skipped via !isPICEnabled, probeW5500 NAKs) but a UX wart: a Classic user sees Ethernet settings that can never do anything.
2. The TASK-865.12 watchdog fix made the boards.h:235-238 comment stale: it says the combo feeds 0x26 "only when the PIC was detected (isPICEnabled())", but the feed is now an unconditional NAK-safe write (OTGW-Core.ino feedWatchDog). Comment overstates a gate the code no longer has.

## What
1. Gate the Ethernet UI/settings EXPOSURE on runtime state.hw.bEthernetPresent (or eMode==OT_DIRECT), not the compile-time HAS_ETH_CAPABLE flag, so a Classic-mode combo does not present dead Ethernet fields. Keep the driver compiled in (HAS_ETH_CAPABLE); only the user-facing presentation is runtime-gated.
2. Fix the boards.h:235-238 comment to match the unconditional 0x26 feed.

## Anchors
- restAPI.ino:3086-3088,3126-3127 (ETH REST fields); settingStuff.ino:438-440 (ETH settings); data/index.js (ETH UI section); state.hw.bEthernetPresent; boards.h:235-238 (stale comment).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build esp32 + esp32-classic + esp32-combo all SUCCESS; evaluate.py --quick no new failures
- [ ] #2 Ethernet UI/settings are hidden when bEthernetPresent is false (combo in PIC/Classic mode) and shown when present (OTGW32); the driver stays compiled in
- [ ] #3 boards.h 0x26 comment matches the unconditional NAK-safe feed (no isPICEnabled gate claim)
- [ ] #4 FIELD (epic TASK-865): combo on Classic shows no Ethernet fields; combo on OTGW32 shows them and Ethernet still works
<!-- AC:END -->
