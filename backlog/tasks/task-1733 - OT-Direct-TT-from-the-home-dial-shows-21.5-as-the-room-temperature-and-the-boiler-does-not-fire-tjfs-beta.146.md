---
id: TASK-1733
title: >-
  OT-Direct: TT= from the home dial shows 21.5 as the room temperature and the
  boiler does not fire (tjfs, beta.146+)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-08T15:26:00Z'
updated_date: '2026-11-09T06:54:00Z'
labels:
  - bug
  - otdirect
  - field-report
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 840000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-08 15:12 #alpha-testing (msgs 1569006644661190766, 1569006818347458663, screenshot): turned the dial up to 21.5 with SAT off; UI said 'thermostat override 21.5 until its next program change', the hero big number (bRoom = model.room = MsgID 24 room temperature, v2.js:1215) showed 21.5 while 'target 20.0' (model.roomSet = MsgID 16) stayed, and the boiler did not fire. So the TT value appears as room temperature instead of the thermostat taking over the setpoint (TASK-1718 MsgID 9 answer, TASK-1717 dial). Needs: a capture from tjfs during a TT test, and a host replay of TT=21.5 in gateway mode checking which MsgID carries 21.5 towards the boiler/thermostat and what the web UI parser sees.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture or host replay shows why 21.5 lands on MsgID 24 / the room temperature display (file:line)
- [ ] #2 Fix: TT= in OT-Direct gateway mode changes the setpoint path only; room temperature display unchanged; red/green test
- [ ] #3 Field: tjfs confirms the thermostat takes the override and the boiler fires
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 host analysis (session OTGW-firmware toegang). (1) Boiler not firing: tjfs's thermostat never READs MsgID 9. His 20-min capture 20261107-201007 (beta.127) has thermostat frames only for MsgID 0, 1, 15, 16, 17, 24, 25, 27 (the R 9 reads are the gateway polling the boiler). A TT= override only takes effect when the thermostat reads MsgID 9 and adopts it (TASK-1718 answers that read); MsgID 16 to the boiler is informational, so TT= cannot change this thermostat's heating, on OT-Direct or on a PIC. (2) 21.5 as room temperature: not reproduced. Host replay tests/test_otd_gateway_alt_slots.py case T2 (real OTDirect.ino slices incl. TASK-1707 alternative slots and the TASK-1718 MsgID 9 reply): gateway mode, boiler answers MsgID 24 Unknown-DataID as in tjfs's 39 s capture (so 24 becomes an alternative slot), tjfs's thermostat stream without MsgID 9, TT=21.5 applied as setRemoteOverride() does (queue WRITE 16 and 100, write cache, overrides). The bridge log decoded with v2.js's rules: MsgID 24 stays the thermostat's 0x1499, 0 frames carry 21.5 on 24; MsgID 16 (type 1) = 0x1580 = 21.5 in 11 R frames; the boiler gets 11 x WRITE 16 = 21.5. The firmware sets Tr (roomtemperature in /api/v2/otgw/otmonitor) only from a MsgID 24 frame (OTGW-Core.ino:5047). The screenshot is also inconsistent: v2.js renders the big number and the green tick from the same model.room (v2.js:1205-1215), yet the tick sits well below the setpoint arc. Test test_T2_tt_lands_on_the_setpoint_not_on_the_room_temperature passes on dev (characterisation: the defect does not occur in the replay). AC1 stays open: needs a capture from tjfs during TT=21.5 (via REST/MQTT now that TASK-1735 removes the dial) or the thermostat model; the coordinator asks tjfs. No firmware fix.

2026-11-08 17:1x (Discord agent): tjfs answered at 16:07 (msg 1569020552834850847): Ideal Halo Lite thermostat, Ideal Logic boiler. Acknowledged in #alpha-testing; capture during TT=21.5 still asked. (carried over from the main dev tree copy by otgw-firmware-4c, 2026-11-08)

2026-11-09 otgw-firmware-80, root-cause analysis FROM THE CODE, NOT REPRODUCED (tjfs capture of the TT test is not available). ACs stay open.

I have not reproduced this. tjfs's capture taken during the TT test is not in the tree, and I did not run any test, build or device. Everything below comes from reading the code. The firmware I analysed is the same as beta.146: `git diff bc8f07818 HEAD` on OTDirect.ino, OTDirecttypes.h and OTGW-Core.ino changes only the version banner lines.

