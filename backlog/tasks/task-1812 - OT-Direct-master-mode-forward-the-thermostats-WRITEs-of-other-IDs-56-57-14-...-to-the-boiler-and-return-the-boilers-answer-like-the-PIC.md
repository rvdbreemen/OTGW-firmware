---
id: TASK-1812
title: >-
  OT-Direct master mode: forward the thermostat's WRITEs of other IDs (56, 57,
  14, ...) to the boiler and return the boiler's answer, like the PIC
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T12:30:00Z'
updated_date: '2026-11-15T06:00:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - feature
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 905000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report E, U1. Robert's decision (2026-11-11, AskUserQuestion in the coordinator session): "Forward to the boiler": thermostat WRITEs of other IDs (56/57/14/...) in master mode are forwarded as a boiler write and the boiler's answer goes back to the thermostat (like the PIC). Code today (dev 755203ef0): handleMasterModeSlaveFrame (OTDirect.ino:2801-2848) answers every thermostat WRITE_DATA with a WRITE_ACK echo of the thermostat's own data (:2817-2819) and acts only on MsgID 1, 16 and 24 (:2825-2836). Every other write (e.g. 56 DHW setpoint, 57 max CH setpoint, 14 max modulation, 8, 7, 9, 100, 20-22) is acknowledged and dropped, so the thermostat believes its DHW setpoint or max-CH value is set while the boiler never saw it. The PIC relays the thermostat's frame and returns the boiler's reply (WRITE_ACK, Data-Invalid or Unknown-DataID). Design points for the implementer: the slave answer must leave within the OpenTherm response window (spec v4.2: 20-800 ms), while the master port runs its own schedule, so state how the forwarded write and its answer fit that window (a relay in the same slot, as gateway mode does, or another choice with its reason); keep MsgID 1 (TASK-1150, and the TSet expiry task filed with this one), 16 and 24 as they are unless the design says otherwise; keep the SAT ownership rules. ADR-066 (hybrid master mode) does not describe thermostat writes: check with /adr-kit:context whether this needs a new ADR (Proposed, never self-accepted).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In master mode a thermostat WRITE of an ID other than 1, 16 and 24 is sent to the boiler as a WRITE_DATA with the thermostat's data
- [ ] #2 The thermostat receives the boiler's own answer to that write (WRITE_ACK with the boiler's data, Data-Invalid or Unknown-DataID), not a local echo; the answer stays within the OpenTherm response window or the task notes show why it cannot and what was chosen instead
- [ ] #3 MsgID 1, 16 and 24 and the SAT ownership rules behave as before this change, shown by a test
- [ ] #4 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #5 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
The TSet expiry task filed together with this one is TASK-1811.

DESIGN (d3, 2026-11-15, design only, no code; for Robert via the coordinator). /adr-kit:context: no ADR covers thermostat writes in master mode. ADR-066 says the virtual boiler answers 'with cached boiler data' and lists 'instant responses' as a benefit. A forwarded write answered by the boiler changes that contract, so it needs a new ADR (Proposed, amends ADR-066). Side finding: ADR-066 says bEnableSlave defaults to false; the code defaults it to true (OTDirecttypes.h:114).

D1 What is forwarded: in master mode, a thermostat WRITE_DATA of any ID except 1, 16 and 24 goes to the boiler as a WRITE_DATA. 1, 16 and 24 keep today's local WRITE_ACK and write-cache path (TASK-1150, TASK-1811). While SAT owns the setpoint (satOwnsControlSetpoint), the IDs behind the commands SAT refuses from outside (satBlocksExternalCommand: CS=1, C2=8, MM=14; CH/H2 are MsgID 0 bits, TT/TC answer state) keep today's local echo and are not forwarded, so a thermostat cannot change what SAT controls. A user write override for the same ID (SW=56, SH=57, ...) wins as in gateway mode: the frame goes out with the override's data (origin OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN, buildOverriddenReply), so the thermostat and the scheduled user write do not fight.

D2 Timing (spec v4.2: the slave answers 20-800 ms after the request): the write is a relay in the next free master slot. The pending thermostat frame waits only for a master request already in flight (otMasterRequestActive), then goes out ahead of the command ring and the schedule; scheduleMasterRequest() starts no own frame while one waits. Worst case = the boiler latency of the frame in flight + that of the relay. With boilers answering within the spec's 800 ms that can exceed 800 ms. Then the existing TASK-1798 hold drops the late answer and the thermostat retries (spec 4.5); the boiler still got the value. To make that bound right, the hold deadline (OTD_REPLY_DEADLINE_MS 750) must count from the thermostat frame's arrival, not from the reply's creation (a change for the master path only). Rejected: (a) answering the write locally first and forwarding later (the thermostat would again see an answer the boiler never gave, the defect itself); (b) aborting the own frame in flight (the library cannot cancel a frame on the wire).

D3 Answers: the boiler's reply goes back through replyToThermostat() as in gateway mode: WRITE_ACK with the boiler's data, Data-Invalid (the boilerDataInvalid path), Unknown-DataID; a timeout or corrupt reply gives no answer (the thermostat retries, as behind a PIC). Unknown-DataID counts toward the 3-strike UI= rule as for the gateway's own frames. While the boiler is offline (TASK-1807) nothing is forwarded or answered. D4 READs: unchanged (cache answers, ADR-066). D5 Tests (AC3/AC4): the real handleMasterModeSlaveFrame + the master consumer with a fake bus: a 56 write reaches the boiler with the thermostat's data and the thermostat gets the boiler's WRITE_ACK/DI/UNK; 1/16/24 and the SAT-owned 8/14 keep the local echo; an SW= override replaces the data; a write arriving while an own frame is in flight goes out right after it, ahead of the schedule.

Correction to D2 (spec read in full): the answer window is 20-400 ms, typically 100 ms or less (spec v4.2 :1093-1095), not 800 ms; and a slave that functions as a gateway may exceed it: 'the maximum waiting time does not apply' (:1097-1098). A forwarded write is exactly that case, so the relay is within the spec. ADR-241 Proposed (number reserved by the coordinator), amends ADR-066; 'adr relate' to ADR-066 refused (ADR-066 has no Status History block), so ADR-066 is named in the text only; related to ADR-179. Open question in ADR-241 for Robert: the slot rule and the SAT exclusion of 8/14. bEnableSlave default conflict filed as TASK-1953. No code until Robert accepts.

Merge note (2026-11-15, coordinator): TASK-1954 lands first and owns OTD_MWT_MS and otdLastConvEndMs (declared next to otLastAnySendMs, stamped in handleMasterResponse() on every received frame, not on a timeout). At its rebase lane-1924-1812 removes its own definitions (OTDirect.ino ~:840-844) and its own stamp in the TASK-1812 block, and its relay uses 1954's otdLastConvEndMs. Check test_otd_master_write_relay 'busy' (MWT) after the rebase.
<!-- SECTION:NOTES:END -->
