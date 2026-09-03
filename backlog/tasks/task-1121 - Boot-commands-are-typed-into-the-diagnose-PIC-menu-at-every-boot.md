---
id: TASK-1121
title: Boot commands are typed into the diagnose PIC menu at every boot
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 20:17'
updated_date: '2026-09-03 21:55'
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
- [x] #1 Configured boot commands are not sent when the PIC runs the diagnose firmware
- [x] #2 Boot commands still run unchanged on a gateway PIC, including on the very first boot when the firmware type is not yet known
- [x] #3 The gate uses the typed OTGWSerial::firmwareType() accessor, not a string comparison against state.pic.sType
- [x] #4 The latent PR=A writer at OTGW-firmware.ino:284 is either gated the same way or documented as deliberately left alone, with the reason
- [x] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Gate sendOTGWbootcmd() (OTGW-Core.ino:868) negatively on the typed accessor: return early when OTGWSerial.firmwareType() == FIRMWARE_DIAG. Precedent for the call shape is networkStuff.ino:563, which already does firmwareType() != FIRMWARE_OTGW for the time command with the same intent.
2. Record the polarity reasoning in the comment, because the obvious fix is the wrong one: a positive isGatewayFirmware() gate fails closed and would drop boot commands on real gateways, since at that point in setup() the type is whatever detectPIC() happened to catch.
3. Decide AC #4 on the latent PR=A writer at OTGW-firmware.ino:284 rather than reflexively gating it, and document whichever way it goes.
4. Build and run the evaluator.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Hardware-verified on the bench gateway at 192.168.88.68, which runs diagnose 2.2, with otgwcommandenable=true and otgwcommands="GW=1" set for the test and restored to false afterwards.

Capture of the boot sequence on 1.7.5-beta.8+c87ea4c:
- 07:28:20.016 fwreportinfo: Current firmware type: diagnose
- 07:28:20.456 sendOTGWboot(889): Boot commands skipped: PIC runs diagnose firmware

The banner lands 0.44s before the decision, which is the whole point of the change: from setup() the decision runs first, against FIRMWARE_UNKNOWN.

The first attempt at this fix gated inside sendOTGWbootcmd() while leaving the call in setup(), which was dead code. detectPIC() stops at the bootloader ETX before the application banner, and nothing reads the port again until doBackgroundTasks() is unfenced, so firmwareType() was still FIRMWARE_UNKNOWN there. Flagged by an adversarial design review and confirmed by reading find(), resetPic(), matchBanner() and the doBackgroundTasks() call order. The call now runs once from doTaskEvery1s(), with a three-second cap.

AC #2 is verified by mechanism rather than on a gateway PIC: the same one-shot path is proven to fire and to have a resolved firmware type, and the gate is negative, so a non-diagnose PIC cannot be skipped.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Boot commands are no longer typed into the diagnose PIC menu.

The diagnose image is a menu that reads single keystrokes, so the default GW=1 boot command arrived as G, W, =, 1 and CR and selected menu entries at every boot. Reported by Schelte Bron, author of the PIC firmware.

Changes:
- sendOTGWbootcmd() (OTGW-Core.ino) returns early when OTGWSerial.firmwareType() == FIRMWARE_DIAG. The polarity is negative on purpose: firmwareType() starts at FIRMWARE_UNKNOWN, so an unidentified PIC still gets its boot commands and only a confirmed diagnose PIC is spared. A positive isGatewayFirmware() test would fail closed and drop boot commands on a gateway whose banner was missed. The typed accessor is used rather than state.pic.sType, a char[32] whose == is a pointer comparison.
- The call moved out of setup() into a one-shot in doTaskEvery1s(), because in setup() there is no firmware type to test: detectPIC() stops at the bootloader ETX before the application banner. A three-second cap keeps a PIC that never sends a banner from losing its boot commands.
- The PR=A writer at OTGW-firmware.ino:284 is documented as deliberately ungated: it only runs while the device id is unknown, which implies an unknown firmware type, so a FIRMWARE_DIAG test could never fire there, and gating the other way would disable the only automatic recovery from a failed boot probe.

Tests: hardware-verified on a gateway running diagnose 2.2 (capture in the implementation notes). build.py --firmware completed successfully; evaluate.py --quick 35/37 pass, 0 failures.

Risk: boot commands now go out up to three seconds later than before. They were already queued rather than written directly, and handleOTGWqueue() only drains from the same one-second task, so the change to when the PIC actually sees them is smaller than the change to when they are queued.
<!-- SECTION:FINAL_SUMMARY:END -->