**S3, boiler does not fire: explained by the code, and expected.** TT= changes only two things: the MsgID 16 override and a one-off MsgID 100 write. It does not touch MsgID 0 bit 0 (CH enable) or MsgID 1 (TSet); both stay the thermostat's own. Per the OpenTherm spec, the thermostat learns about an override only by reading MsgID 9 (and MsgID 100). The firmware answers those reads correctly (TASK-1718). If tjfs's Ideal Halo Lite never reads MsgID 9, it never adopts 21.5, so it does not raise CH enable or TSet. A PIC would behave the same. The claim that this thermostat never reads MsgID 9 rests on one source only: the task note's analysis of capture 20261107-201007 from beta.127, which is not in the tree. I did not re-check it.

**S1, 21.5 shown as the room temperature: no code path found.** The room temperature only reaches the UI from a frame with MsgID 24. Nothing in the TT= path writes MsgID 24 or sends anything on it. The only firmware writer of MsgID 24 is the MQTT `otgw32/room_temp` command, which has nothing to do with TT=.

**S2 contradicts the code model too.** With the override active, every thermostat WRITE 16 = 20.0 goes to the boiler rewritten as 21.5, logged as a T frame then an R frame. v2.js takes MsgID 16 from any type-1 frame, so the R frame is the last one seen and the target should read 21.5. The REST value (`roomsetpoint`) should also follow the R frame. tjfs saw 20.0. So the field unit differs from the model on both values, not only on the room temperature.

**Most likely explanations, all hypotheses:**
- **(H1) The override never took effect, and 21.5 was the real room temperature.** This explains S2, and S3 as well (with the room at 21.5, a thermostat set to 20.0 asks no heat). The UI message 'thermostat override 21.5 until its next program change' is no evidence that it worked: REST answers 202 'queued' before the command runs, and beta.146 printed the message on any `r.ok`. One concrete way the TT= gets lost while still looking confirmed: when the 12-slot command queue is full, `setRemoteOverride()` returns silently, but the TT branch still sends the 'TT: 21.50' confirmation.
- **(H2) The screenshot was taken before the next thermostat MsgID 16 cycle.** This would explain S2 only.
- **(H3) The values were misread from the screenshot.** The task note already flags the screenshot as inconsistent: big number and tick both come from `model.room`, yet the tick sat below the setpoint arc. I could not see the screenshot myself.

**Separate finding: the OT-Direct TT= is not PIC parity, although its comment says it is.**
- On the PIC (otgw-6.6 gateway.asm), TT= reaches the thermostat only through the answers to its MsgID 9 and MsgID 100 reads. The PIC's MsgID 16 handler never puts the TT value on MsgID 16 towards the boiler; it only substitutes the BS= fake setpoint. It uses the thermostat's own MsgID 16 to detect acceptance, and drops the override after a few mismatches.
- OT-Direct instead:
  - queues a WRITE 16 to the boiler;
  - rewrites every thermostat WRITE 16 with the TT value;
  - queues a WRITE 100 to the boiler, although the spec marks MsgID 100 as read-only;
  - uses flags 0x02 for TT where the PIC uses 0b11.
- The auto-clear never releases a TT that the thermostat never honoured, so the override lasts until TT=0 or a reboot.
- None of this explains S1. It does mean the boiler sees TrSet = 21.5, which the thermostat did not set.

**The replay test proves less than it seems.** It is a characterisation test: it passes on the current code and has never been red. It does not run the real TT= path. It reimplements `setRemoteOverride()` by hand, stubs out the honour/auto-clear logic, and decodes with a C++ copy of the v2.js rules. Under the project rule, a test that reimplements the code under test proves nothing about that code.

**Fix candidates**, both behaviour decisions for Robert:
- **(F1, small, safe):** make TT=/TC= confirm only when the write was actually queued, as CS= already does. Otherwise answer NG.
- **(F2, PIC parity):** stop writing MsgID 16 and MsgID 100 to the boiler on TT=/TC=, and keep TT= as the MsgID 9/100 answers plus acceptance tracking on MsgID 16. This changes what the boiler sees, so it needs Robert's OK and an ADR check.

Neither fix addresses S1, because no code defect for S1 was found. Settling S1 and S2 needs tjfs's capture during a TT=21.5 sent over REST, MQTT or telnet. The dial no longer sends TT= since beta.153 (TASK-1735).

