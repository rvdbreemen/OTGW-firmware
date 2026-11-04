---
id: TASK-1178
title: >-
  OT-Direct gateway mode never answers a thermostat frame that an active
  override modified
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 09:12'
updated_date: '2026-09-30 11:49'
labels:
  - bug
  - otdirect
  - gateway
dependencies: []
priority: high
ordinal: 301000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev / 2.0.0, OT-Direct gateway mode; verified by reading the code on 2026-09-30, not yet reproduced on hardware)

In gateway mode a thermostat frame goes through applyOverrides() (src/OTGW-firmware/OTDirect.ino ~:1406-1432). When an override for its MsgID is active, the data is replaced and modified = true. The frame is then sent to the boiler with origin OT_DIRECT_ORIGIN_GATEWAY instead of OT_DIRECT_ORIGIN_THERMOSTAT (:2002-2003: 'modified ? OT_DIRECT_ORIGIN_GATEWAY : OT_DIRECT_ORIGIN_THERMOSTAT').

handleMasterResponse() only sends the boiler's reply back to the thermostat when otLastRequestOrigin == OT_DIRECT_ORIGIN_THERMOSTAT (:1375, otSlave.sendResponse at :1387). There is no other reply path for GATEWAY-origin frames: the function ends by resetting the origin to GATEWAY (:1403). The five otSlave.sendResponse sites are :1243 (loopback), :1387 (forwarded THERMOSTAT origin), :1976 (UI table), :1989 (SR table) and :2558 (master-mode slave handler).

Consequence: while any override is active (TT/TC room-setpoint override on MsgID 16, CS= control setpoint on MsgID 1, or any other otOverrides entry), every thermostat request for that MsgID goes unanswered. OT spec v4.2 requires a slave to answer every valid request, and the PIC gateway always answers the thermostat. A thermostat that misses replies reports communication errors and may fall back. The thermostat overrides via Home Assistant climate setpoints are the core use of gateway mode.

Found as follow-up (b) of the TASK-1173 judge; this task records the code-level verification. 1.x: not applicable (no OT-Direct).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The required reply semantics for an overridden frame are established from primary sources and recorded in the notes with file:line: OT spec v4.2 (slave response rules) and the PIC gateway firmware (other-projects/otgw-6.6): which data the PIC returns to the thermostat for an overridden WRITE and READ (the boiler's reply as-is, or the thermostat's original value), and whether the PIC ever leaves a thermostat request unanswered
- [x] #2 In gateway mode every thermostat request that is forwarded to the boiler, modified or not, produces exactly one reply to the thermostat (or the documented deliberate exception from AC#1), for both a boiler reply and a boiler timeout
- [x] #3 Old-vs-fix proof on the same setup: OLD shows the thermostat's request for an overridden MsgID without any reply (no 'A'/reply frame to the thermostat, or thermostat error), FIX shows the reply. Real thermostat on the OTGW32 slave terminals preferred; a host harness is acceptable only if it compiles the real handleMasterResponse/gateway code (sliced, not re-implemented)
- [x] #4 Monitor mode stays transparent and the UI/SR table paths are unchanged
- [x] #5 The change ships in one commit with its own prerelease bump; build.bat (esp32 and esp32-combo) SUCCESS with fresh binaries; evaluate.py --quick shows no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30. Verified statically by reading OTDirect.ino (the :2003 origin choice, the :1375 reply gate, the :1403 origin reset, applyOverrides' modified flag). Needs a thermostat on the OTGW32 slave terminals for the hardware proof; the bench currently has none.

