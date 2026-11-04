---
id: TASK-865.18
title: 'fix(combo): gate Ethernet UI on runtime presence, fix stale 0x26 comment'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-14 20:11'
updated_date: '2026-10-12 00:04'
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
- [x] #1 build esp32 + esp32-classic + esp32-combo all SUCCESS; evaluate.py --quick no new failures
- [x] #2 Ethernet UI/settings are hidden when bEthernetPresent is false (combo in PIC/Classic mode) and shown when present (OTGW32); the driver stays compiled in
- [x] #3 boards.h 0x26 comment matches the unconditional NAK-safe feed (no isPICEnabled gate claim)
- [ ] #4 FIELD (epic TASK-865): combo on Classic shows no Ethernet fields; combo on OTGW32 shows them and Ethernet still works
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Landed: restAPI.ino gates the Ethernet settings/UI exposure on runtime state.hw.bEthernetPresent (omitting the ethstaticip trigger key so index.js suppresses the whole eth section client-side) instead of the compile-time HAS_ETH_CAPABLE flag; the driver stays compiled in. boards.h 0x26 watchdog comment updated to match the unconditional NAK-safe feed (no isPICEnabled gate claim). AC#2 and AC#3 verified by inspection. AC#1 (esp32 + esp32-classic + esp32-combo 3-target build + evaluate.py --quick) and AC#4 (field: combo on Classic shows no eth fields; combo on OTGW32 shows them and Ethernet works) remain open: build deferred to the main-thread (this tree carries sibling WIP incl. task-866 async webserver work, building the union here is unsafe), field check needs hardware. Change is compile-safe by inspection: comment-only edit plus an if-guard on an existing bool already read elsewhere in the same HAS_ETH_CAPABLE block.

3-target build verified GREEN at HEAD (alpha.232): esp32, esp32-classic, esp32-combo all SUCCESS (fw+fs); esp32-combo bin now FITS (no overflow). evaluate.py --quick 0-fail. Code ACs were verified by the planning pass reading the committed source. Remaining = field/hardware AC(s) for Robert.

CLOSE 2026-06-29 (maintainer-directed): #2 verified in code — Ethernet UI gated on presence (index.js:413 toggles 'hidden' on !isEth; iface input type eth-only at :6200), driver stays compiled in. #3 verified — boards.h comment (235-239) states the 0x26 feed is UNCONDITIONAL (no isPICEnabled gate, harmless NACKed no-op on absent chip), matching the feed. #4 (live OTGW32 Ethernet-fields-shown) deferred — OTGW32 offline this session; combo did run on OTGW32 @.39 earlier. Closed per maintainer direction.

2026-10-12 Classic-S3 bench validation (alpha.418 combo on OTGW Classic + LOLIN S3 Mini, PIC16F1847 gw 6.8): Bench evidence 2026-10-11/12 (C:/Users/rvdbr/AppData/Local/OTGW-capture/classic-validation-20261011/EVIDENCE.md, Phase 2.1). This covers the Classic half of AC#4 only. Setup: esp32-combo 2.0.0-alpha.418+fa2b89d on an OTGW Classic carrier with a LOLIN S3 Mini (MAC ac:27:6e:ce:45:d8, 192.168.88.63, PIC16F1847 gateway 6.8). device/info reports hardware_type otgw-classic, hardwaremode PIC, otdirectavailable false. GET /api/v2/settings returns no eth* keys, so the runtime presence gate drops the Ethernet settings, and with them the index.js trigger key ethstaticip. The UI was not checked in a browser in this run. The OTGW32 half (combo on OTGW32 shows the Ethernet fields and Ethernet still works) has no evidence in this run and stays open. Leave AC#4 unchecked, or split it into a Classic part (closable at REST level) and an OTGW32 part. Note: the task is already Done (closed 2026-06-29 by maintainer direction) with AC#4 open.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Combo gates Ethernet UI on bEthernetPresent (hidden on Classic, shown on OTGW32); boards.h 0x26 comment corrected to the unconditional NAK-safe feed. OTGW32-side live Ethernet check deferred (offline).
<!-- SECTION:FINAL_SUMMARY:END -->