NUMBERS: Measured, by reading the code:
- Command ring: 12 frames (OTDirect.ino:338). One TT= uses 2, so about 6 un-drained TT=/TC= commands fill it.
- Honour and release thresholds: 0.25 C, 0.5 C and 3 cycles (OTDirect.ino:476-478).
- TT=21.5 is 0x1580 in f8.8. The thermostat value used in the harness is 0x1499, about 20.6 C (test file:375-376).

Estimate, not verified: the ring fills only if the queue does not drain. That happens when the boiler supports every Data-ID the thermostat asks (no alternative slot) and several TT=/CS=/MM= commands are sent. tjfs's 39 s capture, as summarised in the task note (I did not see it), shows MsgID 24 answered Unknown-DataID. If so, slots exist, which makes a full ring less likely for him.

No tests, builds or device measurements were run in this session.

FACTS:
- TT=xx.x is handled in handleOTDirectCommand: value 0 clears the override, any other value calls setRemoteOverride(OT_OVERRIDE_TEMPORARY, temp). It then calls synthesizeResponse(buf, rspBuf) unconditionally, so 'TT: 21.50' is reported even when setRemoteOverride() returned early. CS= confirms only when the write was queued. [D:/Users/Robert/Documents/GitHub/RvdB/wt-1689/src/OTGW-firmware/OTDirect.ino:2841-2849 (TT), 2826-2834 (TC); CS= confirms only `if (enqueued)` at OTDirect.ino:2808-2811]
- setRemoteOverride() queues WRITE 16 = f8.8(TT) first and returns silently, with the state unchanged, if the queue is full. It then queues WRITE 100 = 0x0002 for TT (0x0001 for TC) and commits otRemoteOverride.* and state.otd.*. [OTDirect.ino:2345-2381 (queue-full return at 2355-2358; flags at 2366)]
- enqueueWriteCommand() puts the frame in the command ring, updates the periodic write cache and calls setOverride(msgId, value). From then on every thermostat WRITE-DATA for that MsgID is rewritten. On a full ring it sets otdWriteRefused, so handleOTDirectCommand returns false and the caller marks the command Dropped. [OTDirect.ino:2257-2274, 3713; OTGW-Core.ino:3856]
- The command ring holds 12 frames. One TT= takes 2 of them (WRITE 16 and WRITE 100). [OTDirect.ino:338, 2355, 2367]
- The override table contains MsgID 16 (TT/TC) and MsgID 100. It contains no MsgID 24, so no override can rewrite a thermostat MsgID 24 frame. [OTDirect.ino:420-431]
- applyOverrides() first passes the thermostat's MsgID 16 to onThermostatMsgID16() (honour/release detection). It rewrites only WRITE-DATA frames, and only when the override value differs from the thermostat's own. Separately, it rewrites MsgID 0 bit 0 (CH enable), and only under the ADR-208 CS-override rule. [OTDirect.ino:607-648 (16 hook 613-616, MsgID 0 618-626, WRITE-only 628, substitution 630-645)]
- CH enable in the thermostat's relayed MsgID 0 changes only while a CS override is active, never because of TT=. MsgID 1 (TSet) has its own override entry, which only CS= sets. [OTDirect.ino:821-824 (otdEffectiveMasterFlags), 831-838 (otdRelayedStatusFrame), 421 and 2790-2811 (CS=)]
- The thermostat learns of the override only when it READs MsgID 9 or MsgID 100. replyToThermostat answers those reads with the override setpoint and the flags (TASK-1718), and otdIsAltSlot keeps 9 and 100 out of the alternative slots while an override is active. [OTDirect.ino:1397-1418 (otdRemoteOverrideReply), 1428-1450 (replyToThermostat), 451-454 (otdRemoteOverrideHoldsSlot), 1766-1770 (otdIsAltSlot)]
- OpenTherm v4.2: MsgID 9 is master-Read ('0 = no override, 1..30 = remote override room setpoint'). The master reads it to learn the override. MsgID 100 is 'R -' (read-only) with bit0 = manual change priority and bit1 = program change priority. MsgID 16 is master-Write 'current room temperature Setpoint'. [D:/Users/Robert/Documents/GitHub/RvdB/wt-1689/docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:2237-2244, 2292-2306, 2316-2318, 1844]
- On the PIC, TT= sets setpoint1/2 and the function bits: TT allows manual and program override (b'11'), TC manual only (b'01'). The setpoint reaches the thermostat only through the answers to its MsgID 9 and MsgID 100 reads. [other-projects/otgw-6.6/gateway.asm:4053-4062 (SetContSetPoint/SetTempSetPoint), 4065-4090 (SetSetPoint), 2933-2955 (MessageID9), 3272-3290 (MessageID100)]
- The PIC's MessageID16 handler does not put the TT setpoint into the thermostat's MsgID 16 towards the boiler: roomsetptchange substitutes only the BS= fakesetpoint. It compares the thermostat's MsgID 16 with the requested setpoint to detect acceptance (OverrideSet). Mismatches before acceptance increment the low bits of the 'override' register, and the override is cancelled once OverrideSet (bit 2) flips. My reading is that this is the comment's '3 attempts', taken from the bit definitions; I did not trace it on hardware. [gateway.asm:2976-3034 (MessageID16, roomsetptchange 3018-3023, roomsetptdiff 3024-3028), 429-434 (override bit definitions)]
- OT-Direct's TT=/TC= header comment calls it 'PIC parity', but it writes MsgID 16 and MsgID 100 to the boiler, which the PIC does not. Its TT flags are 0x02, where the PIC uses 0b11. [OTDirect.ino:2330-2344 (comment), 2355, 2366-2367; gateway.asm:4060]
- onThermostatMsgID16 releases a TT only after it has been honoured at least 3 times and the thermostat's value then moves by more than 0.5 C. A TT that the thermostat never echoes therefore stays active until TT=0 or a reboot. [OTDirect.ino:2420-2455; thresholds at 476-478]
- In gateway mode with a thermostat connected, the gateway sends its queued commands and its schedule only in alternative slots: thermostat Data-IDs that the boiler answered Unknown-DataID 3 times, or that the UI= table marks unknown. In such a slot the thermostat gets Unknown-DataID carrying its own data, and the queued WRITE 16 goes to the boiler. If the boiler supports every Data-ID the thermostat asks, the queue never drains in gateway mode. [OTDirect.ino:1669 and 855-857 (otdOwnFramesAllowed), 1806-1837 (otdAltSlotTaken), 1766-1770]
- The schedule has a write entry for 16 but only a read entry for 100. updateWriteCache(100, ...) is therefore a no-op, and WRITE 100 goes to the boiler once, through the queue. [OTDirect.ino:164, 248, 676-685 (updateWriteCache)]
- The only firmware path that writes MsgID 24 towards the boiler is the MQTT command set/<node>/otgw32/room_temp. It is not linked to TT=, and no HA discovery entry under src/ references it. [OTDirect.ino:2069-2073 (otdMqttSetRoomTemp); MQTTstuff.ino:933-934; grep for 'otgw32/room_temp' under src/ found nothing]
- v2.js (beta.146 and current) sets model.room from any frame with MsgID 24 of type 1, 4 or 5, whatever its prefix (T/B/R/A). It sets model.roomSet from any type-1 MsgID 16 frame, T or R. renderB shows bRoom = model.room and 'target ' + model.roomSet. In beta.146, stepRoomOverride does not touch model.room or model.roomSet. [git show bc8f07818:src/OTGW-firmware/data/v2.js lines 518, 521 (parser), 1189-1216 (renderB), 3605-3616 (stepRoomOverride); current data/v2.js:624-625, 1319-1320]
- The REST seed maps model.room to otmonitor 'roomtemperature' (OTcurrentSystemState.Tr) and model.roomSet to 'roomsetpoint' (TrSet). [data/v2.js:3499; restAPI.ino:3092-3093]
- Tr (room temperature) is decoded only from MsgID 24, through the OTmap decode or the PS=1 summary. OGW-Core.ino:5047, cited in the task note as the frame path, is actually the PS=1 summary path. The live-frame decode is print_f88(OTcurrentSystemState.Tr) for OT_Tr. [OTGW-Core.ino:5376 (frame path), 5047 (PS=1 summary path), 5021-5027 (stampThermostatRoomTemp)]
- processOT treats a T frame followed within 500 ms by an R frame with the same id as a gateway substitution. The R frame becomes the canonical value. For TT, OTcurrentSystemState.TrSet would therefore hold the override (21.5), not the thermostat's 20.0. This is a code reading of the TASK-1259 comment and the pairing logic; I did not run it. [OTGW-Core.ino:5876-5890 (pairing), 6004-6012 (TASK-1259 comment)]
- The 'TT: 21.50' confirmation from synthesizeResponse cannot be parsed as an OT frame: isvalidotmsg rejects buf[2]==':'. The line is handled as a command response and sent to the web UI as a '<' event, which v2.js rawFromLine ignores. [OTGW-Core.ino:5317-5323, 6160-6168; data/v2.js:637-646]
- POST /api/v2/otgw/commands answers 202 {"status":"queued"} after addCommandToQueue(), before the OT-Direct handler runs. The beta.146 dial showed 'thermostat override X° until its next program change' on any r.ok, so that message is no evidence that the override took effect. [restAPI.ino:712-720 and handleCommandSubmit (addCommandToQueue then webSend 202, around restAPI.ino:365-367); bc8f07818 v2.js stepRoomOverride]
- tests/test_otd_gateway_alt_slots.py T2 does not call the real handleOTDirectCommand, setRemoteOverride or synthesizeResponse; it reproduces their effect by hand (otCmdEnqueue + updateWriteCache + setOverride for 16 and 100). It stubs onThermostatMsgID16, clearWriteOverride and satOwnsControlSetpoint. It never runs processOT or the REST otmonitor path, and it decodes the log with a C++ copy of the v2.js rules, not v2.js itself. Its thermostat stream and boiler model are synthetic: the boiler echoes every WRITE as a WRITE_ACK with the same data and answers 24 with Unknown-DataID. [tests/test_otd_gateway_alt_slots.py:204-217 (stubs), 230-249 (boiler model), 371-394 (T2), 516-524 (assertions)]
- What T2 does show, for the sliced real code (alternative-slot logic, applyOverrides, loopOTDirect, replyToThermostat): with an active MsgID 16 override and MsgID 24 as an alternative slot, no logged frame carries 0x1580 on MsgID 24, the boiler receives WRITE 16 = 0x1580, and the last type-1 MsgID 16 frame in the log is 0x1580. According to the task notes it passes on dev; it has never been red, so it is characterisation, not a reproduction. [tests/test_otd_gateway_alt_slots.py:516-524; task-1733 Implementation Notes]
- The firmware logic analysed equals beta.146: between bc8f07818 (beta.146) and HEAD, the only changes to OTDirect.ino, OTDirecttypes.h and OTGW-Core.ino are the version banner lines. [git diff bc8f07818 HEAD on those three files (only the '**  Version' lines differ)]