2026-09-30 AC#1: reply semantics from primary sources (research workflow wf_e818d013-04c: PIC firmware, OT spec + OTGW docs, OT-Thing; synthesis re-checked the decisive lines; I re-read the spec rule and the PIC CreateMessage myself).
GOVERNING RULE (all sources agree; the firmware is the only outlier): a thermostat request that the gateway forwards gets exactly ONE reply, sent in reaction to the boiler's reply for that exchange; an override never suppresses it.
- OT spec v4.2 (docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:1133-1136): 'After the answer from the next slave in line is received, a answer is sent to the master. If the original message was meant for the gateway, then the gateway will create the answer, otherwise the received answer is forwarded to the master. Within 7ms an answer has to be sent to the master.'
- PIC (other-projects/otgw-6.6/gateway.asm:2456-2472, :3464-3471): for an override, SendAltResponse sets the response bit and CreateMessage 'Combine[s] W and the original message type', restores data bytes #1/#2 and the message ID: the thermostat gets WRITE-ACK with its OWN original data, whatever the boiler answered. OT-Thing relays every boiler reply in repeater mode (otcontrol.cpp:658-692).
SEMANTICS TO IMPLEMENT:
1) Overridden WRITE-DATA (CS 1, CC/CL 7, C2 8, MM 14, TT/TC/BS 16): WRITE-ACK with the thermostat's original data (parity recomputed) for a boiler WRITE-ACK, DATA-INVALID or UNKNOWN. Exception 56/57 (PIC code trace only): boiler WRITE-ACK -> its echo; boiler UNKNOWN -> the override value; DATA-INVALID -> the original.
2) Overridden READ-DATA: never rewrite a READ's data; forward it unmodified and relay the boiler's reply.
3) Boiler timeout or corrupt reply: send nothing (PIC, OT-Thing and the library demo all stay silent; the library only reports TIMEOUT after 1 s). This is the deliberate exception AC#2 allows.
4) Unmodified frame with boiler DATA-INVALID or UNKNOWN: relay unchanged. Today DATA-INVALID is dropped (handleMasterResponse acts on SUCCESS only), so AC#2 covers it too. The loopback reply gate (:1242) has the same origin defect.
Fix sketch: new origin OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN, a snapshot of the original thermostat frame, applyOverrides only rewrites WRITE_DATA and only when the value differs, buildOverriddenReply() helper, DATA-INVALID classification (status INVALID + good parity + type 6 + matching ID), one reply path for SUCCESS/DATA-INVALID, same rule in loopback; monitor, UI/SR tables and master mode untouched. ADR-179: the reply path writes no MsgID 1 state, and the PIC's CS cancellation on a rejected value is NOT ported (it would be a sixth, ungated MsgID 1 writer).
OPEN (maintainer): (a) TT/TC is still not effective after this fix: MsgID 9/100 are never answered by the gateway; PIC parity needs gateway-built READ-ACKs for 9 and 100, its own task and probably an ADR. (b) 56/57: PIC-exact three-case rule (recommended, used) vs a uniform WRITE-ACK(original). (c) Porting the PIC's override cancellations on a rejected value. (d) Latency: replies leave >= 50 ms after the boiler reply (library gate), beyond the spec's 7 ms; no staleness guard. Full synthesis: %LOCALAPPDATA%/OTGW-capture/task1178-synthesis.json

Integration 2026-09-30 (main thread, alpha.389):
- Patch sha256 234324505efe... (82052 bytes, reviewed plus fixup) applied cleanly to dev 9c1823528 (offset +7 from the version banners).
- Own harness run, build_and_run_override_reply.ps1 -OldVsFix -OldRev HEAD: FIX 40/40, OLD 13/40; 25/25 defect cases (OLD sends no reply, FIX does), 13/13 unchanged cases byte-identical including cache and online state, 2/2 log-only; RESULT: PASS. Transcript: %LOCALAPPDATA%/OTGW-capture/a1-patches/TASK-1178-run-oldvsfix-main-9c1823528.txt
- Own slice audit: all 46 OTDirect.ino slices and all 4 OTDirecttypes.h slices in test/host/generated/override_reply/{old,fix} are byte-equal to the source lines they name (working tree for FIX, git HEAD for OLD). The harness compiles the real code.
- CORRECTION to open question (b): the 56/57 rule is not 'PIC-exact'. gateway.asm setbyte1 (:2672-2677) compares the whole byte1, parity bit included, so the PIC answers echo / override value / own value only for a thermostat frame with parity bit 0. For parity bit 1 it answers a boiler WRITE-ACK with WRITE-ACK(SW/SH) and a DATA-INVALID with WRITE-ACK(dhwsetpointmax/chsetpointmax). The firmware applies one parity-independent rule; harness cases C7a-f are parity-1 frames, C7g/h parity-0. Single source (gateway.asm trace, reviewer plus fixup agent); no PIC test covers a thermostat WRITE(56/57) under SW=/SH=.
- Further PIC deviations, notes only: an equal-value override on MsgID 16 is relayed, while the PIC ACKs 16 via missingdata; for 56/57 with parity bit 1 the PIC treats even an equal value as modified; hotwaterreturn/maxchsetptret also run on unsubstituted 56/57 frames (gateway.asm:2458), turning a boiler UNKNOWN into WRITE-ACK(SW/SH, or the boiler maximum) and zapping SW/SH on a DATA-INVALID (predates this fix). The generic WRITE-ACK(own) rule is traced for IDs 1/7/8/14/16 only; for 20 (SC=) the PIC answers with its own clock, so 'matches the PIC' does not extend to 20/24/71.
- Deliberate behaviour changes beyond the defect, each pinned by a harness case: monitor mode relays a boiler DATA-INVALID (C20; OLD dropped it, so monitor mode is now more transparent, not less); an RM=-modified reply is logged as A (C17); a gateway-origin DATA-INVALID is logged as B (C19); READs of overridden IDs are relayed, not data-rewritten (C8); an equal-value override is a plain relay (C14); no duplicate T on a retry (C21); a DATA-INVALID answer to MsgID 0 no longer marks the bus offline (C26).
- Surface effects, verified by reading only: the REST override list shows kind 'answer' instead of 'substituted' for overridden WRITE IDs (value unchanged); RM= IDs gain an ADR-118 store entry and a retained <label>/override topic; the _thermostat subtopic carries the A value; a DATA-INVALID B updates tBoilerLastSeen and calls satNotifyBoilerFrameSeen().
- Pre-existing, found by the fixup agent: the SUCCESS path caches a boiler UNKNOWN-DATA-ID reply as a valid value (cache 01:0000 in C2), in OLD as well. Filed as its own task.

