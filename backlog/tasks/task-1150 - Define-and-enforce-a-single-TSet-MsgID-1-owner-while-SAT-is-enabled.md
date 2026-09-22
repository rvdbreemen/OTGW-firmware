---
id: TASK-1150
title: Define and enforce a single TSet (MsgID 1) owner while SAT is enabled
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-22 06:37'
updated_date: '2026-09-22 10:24'
labels: []
dependencies: []
ordinal: 285000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported on Discord #dev-sat-mqtt 2026-05-29 by sergeantd: 'When SAT is enabled, ONLY SAT should drive TSet. Otherwise a conflict will occur.'

Verified against dev HEAD 2a2e5fba3 (2026-09-22), investigation plus an adversarial completeness pass that grepped every setOverride(1 / updateWriteCache(1 / enqueueWriteCommand(1 / clearWriteOverride(1 site. No sixth writer exists; the enumeration below is complete.

ALREADY FIXED: the exact 45<->10 flip-flop sergeantd saw. TASK-761 landed the same day (alpha.93, commit 45d95a226) gating OTDirect's heating-curve/PI MsgID 1 writer behind !state.sat.bActive (OTDirect.ino:2072-2085). Note TASK-761 AC#5 field validation is still unticked, so even that part has no hardware receipt.

STILL OPEN: no ADR names the MsgID 1 owner, and four other paths can write it while SAT is enabled. Ordered by how easily they fire on a stock config:

1. THERMOSTAT-TIMEOUT SETBACK (leads; fires with no external actor). settings.otd.bFailSafe defaults true, iSetbackTimeout defaults 30 s (OTDirecttypes.h:110,113), gateway is the default mode, and setOverride(1, 16 C) at OTDirect.ino:2470-2475 is ungated. A quiet thermostat while SAT is enabled hands TSet to a 16 C setback.
2. CS= HAS NO SAT GATE (OTDirect.ino:2620-2641), reachable from MQTT ctrlsetpt and raw command (MQTTstuff.ino:496,501), POST /api/v2/otgw/commands (restAPI.ino:304-331), port 25238 (OTDirect.ino:645-670) and telnet. addCommandToQueue() (OTGW-Core.ino:3550-3561) arbitrates nothing. Needs a deliberate external actor: HA discovery publishes MsgID 1 read-only (MQTTHaDiscovery.cpp:829-830).
3. REFRESH-VS-EXPIRY GAP, narrowed. SAT re-sends CS only on a 0.1 C change or every SAT_CMD_REFRESH_MS = 300000 (SATcontrol.ino:312-345); OTDirect expires the override after OT_CSC2_EXPIRY_MS = 60000 (OTDirect.ino:456, cleared at 2033-2037 -> 2167-2175). otCSLastCommandMs is stamped at only four sites and never by the scheduler, so the expiry really fires while SAT stays silent. The adversarial pass narrowed the window: SAT's CH= carries no expiry timer (OTDirect.ino:2864-2869), so the ADR-150 cold cutoff keeps CH=0 alive and DHW-active is the real exposure. Open sub-question: does otMasterStatusFlags bit0 reach the boiler on pass-through thermostat frames in gateway mode (consumed at OTDirect.ino:733)?
4. SILENT HANDOVER ON SAFETY TRIP (reframed; NOT a two-writer conflict). satDisable() clears state.sat.bActive and leaves settings.sat.bEnabled true (SATcontrol.ino:1853-1873); both trips (4274, 4630) route through it and satControlLoop returns early at 4190. UI, MQTT and REST keep reporting SAT enabled while SAT has stopped sending CS= and handed the boiler back. In gateway mode the re-armed OTDirect code puts no new value on the bus (the heating-curve write sits behind IS_MASTER_MODE() at 2077), so this is a state/reporting divergence, not a competing writer.
5. MASTER-MODE thermostat WRITE_DATA MsgID 1, ungated (OTDirect.ino:2523-2527). Exotic config, listed last.

PLATFORM SPLIT: esp32/OTGW32 and esp32-combo in OT-Direct mode hit all five. esp32-classic (HAS_DIRECT_OT=0, boards.h:169-177) hits only item 2, external CS= versus SAT CS= into the PIC single override.

CAVEAT, single-source and unverified: the gateway.asm CommandExpiry parse (other-projects/otgw-6.6/gateway.asm:1619-1630) suggests the PIC keeps a >= 8 C setpoint alive past 60 s, which would make item 3 OTDirect-specific. Do not build a fix on that without re-reading the skip logic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A single named predicate decides TSet ownership and is keyed on SAT being ENABLED, not merely bActive
- [x] #2 The thermostat-timeout setback path (OTDirect.ino:2470-2475) does not override MsgID 1 while SAT owns TSet, or an ADR records why setback deliberately outranks SAT as a fail-safe
- [x] #3 An external CS= arriving while SAT owns TSet (MQTT ctrlsetpt, MQTT raw command, REST, port 25238, telnet) is handled by one documented, deliberate policy rather than last-write-wins
- [x] #4 SAT CS refresh cadence and OTDirect CS expiry no longer fight; the DHW-active window is closed or documented
- [x] #5 The otMasterStatusFlags bit0 pass-through question (OTDirect.ino:733) is answered before the fix is written
- [x] #6 The safety-trip handover is no longer silent: UI/MQTT/REST stop reporting SAT as controlling once bActive is false
- [x] #7 Master-mode WRITE_DATA MsgID 1 (OTDirect.ino:2523-2527) is covered by the same predicate
- [x] #8 An ADR names the MsgID 1 owner per mode and per board and states what happens to each losing writer
- [x] #9 python build.py green for esp32 and esp32-classic; python evaluate.py --quick shows no new failures
- [ ] #10 Field-validated on OTGW32: SAT enabled with a safety trip forced, TSet does not revert to the heating curve or the thermostat
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC5 ANSWERED (was: does otMasterStatusFlags bit0 reach the boiler on pass-through thermostat frames in gateway mode?).

Evidence: otMasterStatusFlags is read at exactly one site that reaches the bus, OTDirect.ino:733 inside buildStatusRequest(). Every other occurrence (797, 1341, 1353, 1753, 1765, 1787, 1794, 2331-2332, 2830) is an assignment. There is no setOverride(0, ...) anywhere - the only setOverride targets are 71, 1, 16 and the generic 2119 - so a thermostat's own MsgID 0 frame is relayed unmodified.

buildStatusRequest() is used in two places: the setup connectivity probe (861) and the scheduler when a scheduled entry has msgId == 0 (1458). scheduleMasterRequest() is called from OTDirect.ino:2015 under 'if (DUE(timerOTSchedule) && !IS_MONITOR_MODE())', i.e. it runs in GATEWAY mode as well - only monitor mode is excluded.

Conclusion, and it is worse than the narrowed reading in the task description: SAT's CH= does reach the boiler in gateway mode, but only on the gateway's OWN MsgID 0 frames, interleaved with the thermostat's frames carrying the thermostat's own CH bit. So the ADR-150 cold cutoff is not reliably held by CH=0 either; whose bit applies depends on which MsgID 0 the boiler saw last. Finding 3's exposure is therefore not limited to the DHW-active window.

This must inform the fix: TSet ownership cannot be solved by MsgID 1 arbitration alone while two masters keep publishing conflicting MsgID 0 status bits.
<!-- SECTION:NOTES:END -->