NEXT:
- Ask tjfs for a capture (capture-mqtt-debug.bat / capture-otgw.sh) covering: TT=21.5 sent over REST, MQTT or telnet; at least 2-3 full thermostat cycles; and the /api/v2/otgw/otmonitor JSON from before and after. The capture should settle: (a) whether a T 16 / R 16 = 0x1580 pair appears; (b) what MsgID 24 really carries; (c) whether the Halo Lite ever reads MsgID 9 or MsgID 100 while an override is active; (d) whether the 'TT: 21.50' line appears while the R 16 frames do not. Together these decide between H1, H2 and H3.
- Fix candidate F1 (small, no behaviour change on success): in the TT and TC branches (OTDirect.ino:2826-2849), call synthesizeResponse only when the write was actually queued, and send otDirectBridgeProcessStatus("NG") otherwise. CS= already does this at 2808. Red/green host test: slice the real handleOTDirectCommand TT branch, setRemoteOverride and enqueueWriteCommand into a harness; fill the ring to 12 frames; send TT=21.5; assert no 'TT: 21.50' line and an NG plus Dropped result. It should fail on the current code (confirmation emitted, override state unchanged) and pass on the fix.
- Fix candidate F2 (PIC parity, a behaviour decision for Robert, check ADRs with /adr-kit:context first): on TT=/TC=, stop queueing WRITE 16 and WRITE 100 to the boiler, and stop rewriting the thermostat's WRITE 16. Keep the MsgID 9/100 answers, and use the thermostat's MsgID 16 for acceptance and release (abandon after N mismatches, as the PIC's roomsetptdiff does), so an unhonoured TT does not stay active forever. Consider TT flags 0x03 for parity with the PIC. Red/green design: the real setRemoteOverride, applyOverrides and onThermostatMsgID16 (no stubs), with tjfs's stream without MsgID 9 reads. Assert that the boiler never receives a gateway WRITE 16 or WRITE 100, and that the override is released after N mismatching thermostat MsgID 16 frames. It should fail on the current code and pass on the fix. Put the decision to Robert as one question with the options, before any change.
- Strengthen test T2 into a real regression test: replace the hand-made override with a call to the real handleOTDirectCommand("TT=21.5"), un-stub onThermostatMsgID16 and clearWriteOverride, and feed the bridge log through the real processOT pairing, or at least through v2.js applyFrame in node. As it stands it only characterises the code.
- Fix the stale reference in the TASK-1733 note: OTGW-Core.ino:5047 is the PS=1 summary path. Tr from a live frame is set at OTGW-Core.ino:5376 (OT_Tr, print_f88).
- Keep AC1 open: it needs the capture. AC2's wording ('room temperature display unchanged') has no code defect to fix; that is for Robert to decide, and it may need rewording around F1 or F2. AC3 stays a field check.

NOT VERIFIED:
- Not reproduced. tjfs's capture taken during the TT=21.5 test does not exist in the tree, and I did not see his screenshot.
- That the Ideal Halo Lite never reads MsgID 9: this comes only from the task note's analysis of capture 20261107-201007 (beta.127, not in the tree). One source; I did not re-check it.
- That tjfs's boiler answers MsgID 24 with Unknown-DataID: taken from the task note (39 s capture). Not seen by me.
- I did not run tests/test_otd_gateway_alt_slots.py in this session. Its pass status comes from the task notes.
- Hypotheses H1 (override never applied; 21.5 was the real room temperature), H2 (screenshot taken before the next MsgID 16 cycle) and H3 (misread screenshot) are all unconfirmed. The code shows only that H1 is possible (silent queue-full drop with a confirmation still sent; 202 'queued' before execution), not that it happened.
- The PIC's mismatch counter ('3 attempts, then abandon') is my reading of gateway.asm:3024-3034 together with the bit definitions at 429-434. I did not check it on a PIC.
- Whether the Ideal Logic boiler does anything with TrSet (MsgID 16) or with WRITE 100: unknown. The spec calls MsgID 16 'current room setpoint' and marks 100 read-only.
- The processOT T/R pairing outcome for TrSet (canonical = R = 21.5) comes from reading the code, not from a run.
- The exact handleCommandSubmit line numbers in restAPI.ino (around 345-367) come from grep context, not a full read.

2026-11-09 Robert (in session otgw-firmware-80, AskUserQuestion): chose 'PIC parity now (F2)' over capture-first: change OT-Direct TT=/TC= to PIC semantics (no gateway WRITE 16/100 to the boiler, stop rewriting the thermostat's WRITE 16, keep the MsgID 9/100 answers, release the override after N mismatching thermostat MsgID 16 frames as the PIC's roomsetptdiff does). Before coding: /adr-kit:context for OT-Direct gateway behaviour, and the PIC firmware reference for TT/TC semantics (other-projects).

2026-11-09 Robert (session otgw-firmware-80, AskUserQuestion) settled the F2 parity sub-points after the read-only design research (PIC source other-projects/otgw-6.6/gateway.asm v6.6): (a) mismatch counting starts only after the thermostat read MsgID 9 (strict PIC, gateway.asm:2980-2982 + 2948); for a thermostat that never reads MsgID 9 the override stays active until TT=0, as on a PIC. (b) TT only: TC= keeps today's no-auto-clear; releasing TC changes SAT push-setpoint behaviour on OT-Direct and goes to Sergeant D as a question first (corrections entry 14 Open note). (c) Keep the 0.25 C match tolerance (documented deviation from the PIC's ~0.125 C, gateway.asm:3002). (d) Master mode keeps writing TrSet (MsgID 16) for TT=; F2 applies to gateway mode only. Design inputs (PIC semantics, OT-Direct trace, real-code test design) saved locally; F2 also needs an ADR via adr-kit (ADR-068 rationale lists TT/TC as WRITE 16/100 producers).

2026-11-09 ADR-223 drafted via adr-kit as Proposed (adr-lint --strict 0 FAIL; test_adr_governance OK): TT= in OT-Direct gateway mode is answer state only (MsgID 9/100 reads), no gateway WRITE 16/100, PIC release rule after a MsgID 9 read; TC= and master mode unchanged. Sergeant D asked about TC= via 97 (Q-1733a, #dev-sat-mqtt msg 1569226090260725952). Implementation starts after Robert accepts ADR-223.

2026-11-09 Sergeant D answered Q-1733a with A (#dev-sat-mqtt msg 1569231775144607755, corrections entry 286, recorded by otgw-firmware-97): TC= on OT-Direct behaves like the PIC too. ADR-223 (still Proposed) updated to cover TT= and TC= alike; Open Question answered in the record; master mode keeps TrSet for both. Implementation waits for Robert's acceptance of the final ADR-223 text.
<!-- SECTION:NOTES:END -->