AC#5 evidence 2026-09-30: build.bat --target all at alpha.389 (+9c18235): esp32, esp32-classic and esp32-combo firmware and filesystem all SUCCESS, 'Build completed successfully!'; fresh binaries (.pio/build/*/firmware.bin 13:41:50 / 13:44:43 / 13:47:33, littlefs.bin right after), flash 79.4% / 77.1% / 81.3%. Log: %LOCALAPPDATA%/OTGW-capture/build-alpha389-task1178.log. evaluate.py --quick: 77 checks, 70 passed, 0 warnings, 0 failed. Full evaluate.py: 81 passed, 4 warnings (the known String counts, large local buffers, uncommitted working tree), 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct gateway mode now answers every forwarded thermostat frame, including one whose data an active override replaced. Before, an overridden WRITE was sent with gateway origin and the thermostat never got a reply.
- New origin OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN plus a snapshot of the thermostat's own frame; the reply to an overridden WRITE is WRITE-ACK with the thermostat's own Data-ID and data (the PIC's CreateMessage rule, traced for IDs 1/7/8/14/16). MsgID 56/57 use one parity-independent rule (boiler WRITE-ACK: the echo; UNKNOWN: the override value; DATA-INVALID: the thermostat's value); the PIC matches it only for frames with parity bit 0.
- applyOverrides substitutes only a WRITE-DATA whose value differs; READs pass unchanged. A genuine boiler DATA-INVALID (even parity, type 6, matching ID) is logged as B, relayed, not cached, and no longer marks the bus offline. A timeout or corrupt frame gets no reply (OT spec 4.5: the thermostat retries, as behind a PIC). A reply that differs from the boiler frame is logged as A, only after the slave accepted it. Loopback uses the same reply rule.
Evidence per AC:
- AC#1: primary-source trace in the notes (OT spec v4.2, gateway.asm file:line), corrected for the 56/57 parity dependence.
- AC#2 and AC#3: host harness compiling the real sliced code (50/50 slices byte-equal to the source, audited in the main thread): FIX 40/40, OLD 13/40; 25/25 defect cases OLD without a reply that FIX sends; timeouts/corrupt frames get no reply by design. Transcript a1-patches/TASK-1178-run-oldvsfix-main-9c1823528.txt. Mutants M1a/M1b/M2/M3, G1-G4, R1, P1-P3 all killed. No hardware run: the OTGW32 bench has no thermostat attached.
- AC#4: monitor mode and UI/SR table cases C12, C13a, C13b, C25 byte-identical OLD vs FIX; C20 now relays a boiler DATA-INVALID that OLD dropped.
- AC#5: alpha.389, three targets built fresh, evaluate --quick 0 FAIL.
Open for the maintainer: TT/TC via MsgID 9/100 still not effective (own task/ADR), the PIC's override cancellations, reply latency. Follow-up filed: TASK-1184 (boiler UNKNOWN cached as a valid value).
<!-- SECTION:FINAL_SUMMARY:END -->
