---
id: TASK-1121
title: Boot commands are typed into the diagnose PIC menu at every boot
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-03 20:17'
updated_date: '2026-09-03 21:00'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 211000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
sendOTGWbootcmd() (OTGW-Core.ino:868, worktree wt-otgw-1.x.x) guards on isPICEnabled() and settings.otgw.bEnable only, with no PIC firmware-type check. It is called from setup() at OTGW-firmware.ino:205. On hardware running the DIAGNOSE PIC image, whose interface is a menu that reads single keystrokes, the user's configured boot commands (default GW=1) arrive as keystrokes and select menu entries at every boot.\n\nFound while designing a diagnose-mode screen, reported by Schelte Bron, author of the PIC firmware. Note that the neighbouring suspicion is NOT true and should not be fixed: the PR= settings poll is already gated, because queryNextPICsetting() (OTGW-Core.ino:677) opens with isPICEnabled() and isGatewayFirmware(). Only the boot-command path is ungated.\n\nThe naive fix is wrong. Gating on isGatewayFirmware() at :205 would break boot commands on real gateways: sType is written only in processOT()'s banner branch (OTGW-Core.ino:4552) and in fwreportinfo() (:5135), and neither can have run by then, since resetOTGW() at :199 restarts the PIC and doBackgroundTasks() is gated on state.bSetupComplete which is set at the end of setup(). A positive gate would therefore see an unset or stale value and skip the commands.\n\nThe polarity is what makes it safe. Gate negatively on the typed accessor rather than the string: OTGWSerial::firmwareType() returns an enum initialised to FIRMWARE_UNKNOWN (OTGWSerial.cpp:53, enum at OTGWSerial.h:39-45). Skipping only when it equals FIRMWARE_DIAG fails open: at boot it is UNKNOWN, so gateways keep their boot commands, while a PIC already identified as diagnose is left alone. This also avoids a separate defect: state.pic.sType is char[32] (OTGW-firmware.h:265), so comparing it with == is a pointer comparison that is always false, and the project rule prefers typed discriminators over string tokens anyway.\n\nA second, latent instance of the same class: OTGW-firmware.ino:284-285 writes PR=A directly to the UART every minute, bypassing the command queue, gated on the device-id string rather than on firmware type. It avoids the diagnose menu only because diagnose 2.2 resolves to pic16f1847 instead of unknown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Configured boot commands are not sent when the PIC runs the diagnose firmware
- [ ] #2 Boot commands still run unchanged on a gateway PIC, including on the very first boot when the firmware type is not yet known
- [ ] #3 The gate uses the typed OTGWSerial::firmwareType() accessor, not a string comparison against state.pic.sType
- [ ] #4 The latent PR=A writer at OTGW-firmware.ino:284 is either gated the same way or documented as deliberately left alone, with the reason
- [ ] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Gate sendOTGWbootcmd() (OTGW-Core.ino:868) negatively on the typed accessor: return early when OTGWSerial.firmwareType() == FIRMWARE_DIAG. Precedent for the call shape is networkStuff.ino:563, which already does firmwareType() != FIRMWARE_OTGW for the time command with the same intent.
2. Record the polarity reasoning in the comment, because the obvious fix is the wrong one: a positive isGatewayFirmware() gate fails closed and would drop boot commands on real gateways, since at that point in setup() the type is whatever detectPIC() happened to catch.
3. Decide AC #4 on the latent PR=A writer at OTGW-firmware.ino:284 rather than reflexively gating it, and document whichever way it goes.
4. Build and run the evaluator.
<!-- SECTION:PLAN:END -->
